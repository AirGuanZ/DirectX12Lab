#pragma once

#include <agz/d3d12/framegraph/compiler.h>
#include <agz/d3d12/framegraph/executer.h>
#include <agz/d3d12/framegraph/graphData.h>

AGZ_D3D12_FG_BEGIN

class FrameGraph
{
public:

    FrameGraph(
        ID3D12Device *device,
        DescriptorSubHeap *subRTVHeap,
        DescriptorSubHeap *subDSVHeap,
        DescriptorSubHeap *subGPUHeap,
        ID3D12CommandQueue *cmdQueue,
        int threadCount,
        int frameCount);

    ~FrameGraph();

    void startFrame(int frameIndex);

    void endFrame();

    void newGraph();

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

    void restart();

    void compile();

    void setExternalRsc(ResourceIndex idx, ComPtr<ID3D12Resource> rsc);

    void execute();

private:

    ID3D12Device       *device_;
    ID3D12CommandQueue *cmdQueue_;

    DescriptorSubHeap *subRTVHeap_;
    DescriptorSubHeap *subDSVHeap_;
    DescriptorSubHeap *subGPUHeap_;

    ResourceAllocator rscAllocator_;
    ResourceReleaser  graphReleaser_;
    ResourceReleaser  frameReleaser_;

    FrameGraphExecuter executer_;

    std::unique_ptr<FrameGraphCompiler> compiler_;
    FrameGraphData graphData_;
};

inline FrameGraph::FrameGraph(
    ID3D12Device       *device,
    DescriptorSubHeap  *subRTVHeap,
    DescriptorSubHeap  *subDSVHeap,
    DescriptorSubHeap  *subGPUHeap,
    ID3D12CommandQueue *cmdQueue,
    int                 threadCount,
    int                 frameCount)
    : device_       (device),
      cmdQueue_     (cmdQueue),
      subRTVHeap_   (subRTVHeap),
      subDSVHeap_   (subDSVHeap),
      subGPUHeap_   (subGPUHeap),
      rscAllocator_ (device),
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
        const auto or = subRTVHeap_->allocRange(graphData_.rtvDescCount);
        if(!or)
        {
            throw D3D12LabException(
                "failed to allocate render target view descriptor");
        }
        rtvRange = *or ;
        frameReleaser_.add(*subRTVHeap_, rtvRange);
    }

    DescriptorRange dsvRange;
    if(graphData_.dsvDescCount)
    {
        const auto or = subDSVHeap_->allocRange(graphData_.dsvDescCount);
        if(!or)
        {
            throw D3D12LabException(
                "failed to allocate depth stencil view descriptor");
        }
        dsvRange = *or ;
        frameReleaser_.add(*subDSVHeap_, dsvRange);
    }

    DescriptorRange gpuRange;
    if(graphData_.gpuDescCount)
    {
        const auto or = subGPUHeap_->allocRange(graphData_.gpuDescCount);
        if(!or)
        {
            throw D3D12LabException(
                "failed to allocate GPU view descriptor");
        }
        gpuRange = *or ;
        frameReleaser_.add(*subGPUHeap_, gpuRange);
    }

    executer_.execute(
        subGPUHeap_->getRawHeap(), graphData_,
        gpuRange, rtvRange, dsvRange, cmdQueue_);
}

inline ResourceIndex FrameGraph::addTransientResource(
    const RscDesc &rscDesc, D3D12_RESOURCE_STATES initialState)
{
    return compiler_->addTransientResource(rscDesc, initialState);
}

inline ResourceIndex FrameGraph::addTransientResource(
    const RscDesc        &rscDesc,
    D3D12_RESOURCE_STATES initialState,
    const ClearColor     &clearColorValue,
    DXGI_FORMAT           clearFormat)
{
    return compiler_->addTransientResource(
        rscDesc, initialState, clearColorValue, clearFormat);
}

inline ResourceIndex FrameGraph::addTransientResource(
    const RscDesc           &rscDesc,
    D3D12_RESOURCE_STATES    initialState,
    const ClearDepthStencil &clearDepthStencilValue,
    DXGI_FORMAT              clearFormat)
{
    return compiler_->addTransientResource(
        rscDesc, initialState, clearDepthStencilValue, clearFormat);
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
PassIndex FrameGraph::addPass(
    FrameGraphPassFunc passFunc, Args &&... args)
{
    return compiler_->addPass(
        std::move(passFunc), std::forward<Args>(args)...);
}

AGZ_D3D12_FG_END
