#pragma once

#include <d3d12.h>

#include <agz/d3d12/framegraph/graphData.h>
#include <agz/d3d12/framegraph/resourceBinding.h>
#include <agz/d3d12/framegraph/resourceDesc.h>
#include <agz/d3d12/framegraph/resourceManager.h>
#include <agz/d3d12/sync/resourceReleaser.h>

AGZ_D3D12_FG_BEGIN

using ResourceReleaser = d3d12::ResourceReleaser;

class FrameGraphCompiler
{
public:
    
    struct CompilerTransientResourceNode
    {
        RscDesc desc;
        D3D12_RESOURCE_STATES initialState = {};

        bool clearColor = false;
        ClearColor clearColorValue;

        bool clearDepthStencil = false;
        ClearDepthStencil clearDepthStencilValue;

        DXGI_FORMAT clearFormat = DXGI_FORMAT_UNKNOWN;

        std::pair<bool, D3D12_CLEAR_VALUE> getClearValue() const noexcept;
    };

    struct CompilerExternalResourceNode
    {
        ComPtr<ID3D12Resource> rsc;

        D3D12_RESOURCE_STATES initialState = {};
        D3D12_RESOURCE_STATES finalState   = {};
    };

    using CompilerResourceNode = misc::variant_t<
        CompilerTransientResourceNode,
        CompilerExternalResourceNode>;

    struct CompilerPassNode
    {
        struct RscInPass
        {
            ResourceIndex idx;
            D3D12_RESOURCE_STATES inState = {};

            using ViewDesc = misc::variant_t<std::monostate, SRV, UAV, RTV, DSV>;
            ViewDesc viewDesc;

            int idxInRscUsers = -1;

            using RTDSBinding = FrameGraphPassNode::PassResource::RTDSBinding;

            RTDSBinding rtdsBinding;
        };

        FrameGraphPassFunc passFunc;

        std::vector<RscInPass> rscs;
    };

    ResourceIndex addTransientResource(
        const RscDesc        &rscDesc,
        D3D12_RESOURCE_STATES initialState);

    ResourceIndex addTransientResource(
        const RscDesc        &rscDesc,
        D3D12_RESOURCE_STATES initialState,
        const ClearColor     &clearColorValue,
        DXGI_FORMAT           clearFormat = DXGI_FORMAT_UNKNOWN);

    ResourceIndex addTransientResource(
        const RscDesc           &rscDesc,
        D3D12_RESOURCE_STATES    initialState,
        const ClearDepthStencil &clearDepthStencilValue,
        DXGI_FORMAT              clearFormat = DXGI_FORMAT_UNKNOWN);

    ResourceIndex addExternalResource(
        const ComPtr<ID3D12Resource> rscDesc,
        D3D12_RESOURCE_STATES        initialState,
        D3D12_RESOURCE_STATES        finalState);

    template<typename...Args>
    PassIndex addPass(FrameGraphPassFunc passFunc, Args &&...args);

    FrameGraphData compile(
        ID3D12Device      *device,
        ResourceAllocator &rscAlloc,
        ResourceReleaser  &rscReleaser);

private:

    std::vector<CompilerPassNode>     passes_;
    std::vector<CompilerResourceNode> rscs_;
};

inline std::pair<bool, D3D12_CLEAR_VALUE>
    FrameGraphCompiler::CompilerTransientResourceNode
        ::getClearValue() const noexcept
{
    if(clearColor)
    {
        D3D12_CLEAR_VALUE clearValue;
        clearValue.Format = clearFormat;
        std::memcpy(clearValue.Color, &clearColorValue.r, sizeof(ClearColor));
        return { true, clearValue };
    }

    if(clearDepthStencil)
    {
        D3D12_CLEAR_VALUE clearValue;
        clearValue.Format               = clearFormat;
        clearValue.DepthStencil.Depth   = clearDepthStencilValue.depth;
        clearValue.DepthStencil.Stencil = clearDepthStencilValue.stencil;
        return { true, clearValue };
    }

    return { false, {} };
}

