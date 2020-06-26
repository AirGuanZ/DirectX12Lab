#include <agz/d3d12/framegraph/framegraph.h>

AGZ_D3D12_FG_BEGIN

FrameGraph::FrameGraph(
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

FrameGraph::~FrameGraph()
{
    graphReleaser_.addReleasePoint(cmdQueue_);
    frameReleaser_.addReleasePoint(cmdQueue_);
}

void FrameGraph::startFrame(int frameIndex)
{
    executer_.startFrame(frameIndex);
    graphReleaser_.collect();
    frameReleaser_.collect();
}

void FrameGraph::endFrame()
{
    frameReleaser_.addReleasePoint(cmdQueue_);
}

void FrameGraph::newGraph()
{
    compiler_ = std::make_unique<FrameGraphCompiler>();
}

void FrameGraph::restart()
{
    graphReleaser_.addReleasePoint(cmdQueue_);

    compiler_.reset();
    graphData_ = {};
}

void FrameGraph::compile()
{
    graphReleaser_.addReleasePoint(cmdQueue_);
    graphData_ = compiler_->compile(rscAllocator_, graphReleaser_);
}

void FrameGraph::setExternalRsc(
    ResourceIndex idx, ComPtr<ID3D12Resource> rsc)
{
    graphData_.rscNodes[idx.idx].setExternalResource(std::move(rsc));
}

void FrameGraph::execute()
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

ResourceIndex FrameGraph::addInternalResource(
    const RscDesc &rscDesc, D3D12_RESOURCE_STATES initialState)
{
    return compiler_->addInternalResource(rscDesc, initialState);
}

ResourceIndex FrameGraph::addInternalResource(
    const RscDesc        &rscDesc,
    D3D12_RESOURCE_STATES initialState,
    const ClearColor     &clearColorValue,
    DXGI_FORMAT           clearFormat)
{
    return compiler_->addInternalResource(
        rscDesc, initialState, clearColorValue, clearFormat);
}

ResourceIndex FrameGraph::addInternalResource(
    const RscDesc           &rscDesc,
    D3D12_RESOURCE_STATES    initialState,
    const ClearDepthStencil &clearDepthStencilValue,
    DXGI_FORMAT              clearFormat)
{
    return compiler_->addInternalResource(
        rscDesc, initialState, clearDepthStencilValue, clearFormat);
}

ResourceIndex FrameGraph::addInternalResource(
    const RscDesc &rscDesc)
{
    return addInternalResource(rscDesc, D3D12_RESOURCE_STATE_COMMON);
}

ResourceIndex FrameGraph::addInternalResource(
    const RscDesc        &rscDesc,
    const ClearColor     &clearColorValue,
    DXGI_FORMAT           clearFormat)
{
    return addInternalResource(
        rscDesc, D3D12_RESOURCE_STATE_COMMON,
        clearColorValue, clearFormat);
}

ResourceIndex FrameGraph::addInternalResource(
    const RscDesc           &rscDesc,
    const ClearDepthStencil &clearDepthStencilValue,
    DXGI_FORMAT              clearFormat)
{
    return addInternalResource(
        rscDesc, D3D12_RESOURCE_STATE_COMMON,
        clearDepthStencilValue, clearFormat);
}

ResourceIndex FrameGraph::addExternalResource(
    const ComPtr<ID3D12Resource> rscDesc,
    D3D12_RESOURCE_STATES        initialState,
    D3D12_RESOURCE_STATES        finalState)
{
    return compiler_->addExternalResource(
        rscDesc, initialState, finalState);
}

AGZ_D3D12_FG_END
