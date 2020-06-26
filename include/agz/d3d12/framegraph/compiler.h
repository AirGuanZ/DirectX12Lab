#pragma once

#include <d3d12.h>

#include <agz/d3d12/framegraph/graphData.h>
#include <agz/d3d12/framegraph/resourceDesc.h>
#include <agz/d3d12/framegraph/resourceAllocator.h>
#include <agz/d3d12/framegraph/resourceReleaser.h>
#include <agz/d3d12/framegraph/RTDSBinding.h>
#include <agz/d3d12/framegraph/viewport.h>

AGZ_D3D12_FG_BEGIN

class FrameGraphCompiler : public misc::uncopyable_t
{
public:
    
    struct CompilerInternalResourceNode
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
        CompilerInternalResourceNode,
        CompilerExternalResourceNode>;

    struct CompilerPassNode
    {
        struct RscInPass
        {
            ResourceIndex idx;
            D3D12_RESOURCE_STATES inState = {};

            using ViewDesc = misc::variant_t<
                std::monostate,
                _internalSRV,
                _internalUAV,
                _internalRTV,
                _internalDSV>;
            ViewDesc viewDesc;

            int idxInRscUsers = -1;

            using RTDSBinding = FrameGraphPassNode::PassResource::RTDSBinding;

            RTDSBinding rtdsBinding;
        };

        bool isGraphics;

        FrameGraphPassFunc passFunc;

        std::vector<RscInPass> rscs;

        bool defaultViewport = true;
        std::vector<D3D12_VIEWPORT> viewports;

        bool defaultScissor = true;
        std::vector<D3D12_RECT> scissors;

        ComPtr<ID3D12RootSignature> rootSignature;
        ComPtr<ID3D12PipelineState> pipelineState;
    };

    ResourceIndex addInternalResource(
        const RscDesc        &rscDesc,
        D3D12_RESOURCE_STATES initialState);

    ResourceIndex addInternalResource(
        const RscDesc        &rscDesc,
        D3D12_RESOURCE_STATES initialState,
        const ClearColor     &clearColorValue,
        DXGI_FORMAT           clearFormat = DXGI_FORMAT_UNKNOWN);

    ResourceIndex addInternalResource(
        const RscDesc           &rscDesc,
        D3D12_RESOURCE_STATES    initialState,
        const ClearDepthStencil &clearDepthStencilValue,
        DXGI_FORMAT              clearFormat = DXGI_FORMAT_UNKNOWN);

    ResourceIndex addExternalResource(
        const ComPtr<ID3D12Resource> rscDesc,
        D3D12_RESOURCE_STATES        initialState,
        D3D12_RESOURCE_STATES        finalState);

    template<typename...Args>
    PassIndex addGraphicsPass(FrameGraphPassFunc passFunc, Args &&...args);

    template<typename...Args>
    PassIndex addComputePass(FrameGraphPassFunc passFunc, Args &&...args);

    FrameGraphData compile(
        ID3D12Device      *device,
        ResourceAllocator &rscAlloc,
        ResourceReleaser  &rscReleaser);

private:

    std::vector<CompilerPassNode>     passes_;
    std::vector<CompilerResourceNode> rscs_;
};

namespace detail
{

    inline void _initCompilerRP(
        FrameGraphCompiler::CompilerPassNode &passNode,
        const _internalSRV &srv)
    {
        FrameGraphCompiler::CompilerPassNode::RscInPass rsc;
        rsc.idx      = srv.rsc;
        rsc.inState  = srv.getRequiredRscState();
        rsc.viewDesc = srv;
        passNode.rscs.push_back(rsc);
    }

    inline void _initCompilerRP(
        FrameGraphCompiler::CompilerPassNode &passNode,
        const _internalUAV &uav)
    {
        FrameGraphCompiler::CompilerPassNode::RscInPass rsc;
        rsc.idx      = uav.rsc;
        rsc.inState  = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        rsc.viewDesc = uav;
        passNode.rscs.push_back(rsc);
    }