inline ResourceIndex FrameGraphCompiler::addTransientResource(
    const RscDesc &rscDesc, D3D12_RESOURCE_STATES initialState)
{
    CompilerTransientResourceNode newNode;
    newNode.desc         = rscDesc;
    newNode.initialState = initialState;

    const auto idx = static_cast<int32_t>(rscs_.size());
    rscs_.emplace_back(newNode);
    return { idx };
}

inline ResourceIndex FrameGraphCompiler::addTransientResource(
    const RscDesc        &rscDesc,
    D3D12_RESOURCE_STATES initialState,
    const ClearColor     &clearColorValue,
    DXGI_FORMAT           clearFormat)
{
    CompilerTransientResourceNode newNode;
    newNode.desc            = rscDesc;
    newNode.initialState    = initialState;
    newNode.clearColor      = true;
    newNode.clearColorValue = clearColorValue;
    
    if(clearFormat == DXGI_FORMAT_UNKNOWN)
        newNode.clearFormat = rscDesc.desc.Format;
    else
        newNode.clearFormat = clearFormat;

    const auto idx = static_cast<int32_t>(rscs_.size());
    rscs_.emplace_back(newNode);
    return { idx };
}

inline ResourceIndex FrameGraphCompiler::addTransientResource(
    const RscDesc           &rscDesc,
    D3D12_RESOURCE_STATES    initialState,
    const ClearDepthStencil &clearDepthStencilValue,
    DXGI_FORMAT              clearFormat)
{
    CompilerTransientResourceNode newNode;
    newNode.desc                   = rscDesc;
    newNode.initialState           = initialState;
    newNode.clearDepthStencil      = true;
    newNode.clearDepthStencilValue = clearDepthStencilValue;

    if(clearFormat == DXGI_FORMAT_UNKNOWN)
        newNode.clearFormat = rscDesc.desc.Format;
    else
        newNode.clearFormat = clearFormat;

    const auto idx = static_cast<int32_t>(rscs_.size());
    rscs_.emplace_back(newNode);
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

    const auto idx = static_cast<int32_t>(rscs_.size());
    rscs_.emplace_back(newNode);
    return { idx };
}

namespace detail
{

    inline void _initCompilerRP(
        FrameGraphCompiler::CompilerPassNode &passNode,
        const SRV &srv)
    {
        FrameGraphCompiler::CompilerPassNode::RscInPass rsc;
        rsc.idx      = srv.rsc;
        rsc.inState  = srv.getRequiredRscState();
        rsc.viewDesc = srv;
        passNode.rscs.push_back(rsc);
    }

    inline void _initCompilerRP(
        FrameGraphCompiler::CompilerPassNode &passNode,
        const UAV &uav)
    {
        FrameGraphCompiler::CompilerPassNode::RscInPass rsc;
        rsc.idx      = uav.rsc;
        rsc.inState  = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        rsc.viewDesc = uav;
        passNode.rscs.push_back(rsc);
    }

    inline void _initCompilerRP(
        FrameGraphCompiler::CompilerPassNode &passNode,
        const RTV &rtv)
    {
        FrameGraphCompiler::CompilerPassNode::RscInPass rsc;
        rsc.idx      = rtv.rsc;
        rsc.inState  = D3D12_RESOURCE_STATE_RENDER_TARGET;
        rsc.viewDesc = rtv;
        passNode.rscs.push_back(rsc);
    }

    inline void _initCompilerRP(
        FrameGraphCompiler::CompilerPassNode &passNode,
        const DSV &dsv)
    {
        FrameGraphCompiler::CompilerPassNode::RscInPass rsc;
        rsc.idx      = dsv.rsc;
        rsc.inState  = D3D12_RESOURCE_STATE_DEPTH_WRITE;
        rsc.viewDesc = dsv;
        passNode.rscs.push_back(rsc);
    }

