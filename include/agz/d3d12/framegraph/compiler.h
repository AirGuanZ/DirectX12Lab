#pragma once

#include <map>

#include <d3d12.h>

#include <agz/d3d12/framegraph/resourceBinding.h>
#include <agz/d3d12/framegraph/resourceDesc.h>
#include <agz/d3d12/framegraph/resourceManager.h>
#include <agz/d3d12/framegraph/rootSignature.h>

AGZ_D3D12_FG_BEGIN

struct ResourceWithState
{
    ResourceIndex rsc;
    D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;
};

struct CompilerTransientResourceNode
{
    RscDesc desc;
    D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON;

    bool clearColor = false;
    ClearColor clearColorValue;

    bool clearDepth = false;
    ClearDepthStencil clearDepthStencil;

    std::pair<bool, D3D12_CLEAR_VALUE> getClearValue() const noexcept;
};

struct CompilerExternalResourceNode
{
    ComPtr<ID3D12Resource> rsc;

    D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON;
    D3D12_RESOURCE_STATES finalState   = D3D12_RESOURCE_STATE_COMMON;
};

using CompilerResourceNode = misc::variant_t<
    CompilerTransientResourceNode,
    CompilerExternalResourceNode>;

struct CompilerPassNode
{
    // func

    FrameGraphPassFunc passFunc;

    // state

    std::map<ResourceIndex, D3D12_RESOURCE_STATES> rsc2State;

    // srv & uav

    using GPUView = misc::variant_t<SRV, UAV>;
    std::vector<GPUView> gpuViews;

    // render targets & depth stencil

    std::vector<RenderTargetBinding>   renderTargetBindings;
    std::optional<DepthStencilBinding> depthStencilBinding;

    // root signature & pipeline state

    std::optional<RootSignature> rootSignature;
    ComPtr<ID3D12RootSignature> cachedRootSignature;

    // TODO: create with PipelineState
    ComPtr<ID3D12PipelineState> cachedPipelineState;
};

class FrameGraphCompiler
{
public:

    ResourceIndex addTransientResource(
        const RscDesc &rscDesc,
        D3D12_RESOURCE_STATES initialState);

    ResourceIndex addExternalResource(
        ComPtr<ID3D12Resource> rsc,
        D3D12_RESOURCE_STATES  initialState,
        D3D12_RESOURCE_STATES  finalState);

    template<typename Func, typename...Args>
    PassIndex addRenderPass(Func &&passFunc, Args&&...args);

    std::vector<FrameGraphPassNode> compile(
        ResourceAllocator &rscAlloc);

private:

    struct RscTempNode
    {
        std::vector<PassIndex> users;

        ComPtr<ID3D12Resource> d3dRsc;
        D3D12_RESOURCE_STATES actualInitialState;
    };

    struct PassTempNode
    {
        struct RscInPassTempNode
        {
            D3D12_RESOURCE_STATES expectedState;
            size_t idxInRscUsers;
        };

        std::map<ResourceIndex, RscInPassTempNode> rscs;
    };

    void collectUsedRscsForSinglePass(
        std::vector<RscTempNode> &rscTempNodes,
        PassIndex                 passIndex,
        const CompilerPassNode   &passNode,
        PassTempNode             &passTempNode) const;

    void initD3DRscsAndInitialState(
        ResourceAllocator        &rscAlloc,
        std::vector<RscTempNode> &rscTempNodes) const;

    std::vector<CompilerResourceNode> rscNodes_;
    std::vector<CompilerPassNode>     passNodes_;
};

AGZ_D3D12_FG_END

#include "./impl/compiler.inl"
