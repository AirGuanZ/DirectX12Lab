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

    void restart();

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

inline FrameGraph::FrameGraph(
    ID3D12Device       *device,
    IDXGIAdapter       *adaptor,
    DescriptorSubHeap   subRTVHeap,
    DescriptorSubHeap   subDSVHeap,
    DescriptorSubHeap   subGPUHeap,
    ID3D12CommandQueue *cmdQueue,
    int                 threadCount,
    int                 frameCount)
    : device_       (device),
      cmdQueue_     (cmdQueue),
      subRTVHeap_   (std::move(subRTVHeap)),
      subDSVHeap_   (std::move(subDSVHeap)),
      subGPUHeap_   (std::move(subGPUHeap)),
      rscAllocator_ (device, adaptor),
      graphReleaser_(device),
      frameReleaser_(device),
      executer_     (device, threadCount, frameCount)
{

}

inline FrameGraph::~FrameGraph()
{
    graphReleaser_.addReleasePoint(cmdQueue_);
    frameReleaser_.addReleasePoint(cmdQueue_);
}

inline void FrameGraph::startFrame(int frameIndex)
{
    executer_.startFrame(frameIndex);
    graphReleaser_.collect();
    frameReleaser_.collect();
}

inline void FrameGraph::endFrame()
{
    frameReleaser_.addReleasePoint(cmdQueue_);
}

inline void FrameGraph::newGraph()
{
    compiler_ = std::make_unique<FrameGraphCompiler>();
}

inline void FrameGraph::restart()
{
    graphReleaser_.addReleasePoint(cmdQueue_);

    compiler_.reset();
    graphData_ = {};
}

inline void FrameGraph::compile()
{
    graphReleaser_.addReleasePoint(cmdQueue_);
    graphData_ = compiler_->compile(device_, rscAllocator_, graphReleaser_);
}

inline void FrameGraph::setExternalRsc(
    ResourceIndex idx, ComPtr<ID3D12Resource> rsc)
{
    graphData_.rscNodes[idx.idx].setExternalResource(std::move(rsc));
}

inline void FrameGraph::execute()
{
    DescriptorRange rtvRange;
    if(graphData_.rtvDescCount)
    {
        rtvRange = subRTVHeap_.allocRange(graphData_.rtvDescCount);
        frameReleaser_.add(subRTVHeap_, rtvRange);
    }

    DescriptorRange dsvRange;
    if(graphData_.dsvDescCount)
    {
        dsvRange = subDSVHeap_.allocRange(graphData_.dsvDescCount);
        frameReleaser_.add(subDSVHeap_, dsvRange);
    }

    DescriptorRange gpuRange;
    if(graphData_.gpuDescCount)
    {
        gpuRange = subGPUHeap_.allocRange(graphData_.gpuDescCount);
        frameReleaser_.add(subGPUHeap_, gpuRange);
    }

    executer_.execute(
        subGPUHeap_.getRawHeap(), graphData_,
        gpuRange, rtvRange, dsvRange, cmdQueue_);
}

inline ResourceIndex FrameGraph::addInternalResource(
    const RscDesc &rscDesc, D3D12_RESOURCE_STATES initialState)
{
    return compiler_->addInternalResource(rscDesc, initialState);
}

inline ResourceIndex FrameGraph::addInternalResource(
    const RscDesc        &rscDesc,
    D3D12_RESOURCE_STATES initialState,
    const ClearColor     &clearColorValue,
    DXGI_FORMAT           clearFormat)
{
    return compiler_->addInternalResource(
        rscDesc, initialState, clearColorValue, clearFormat);
}

inline ResourceIndex FrameGraph::addInternalResource(
    const RscDesc           &rscDesc,
    D3D12_RESOURCE_STATES    initialState,
    const ClearDepthStencil &clearDepthStencilValue,
    DXGI_FORMAT              clearFormat)
{
    return compiler_->addInternalResource(
        rscDesc, initialState, clearDepthStencilValue, clearFormat);
}

inline ResourceIndex FrameGraph::addInternalResource(
    const RscDesc &rscDesc)
{
    return addInternalResource(rscDesc, D3D12_RESOURCE_STATE_COMMON);
}

inline ResourceIndex FrameGraph::addInternalResource(
    const RscDesc        &rscDesc,
    const ClearColor     &clearColorValue,
    DXGI_FORMAT           clearFormat)
{
    return addInternalResource(
        rscDesc, D3D12_RESOURCE_STATE_COMMON,
        clearColorValue, clearFormat);
}

inline ResourceIndex FrameGraph::addInternalResource(
    const RscDesc           &rscDesc,
    const ClearDepthStencil &clearDepthStencilValue,
    DXGI_FORMAT              clearFormat)
{
    return addInternalResource(
        rscDesc, D3D12_RESOURCE_STATE_COMMON,
        clearDepthStencilValue, clearFormat);
}

inline ResourceIndex FrameGraph::addExternalResource(
    const ComPtr<ID3D12Resource> rscDesc,
    D3D12_RESOURCE_STATES        initialState,
    D3D12_RESOURCE_STATES        finalState)
{
    return compiler_->addExternalResource(
        rscDesc, initialState, finalState);
}

template<typename ... Args>
PassIndex FrameGraph::addGraphicsPass(
    FrameGraphPassFunc passFunc, Args &&... args)
{
    return compiler_->addGraphicsPass(
        std::move(passFunc), std::forward<Args>(args)...);
}

AGZ_D3D12_FG_END
