#pragma once

#include <agz/utility/misc.h>

AGZ_D3D12_FG_BEGIN

namespace detail
{

    inline void _initRPNode(
        CompilerPassNode &node, const SRV &srv)
    {
        node.gpuViews.emplace_back(srv);
    }

    inline void _initRPNode(
        CompilerPassNode &node, const UAV &uav)
    {
        node.gpuViews.emplace_back(uav);
    }

    inline void _initRPNode(
        CompilerPassNode &node, const RootSignature &rootSignature)
    {
        node.rootSignature = rootSignature;
    }

    inline void _initRPNode(
        CompilerPassNode &node, ComPtr<ID3D12RootSignature> rootSignature)
    {
        node.cachedRootSignature.Swap(rootSignature);
    }

    inline void _initRPNode(
        CompilerPassNode &node, ComPtr<ID3D12PipelineState> pipelineState)
    {
        node.cachedPipelineState.Swap(pipelineState);
    }

    inline void _initRPNode(
        CompilerPassNode &node, const RenderTargetBinding &rtb)
    {
        node.renderTargetBindings.push_back(rtb);
    }

    inline void _initRPNode(
        CompilerPassNode &node, const DepthStencilBinding &dsb)
    {
        node.depthStencilBinding = dsb;
    }

    inline void _initRPNode(
        CompilerPassNode &node, const ResourceWithState &rws)
    {
        assert(node.rsc2State.find(rws.rsc) == node.rsc2State.end());
        node.rsc2State[rws.rsc] = rws.state;
    }

} // namespace detail

inline ResourceIndex FrameGraphCompiler::addTransientResource(
    const RscDesc &rscDesc,
    D3D12_RESOURCE_STATES initialState)
{
    CompilerTransientResourceNode newNode;
    newNode.desc         = rscDesc;
    newNode.initialState = initialState;

    const auto idx = static_cast<int32_t>(rscNodes_.size());
    rscNodes_.emplace_back(newNode);
    return { idx };
}

inline ResourceIndex FrameGraphCompiler::addExternalResource(
    ComPtr<ID3D12Resource> rsc,
    D3D12_RESOURCE_STATES  initialState,
    D3D12_RESOURCE_STATES  finalState)
{
    CompilerExternalResourceNode newNode;
    newNode.rsc          = rsc;
    newNode.initialState = initialState;
    newNode.finalState   = finalState;

    const auto idx = static_cast<int32_t>(rscNodes_.size());
    rscNodes_.emplace_back(newNode);
    return { idx };
}

template<typename Func, typename ... Args>
PassIndex FrameGraphCompiler::addRenderPass(
    Func &&passFunc, Args &&... args)
{
    const PassIndex ret = { static_cast<int32_t>(passNodes_.size()) };
    passNodes_.emplace_back();
    auto &node = passNodes_.back();

    node.passFunc = std::forward<Func>(passFunc);

    InvokeAll([&] { detail::_initRPNode(node, std::forward<Args>(args)); }...);

    return ret;
}

inline std::vector<FrameGraphPassNode> FrameGraphCompiler::compile(
    ResourceAllocator &rscAlloc)
{
    std::vector<PassTempNode> passTempNodes(passNodes_.size());
    std::vector<RscTempNode>  rscTempNodes(rscNodes_.size());

    // collect rscTempNode.users and passTempNode.rscs

    for(size_t i = 0; i < passNodes_.size(); ++i)
    {
        collectUsedRscsForSinglePass(
            rscTempNodes, { static_cast<int32_t>(i) },
            passNodes_[i], passTempNodes[i]);
    }

    // init rscTmpNode.d3dRscs & rscTmpNode.actualInitialState

    initD3DRscsAndInitialState(rscAlloc, rscTempNodes);

    // init pass nodes

    for(size_t i = 0; i < passNodes_.size(); ++i)
    {
        auto &p  = passNodes_[i];
        auto &pT = passTempNodes[i];

        std::map<ResourceIndex, FrameGraphPassNode::PassResource> passRscs;

        // insert into passRscs and setup state transition

        for(auto &it : pT.rscs)
        {
            const ResourceIndex rscIdx = it.first;
            auto &passRsc = passRscs[rscIdx];
            auto &r       = rscNodes_[rscIdx.idx];
            auto &rT      = rscTempNodes[rscIdx.idx];
            auto &rscInPT = pT.rscs[rscIdx];

            passRsc.rsc = rT.d3dRsc;

            if(rscInPT.idxInRscUsers != 0)
            {
                const auto lastUsingPassIdx = rT.users[rscInPT.idxInRscUsers - 1];
                auto &lastUsingPass = passTempNodes[lastUsingPassIdx.idx];
                passRsc.beforeState = lastUsingPass.rscs[rscIdx].expectedState;
            }
            else
                passRsc.beforeState = rT.actualInitialState;

            passRsc.afterState = rscInPT.expectedState;

            if(rscInPT.idxInRscUsers == rT.users.size() - 1)
            {
                if(auto en = r.as_if<CompilerExternalResourceNode>())
                    passRsc.finalState = en->finalState;
            }
            else
                passRsc.finalState = passRsc.afterState;

            // IMPROVE: optimize unnecessary transitions
            passRsc.doInitialTransition = true;
            passRsc.doFinalTransition   = true;
        }

        // root parameter binding & views

        if(p.rootSignature)
        {
            for(size_t j = 0; j < p.rootSignature->rootParameters.size(); ++j)
            {
                auto &rp = p.rootSignature->rootParameters[j];

                auto dt = rp.as_if<DescriptorTable>();
                if(!dt)
                    continue;
                
                for(auto &range : dt->ranges)
                {
                    match_variant(range,
                        [&](const SRVRange &srvr)
                    {
                        if(srvr.singleSRV.rsc.isNil())
                            return;

                        auto &passRsc = passRscs[srvr.singleSRV.rsc];
                        passRsc.rootSignatureParamType =
                            FrameGraphPassNode::PassResource::DescriptorTable;
                        passRsc.rootSignatureParamIndex = static_cast<UINT>(j);

                        passRsc.viewDesc = srvr.singleSRV;
                    },
                        [&](const UAVRange &uavr)
                    {
                        if(uavr.singleUAV.rsc.isNil())
                            return;

                        auto &passRsc = passRscs[uavr.singleUAV.rsc];
                        passRsc.rootSignatureParamType =
                            FrameGraphPassNode::PassResource::DescriptorTable;
                        passRsc.rootSignatureParamIndex = static_cast<UINT>(j);

                        passRsc.viewDesc = uavr.singleUAV;
                    },
                        [](const auto &) {});
                }
            }
        }

        // render target bindings


    }

    // TODO
    return {};
}