    inline void _initCompilerRP(
        FrameGraphCompiler::CompilerPassNode &passNode,
        const RenderTargetBinding &rtb)
    {
        FrameGraphCompiler::CompilerPassNode::RscInPass rsc;
        rsc.idx      = rtb.rtv.rsc;
        rsc.inState  = D3D12_RESOURCE_STATE_RENDER_TARGET;
        rsc.viewDesc = rtb.rtv;

        rsc.rtdsBinding = FrameGraphPassNode::PassResource::RTB
            { rtb.clearColor, rtb.clearColorValue };

        passNode.rscs.push_back(rsc);
    }

    inline void _initCompilerRP(
        FrameGraphCompiler::CompilerPassNode &passNode,
        const DepthStencilBinding &dsb)
    {
        FrameGraphCompiler::CompilerPassNode::RscInPass rsc;
        rsc.idx      = dsb.dsv.rsc;
        rsc.inState  = D3D12_RESOURCE_STATE_DEPTH_WRITE; // IMPROVE: optimize for read-only usage
        rsc.viewDesc = dsb.dsv;

        rsc.rtdsBinding = FrameGraphPassNode::PassResource::DSB
            { dsb.clearDepth, dsb.clearStencil, dsb.clearDepthStencilValue };

        passNode.rscs.push_back(rsc);
    }

} // namespace detail

template<typename ... Args>
PassIndex FrameGraphCompiler::addPass(
    FrameGraphPassFunc passFunc, Args &&... args)
{
    const auto idx = static_cast<int32_t>(passes_.size());
    passes_.emplace_back();
    auto &newPass = passes_.back();

    newPass.passFunc = std::move(passFunc);

    InvokeAll([&]
    {
        detail::_initCompilerRP(newPass, std::forward<Args>(args));
    }...);

    return { idx };
}

