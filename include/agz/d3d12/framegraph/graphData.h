#pragma once

#include <agz/d3d12/descriptor/descriptorHeap.h>
#include <agz/d3d12/framegraph/resourceView/depthStencilViewDesc.h>
#include <agz/d3d12/framegraph/resourceView/renderTargetViewDesc.h>
#include <agz/d3d12/framegraph/resourceView/shaderResourceViewDesc.h>
#include <agz/d3d12/framegraph/resourceView/unorderedAccessViewDesc.h>
#include <agz/d3d12/framegraph/RTDSBinding.h>
#include <agz/utility/misc.h>

AGZ_D3D12_FG_BEGIN

class FrameGraphPassContext;
class FrameGraphTaskScheduler;

using FrameGraphPassFunc = std::function<
    void(
        ID3D12GraphicsCommandList *,
        FrameGraphPassContext &
        )>;

class FrameGraphResourceNode : public misc::uncopyable_t
{
public:

    FrameGraphResourceNode(bool isExternal, ComPtr<ID3D12Resource> d3dRsc);

    FrameGraphResourceNode(FrameGraphResourceNode &&) noexcept = default;

    FrameGraphResourceNode &operator=(FrameGraphResourceNode &&) noexcept = default;

    void setExternalResource(ComPtr<ID3D12Resource> d3dRsc);

    ID3D12Resource *getD3DResource() const noexcept;

private:

    bool isExternal_;

    ComPtr<ID3D12Resource> d3dRsc_;
};

class FrameGraphPassNode
{
public:

    struct PassResource
    {
        ResourceIndex rscIdx;

        D3D12_RESOURCE_STATES beforeState  = {};
        D3D12_RESOURCE_STATES inState      = {};
        D3D12_RESOURCE_STATES afterState   = {};

        using ViewDesc = misc::variant_t<
            std::monostate,
            _internalSRV,
            _internalUAV,
            _internalRTV,
            _internalDSV>;
        ViewDesc viewDesc;

        DescriptorIndex descIdx = 0;
        mutable Descriptor descriptor;

        // render target & depth stencil binding

        struct RTB
        {
            bool clear = false;
            ClearColor clearColor;
        };

        struct DSB
        {
            bool clearDepth   = false;
            bool clearStencil = false;
            ClearDepthStencil clearDethpStencil;
        };

        using RTDSBinding = misc::variant_t<std::monostate, RTB, DSB>;

        RTDSBinding rtdsBinding;
    };

    struct PassViewport
    {
        std::vector<D3D12_VIEWPORT> viewports;
        std::vector<D3D12_RECT> scissors;
    };

    // init as graphics node
    FrameGraphPassNode(
        std::map<ResourceIndex, PassResource> rscs,
        PassViewport                          passViewport,
        FrameGraphPassFunc                    passFunc,
        ComPtr<ID3D12PipelineState>           pipelineState,
        ComPtr<ID3D12RootSignature>           rootSignature) noexcept;

    // init as compute node
    FrameGraphPassNode(
        std::map<ResourceIndex, PassResource> rscs,
        FrameGraphPassFunc                    passFunc,
        ComPtr<ID3D12PipelineState>           pipelineState,
        ComPtr<ID3D12RootSignature>           rootSignature) noexcept;

    bool execute(
        ID3D12Device                        *device,
        std::vector<FrameGraphResourceNode> &rscNodes,
        DescriptorRange                      allGPUDescs,
        DescriptorRange                      allRTVDescs,
        DescriptorRange                      allDSVDescs,
        ID3D12GraphicsCommandList           *cmdList) const;

private:

    template<bool IS_GRAPHICS>
    bool executeImpl(
        ID3D12Device                        *device,
        std::vector<FrameGraphResourceNode> &rscNodes,
        DescriptorRange                      allGPUDescs,
        DescriptorRange                      allRTVDescs,
        DescriptorRange                      allDSVDescs,
        ID3D12GraphicsCommandList           *cmdList) const;

    friend class FrameGraphPassContext;

    bool isGraphics_;

    std::map<ResourceIndex, PassResource> rscs_;

    PassViewport viewport_;

    FrameGraphPassFunc passFunc_;

    ComPtr<ID3D12PipelineState> pipelineState_;
    ComPtr<ID3D12RootSignature> rootSignature_;
};

struct FrameGraphData
{
    std::vector<FrameGraphPassNode>     passNodes;
    std::vector<FrameGraphResourceNode> rscNodes;

    DescriptorIndex gpuDescCount = 0;
    DescriptorIndex rtvDescCount = 0;
    DescriptorIndex dsvDescCount = 0;
};

AGZ_D3D12_FG_END