inline void FrameGraphCompiler::collectUsedRscsForSinglePass(
    std::vector<RscTempNode> &rscTempNodes,
    PassIndex                 passIndex,
    const CompilerPassNode   &p,
    PassTempNode             &pT) const
{
    auto addRsc = [&](ResourceIndex rsc, D3D12_RESOURCE_STATES states)
    {
        if(pT.rscs.find(rsc) != pT.rscs.end())
            throw D3D12LabException("repeated resource state decleration");
        
        const size_t idxInRscUsers = rscTempNodes[rsc.idx].users.size();
        rscTempNodes[rsc.idx].users.push_back(passIndex);

        pT.rscs.insert({ rsc, { states, idxInRscUsers } });
    };

    for(auto &it : p.rsc2State)
        addRsc(it.first, it.second);

    for(auto &v : p.gpuViews)
    {
        match_variant(v,
            [&](const SRV &srv)
        {
            addRsc(
                srv.rsc,
                srv.getRequiredRscState());
        },
            [&](const UAV &uav)
        {
            addRsc(uav.rsc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        });
    }

    for(auto &rt : p.renderTargetBindings)
    {
        if(rt.rsc)
            addRsc(*rt.rsc, D3D12_RESOURCE_STATE_RENDER_TARGET);
        else if(rt.rtv)
            addRsc(rt.rtv->rsc, D3D12_RESOURCE_STATE_RENDER_TARGET);
        else
        {
            throw D3D12LabException(
                "both rsc & rtv of a render target binding "
                "are non-nil");
        }
    }

    if(p.depthStencilBinding)
    {
        if(p.depthStencilBinding->rsc)
            addRsc(*p.depthStencilBinding->rsc, D3D12_RESOURCE_STATE_DEPTH_WRITE);
        else if(p.depthStencilBinding->dsv)
            addRsc(*p.depthStencilBinding->rsc, D3D12_RESOURCE_STATE_DEPTH_WRITE);
        else
        {
            throw D3D12LabException(
                "both rsc & rtv of a depth stencil binding "
                "are non-nil");
        }
    }

    if(p.rootSignature)
    {
        for(auto &param : p.rootSignature->rootParameters)
        {
            match_variant(param,
                [&](const DescriptorTable &dt)
            {
                for(auto &range : dt.ranges)
                {
                    match_variant(range,
                        [&](const SRVRange &srvr)
                    {
                        if(!srvr.singleSRV.rsc.isNil())
                        {
                            addRsc(
                                srvr.singleSRV.rsc,
                                srvr.singleSRV.getRequiredRscState());
                        }
                    },
                        [&](const UAVRange &uavr)
                    {
                        if(!uavr.singleUAV.rsc.isNil())
                        {
                            addRsc(
                                uavr.singleUAV.rsc,
                                D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
                        }
                    },
                        [](const auto &) {});
                }
            },
                [](const auto &) {});
        }
    }
}

inline void FrameGraphCompiler::initD3DRscsAndInitialState(
    ResourceAllocator        &rscAlloc,
    std::vector<RscTempNode> &rscTempNodes) const
{
    for(size_t i = 0; i < rscNodes_.size(); ++i)
    {
        auto &r  = rscNodes_[i];
        auto &rT = rscTempNodes[i];

        match_variant(r,
            [&](const CompilerTransientResourceNode &tn)
        {
            const auto [clear, clearValue] = tn.getClearValue();

            rT.d3dRsc = rscAlloc.allocResource(
                { tn.desc.desc, clear, clearValue },
                tn.initialState, rT.actualInitialState);
        },
            [&](const CompilerExternalResourceNode &en)
        {
            rT.d3dRsc = en.rsc;
            rT.actualInitialState = en.initialState;
        });
    }
}

AGZ_D3D12_FG_END