inline FrameGraphData FrameGraphCompiler::compile(
    ID3D12Device      *device,
    ResourceAllocator &rscAlloc,
    ResourceReleaser  &rscReleaser)
{
    FrameGraphData ret;
    ret.rscNodes.reserve(rscs_.size());
    ret.passNodes.reserve(passes_.size());

    struct TempRscNode
    {
        D3D12_RESOURCE_STATES actualInitialState = {};
        std::vector<std::pair<PassIndex, D3D12_RESOURCE_STATES>> users;
    };

    std::vector<TempRscNode> tempRscNodes(rscs_.size());

    // collect tempRscNode.users
    // count cpu/gpu descriptors

    for(size_t i = 0; i < passes_.size(); ++i)
    {
        const PassIndex passIdx = { static_cast<int32_t>(i) };
        auto &pass = passes_[i];

        for(auto &rscUsage : pass.rscs)
        {
            auto &tempRsc = tempRscNodes[rscUsage.idx.idx];

            const int idxInRscUsers = static_cast<int>(tempRsc.users.size());
            rscUsage.idxInRscUsers = idxInRscUsers;

            tempRsc.users.push_back({ passIdx, rscUsage.inState });

            match_variant(rscUsage.viewDesc,
                [&](const SRV &) { ++ret.gpuDescCount; },
                [&](const UAV &) { ++ret.gpuDescCount; },
                [&](const RTV &) { ++ret.rtvDescCount; },
                [&](const DSV &) { ++ret.dsvDescCount; },
                [&](const std::monostate &) {});
        }
    }

    // allocate d3d rsc & record actual initial state

    for(size_t i = 0; i < rscs_.size(); ++i)
    {
        const auto &rsc     = rscs_[i];
        auto       &tempRsc = tempRscNodes[i];

        ComPtr<ID3D12Resource> d3dRsc;

        match_variant(rsc,
            [&](const CompilerTransientResourceNode &tn)
        {
            const auto [clear, clearValue] = tn.getClearValue();

            d3dRsc = rscAlloc.allocResource(
                { tn.desc.desc, clear, clearValue },
                tn.initialState, tempRsc.actualInitialState);

            rscReleaser.add(rscAlloc, d3dRsc, tempRsc.users.back().second);
        },
            [&](const CompilerExternalResourceNode &en)
        {
            d3dRsc                     = en.rsc;
            tempRsc.actualInitialState = en.initialState;
        });

        ret.rscNodes.emplace_back(
            rsc.is<CompilerExternalResourceNode>(), d3dRsc);
    }

    // allocate cpu/gpu desc range

    DescriptorIndex nextRTVDescIdx = 0;
    DescriptorIndex nextDSVDescIdx = 0;
    DescriptorIndex nextGPUDescIdx = 0;

    // fill fg pass nodes

    for(size_t i = 0; i < passes_.size(); ++i)
    {
        auto &pass = passes_[i];

        std::map<ResourceIndex, FrameGraphPassNode::PassResource> passRscs;
        for(auto &rscUsage : pass.rscs)
        {
            FrameGraphPassNode::PassResource passRsc;

            const auto &passNode = rscs_[rscUsage.idx.idx];
            const auto &tempRsc  = tempRscNodes[rscUsage.idx.idx];

            // d3d rsc

            passRsc.rscIdx = rscUsage.idx;
            
            // state transitions

            // IMPROVE: elim unnecessary rsc transitions
            passRsc.doInitialTransition = true;

            if(rscUsage.idxInRscUsers > 0)
            {
                passRsc.beforeState =
                    tempRsc.users[rscUsage.idxInRscUsers - 1].second;
            }
            else
                passRsc.beforeState = tempRsc.actualInitialState;

            passRsc.inState = rscUsage.inState;

            if(rscUsage.idxInRscUsers + 1 ==
                    static_cast<int>(tempRsc.users.size()))
            {
                const auto extNode = passNode.as_if<
                    CompilerExternalResourceNode>();
                if(extNode && extNode->finalState != rscUsage.inState)
                {
                    passRsc.doFinalTransition = true;
                    passRsc.afterState        = extNode->finalState;
                }
                else
                {
                    passRsc.doFinalTransition = false;
                    passRsc.afterState        = rscUsage.inState;
                }
            }
            else
            {
                passRsc.doFinalTransition = false;
                passRsc.afterState        = rscUsage.inState;
            }

            // fill descriptor format

            auto fillFmt = [&](DXGI_FORMAT &fmt)
            {
                if(fmt == DXGI_FORMAT_UNKNOWN)
                {
                    fmt = ret.rscNodes[passRsc.rscIdx.idx]
                                .getD3DResource()->GetDesc().Format;
                }
            };

            match_variant(
                rscUsage.viewDesc,
                [&](SRV &view) { fillFmt(view.desc.Format); },
                [&](UAV &view) { fillFmt(view.desc.Format); },
                [&](RTV &view) { fillFmt(view.desc.Format); },
                [&](DSV &view) { fillFmt(view.desc.Format); },
                [&](const std::monostate &) { });

            // assign descriptor

            passRsc.viewDesc = rscUsage.viewDesc;

            match_variant(rscUsage.viewDesc,
                [&](const SRV &)
            {
                passRsc.descIdx = nextGPUDescIdx++;
            },
                [&](const UAV &)
            {
                passRsc.descIdx = nextGPUDescIdx++;
            },
                [&](const RTV &)
            {
                passRsc.descIdx = nextRTVDescIdx++;
            },
                [&](const DSV &)
            {
                passRsc.descIdx = nextDSVDescIdx++;
            },
                [&](const std::monostate &) { }
            );

            passRsc.rtdsBinding = rscUsage.rtdsBinding;

            passRscs[rscUsage.idx] = passRsc;
        }

        ret.passNodes.emplace_back(std::move(passRscs), pass.passFunc);
    }

    return ret;
}

AGZ_D3D12_FG_END
