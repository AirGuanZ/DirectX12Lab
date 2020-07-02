#pragma once

#include <agz/d3d12/framegraph/compiler.h>
#include <agz/d3d12/framegraph/executer.h>
#include <agz/d3d12/framegraph/graphData.h>

AGZ_D3D12_FG_BEGIN

class FrameGraph : public misc::uncopyable_t
{
public:

    FrameGraph(
        ID3D12Device       *device,
        IDXGIAdapter       *adaptor,
        DescriptorSubHeap   subRTVHeap,
        DescriptorSubHeap   subDSVHeap,
        DescriptorSubHeap   subGPUHeap,
        ID3D12CommandQueue *cmdQueue,
        int threadCount,
        int frameCount);

    ~FrameGraph();

    void startFrame(int frameIndex);

    void endFrame();

    void newGraph();

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
    
    ResourceIndex addInternalResource(
        const RscDesc        &rscDesc);

    ResourceIndex addInternalResource(
        const RscDesc        &rscDesc,
        const ClearColor     &clearColorValue,
        DXGI_FORMAT           clearFormat = DXGI_FORMAT_UNKNOWN);

    ResourceIndex addInternalResource(
        const RscDesc           &rscDesc,
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

    void reset();

    void compile();

    void setExternalRsc(ResourceIndex idx, ComPtr<ID3D12Resource> rsc);

    void execute();

private:

    ID3D12Device       *device_;
    ID3D12CommandQueue *cmdQueue_;

    DescriptorSubHeap subRTVHeap_;
    DescriptorSubHeap subDSVHeap_;
    DescriptorSubHeap subGPUHeap_;

    ResourceAllocator rscAllocator_;
    ResourceReleaser  graphReleaser_;
    ResourceReleaser  frameReleaser_;

    FrameGraphExecuter executer_;

    std::unique_ptr<FrameGraphCompiler> compiler_;
    FrameGraphData graphData_;
};

template<typename ... Args>
PassIndex FrameGraph::addGraphicsPass(
    FrameGraphPassFunc passFunc, Args &&... args)
{
    return compiler_->addGraphicsPass(
        std::move(passFunc), std::forward<Args>(args)...);
}

template<typename ... Args>
PassIndex FrameGraph::addComputePass(
    FrameGraphPassFunc passFunc, Args &&... args)
{
    return compiler_->addComputePass(
        std::move(passFunc), std::forward<Args>(args)...);
}

AGZ_D3D12_FG_END