    inline void _initCompilerRP(
        FrameGraphCompiler::CompilerPassNode &passNode,
        const _internalRTV &rtv)
    {
        FrameGraphCompiler::CompilerPassNode::RscInPass rsc;
        rsc.idx      = rtv.rsc;
        rsc.inState  = D3D12_RESOURCE_STATE_RENDER_TARGET;
        rsc.viewDesc = rtv;
        passNode.rscs.push_back(rsc);
    }

    inline void _initCompilerRP(
        FrameGraphCompiler::CompilerPassNode &passNode,
        const _internalDSV &dsv)
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

    inline void _initCompilerRP(
        FrameGraphCompiler::CompilerPassNode &passNode,
        const _internalNoViewport &)
    {
        passNode.defaultViewport = false;
    }

    inline void _initCompilerRP(
        FrameGraphCompiler::CompilerPassNode &passNode,
        const Viewport &viewport)
    {
        passNode.defaultViewport = false;
        passNode.viewports.push_back(viewport);
    }

    inline void _initCompilerRP(
        FrameGraphCompiler::CompilerPassNode &passNode,
        const _internalNoScissor &)
    {
        passNode.defaultScissor = false;
    }

    inline void _initCompilerRP(
        FrameGraphCompiler::CompilerPassNode &passNode,
        const Scissor &scissor)
    {
        passNode.defaultScissor = false;
        passNode.scissors.push_back(scissor);
    }

    inline void _initCompilerRP(
        FrameGraphCompiler::CompilerPassNode &passNode,
        ComPtr<ID3D12PipelineState> pipelineState)
    {
        passNode.pipelineState = std::move(pipelineState);
    }

    inline void _initCompilerRP(
        FrameGraphCompiler::CompilerPassNode &passNode,
        ComPtr<ID3D12RootSignature> rootSignature)
    {
        passNode.rootSignature = std::move(rootSignature);
    }

    inline void _initCompilerCP(
        FrameGraphCompiler::CompilerPassNode &passNode,
        const _internalSRV &srv)
    {
        FrameGraphCompiler::CompilerPassNode::RscInPass rsc;
        rsc.idx      = srv.rsc;
        rsc.inState  = srv.getRequiredRscState();
        rsc.viewDesc = srv;
        passNode.rscs.push_back(rsc);
    }

    inline void _initCompilerCP(
        FrameGraphCompiler::CompilerPassNode &passNode,
        const _internalUAV &uav)
    {
        FrameGraphCompiler::CompilerPassNode::RscInPass rsc;
        rsc.idx      = uav.rsc;
        rsc.inState  = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        rsc.viewDesc = uav;
        passNode.rscs.push_back(rsc);
    }
    
    inline void _initCompilerCP(
        FrameGraphCompiler::CompilerPassNode &passNode,
        ComPtr<ID3D12PipelineState> pipelineState)
    {
        passNode.pipelineState = std::move(pipelineState);
    }

    inline void _initCompilerCP(
        FrameGraphCompiler::CompilerPassNode &passNode,
        ComPtr<ID3D12RootSignature> rootSignature)
    {
        passNode.rootSignature = std::move(rootSignature);
    }

} // namespace detail

template<typename ... Args>
PassIndex FrameGraphCompiler::addGraphicsPass(
    FrameGraphPassFunc passFunc, Args &&... args)
{
    const auto idx = static_cast<int32_t>(passes_.size());
    passes_.emplace_back();
    auto &newPass = passes_.back();

    newPass.isGraphics = true;
    newPass.passFunc   = std::move(passFunc);

    InvokeAll([&]
    {
        detail::_initCompilerRP(newPass, std::forward<Args>(args));
    }...);

    return { idx };
}

template<typename ... Args>
PassIndex FrameGraphCompiler::addComputePass(
    FrameGraphPassFunc passFunc, Args &&... args)
{
    const auto idx = static_cast<int32_t>(passes_.size());
    passes_.emplace_back();
    auto &newPass = passes_.back();

    newPass.isGraphics = false;
    newPass.passFunc   = std::move(passFunc);

    InvokeAll([&]
    {
        detail::_initCompilerCP(newPass, std::forward<Args>(args));
    }...);

    return { idx };
}

AGZ_D3D12_FG_END
