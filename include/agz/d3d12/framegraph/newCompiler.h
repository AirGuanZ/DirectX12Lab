#pragma once

#include <d3d12.h>

#include <agz/d3d12/framegraph/graph.h>
#include <agz/d3d12/framegraph/resourceDesc.h>
#include <agz/d3d12/framegraph/resourceManager.h>

AGZ_D3D12_FG_BEGIN

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
        const ClearColor     &clearColorValue);

    ResourceIndex addTransientResource(
        const RscDesc           &rscDesc,
        D3D12_RESOURCE_STATES    initialState,
        const ClearDepthStencil &clearDepthStencilValue);

    ResourceIndex addExternalResource(
        const ComPtr<ID3D12Resource> rscDesc,
        D3D12_RESOURCE_STATES        initialState,
        D3D12_RESOURCE_STATES        finalState);

    template<typename Func, typename...Args>
    PassIndex addPass(Func &&passFunc, Args &&...args);

    std::vector<FrameGraphPassNode> compile(
        ID3D12Device                                 *device,
        ResourceAllocator                            &rscAlloc,
        const std::function<DescriptorRange(size_t)> &cpuDescAllocator,
        const std::function<DescriptorRange(size_t)> &gpuDescAllocator);

private:

    std::vector<CompilerPassNode>     passes_;
    std::vector<CompilerResourceNode> rscs_;
};

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
    const ClearColor     &clearColorValue)
{
    CompilerTransientResourceNode newNode;
    newNode.desc            = rscDesc;
    newNode.initialState    = initialState;
    newNode.clearColor      = true;
    newNode.clearColorValue = clearColorValue;

    const auto idx = static_cast<int32_t>(rscs_.size());
    rscs_.emplace_back(newNode);
    return { idx };
}

inline ResourceIndex FrameGraphCompiler::addTransientResource(
    const RscDesc           &rscDesc,
    D3D12_RESOURCE_STATES    initialState,
    const ClearDepthStencil &clearDepthStencilValue)
{
    CompilerTransientResourceNode newNode;
    newNode.desc                   = rscDesc;
    newNode.initialState           = initialState;
    newNode.clearDepthStencil      = true;
    newNode.clearDepthStencilValue = clearDepthStencilValue;

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

} // namespace detail

template<typename Func, typename ... Args>
PassIndex FrameGraphCompiler::addPass(
    Func &&passFunc, Args &&... args)
{
    const auto idx = static_cast<int32_t>(passes_.size());
    passes_.emplace_back();
    auto &newPass = passes_.back();

    newPass.passFunc = std::forward<Func>(passFunc);

    InvokeAll([&]
    {
        detail::_initCompilerRP(newPass, std::forward<Args>(args));
    }...);

    return { idx };
}

inline std::vector<FrameGraphPassNode> FrameGraphCompiler::compile(
    ID3D12Device                                 *device,
    ResourceAllocator                            &rscAlloc,
    const std::function<DescriptorRange(size_t)> &cpuDescAllocator,
    const std::function<DescriptorRange(size_t)> &gpuDescAllocator)
{
    struct TempRscNode
    {
        ComPtr<ID3D12Resource> rsc;

        D3D12_RESOURCE_STATES actualInitialState = {};

        std::vector<std::pair<PassIndex, D3D12_RESOURCE_STATES>> users;
    };

    std::vector<TempRscNode> tempRscNodes(rscs_.size());

    // collect tempRscNode.users
    // count cpu/gpu descriptors

    size_t cpuDescCount = 0;
    size_t gpuDescCount = 0;

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
                [&](const SRV &) { ++gpuDescCount; },
                [&](const UAV &) { ++gpuDescCount; },
                [&](const RTV &) { ++cpuDescCount; },
                [&](const DSV &) { ++cpuDescCount; },
                [&](const std::monostate &) {});
        }
    }

    // allocate d3d rsc & record actual initial state

    for(size_t i = 0; i < rscs_.size(); ++i)
    {
        const auto &rsc     = rscs_[i];
        auto       &tempRsc = tempRscNodes[i];

        match_variant(rsc,
            [&](const CompilerTransientResourceNode &tn)
        {
            const auto [clear, clearValue] = tn.getClearValue();

            tempRsc.rsc = rscAlloc.allocResource(
                { tn.desc.desc, clear, clearValue },
                tn.initialState, tempRsc.actualInitialState);
        },
            [&](const CompilerExternalResourceNode &en)
        {
            tempRsc.rsc                = en.rsc;
            tempRsc.actualInitialState = en.initialState;
        });
    }

    // allocate cpu/gpu desc range

    auto cpuDescs = cpuDescAllocator(cpuDescCount);
    auto gpuDescs = gpuDescAllocator(gpuDescCount);

    size_t nextCPUDescIdx = 0;
    size_t nextGPUDescIdx = 0;

    // fill fg pass nodes

    std::vector<FrameGraphPassNode> ret;
    ret.reserve(passes_.size());

    for(size_t i = 0; i < passes_.size(); ++i)
    {
        const auto &pass = passes_[i];

        std::map<ResourceIndex, FrameGraphPassNode::PassResource> passRscs;
        for(auto &rscUsage : pass.rscs)
        {
            FrameGraphPassNode::PassResource passRsc;

            const auto &passNode = rscs_[rscUsage.idx.idx];
            const auto &tempRsc  = tempRscNodes[rscUsage.idx.idx];

            // d3d rsc

            passRsc.rsc = tempRsc.rsc;

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
                    passRsc.finalState        = extNode->finalState;
                }
                else
                {
                    passRsc.doFinalTransition = false;
                    passRsc.finalState        = rscUsage.inState;
                }
            }
            else
            {
                passRsc.doFinalTransition = false;
                passRsc.finalState        = rscUsage.inState;
            }

            // fill descriptor format

            match_variant(
                rscUsage.viewDesc,
                [&](const std::monostate &) { },
                [&](auto &view)
            {
                if(view.desc.Format == DXGI_FORMAT_UNKNOWN)
                    view.desc.Format = passRsc.rsc->GetDesc().Format;
            });

            // create descriptor

            match_variant(rscUsage.viewDesc,
                [&](const SRV &srv)
            {
                passRsc.descriptor = gpuDescs[nextGPUDescIdx++];

                device->CreateShaderResourceView(
                    passRsc.rsc.Get(), &srv.desc,
                    passRsc.descriptor.getCPUHandle());
            },
                [&](const UAV &uav)
            {
                passRsc.descriptor = gpuDescs[nextGPUDescIdx++];

                device->CreateUnorderedAccessView(
                    passRsc.rsc.Get(), nullptr, &uav.desc,
                    passRsc.descriptor.getCPUHandle());
            },
                [&](const RTV &rtv)
            {
                passRsc.descriptor = cpuDescs[nextCPUDescIdx++];

                device->CreateRenderTargetView(
                    passRsc.rsc.Get(), &rtv.desc,
                    passRsc.descriptor.getCPUHandle());
            },
                [&](const DSV &dsv)
            {
                passRsc.descriptor = cpuDescs[nextCPUDescIdx];

                device->CreateDepthStencilView(
                    passRsc.rsc.Get(), &dsv.desc,
                    passRsc.descriptor.getCPUHandle());
            },
                [&](const std::monostate &) { }
            );

            passRscs[rscUsage.idx] = passRsc;
        }

        ret.emplace_back(std::move(passRscs), pass.passFunc);
    }

    return ret;
}

AGZ_D3D12_FG_END
