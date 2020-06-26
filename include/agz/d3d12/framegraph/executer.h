#pragma once

#include <agz/d3d12/framegraph/cmdListPool.h>
#include <agz/d3d12/framegraph/graphData.h>
#include <agz/d3d12/framegraph/scheduler.h>
#include <agz/utility/thread.h>

AGZ_D3D12_FG_BEGIN

class FrameGraphExecuter
{
public:

    FrameGraphExecuter(
        ID3D12Device *device, int threadCount, int frameCount);

    void startFrame(int frameIndex);

    void execute(
        ID3D12DescriptorHeap *gpuRawHeap,
        FrameGraphData       &graph,
        DescriptorRange       allGPUDescs,
        DescriptorRange       allRTVDescs,
        DescriptorRange       allDSVDescs,
        ID3D12CommandQueue   *cmdQueue);

private:

    ID3D12Device *device_;

    int threadCount_;
    thread::thread_group_t threadGroup_;

    CommandListPool cmdListPool_;

    std::mutex schedulerMutex_;
};

AGZ_D3D12_FG_END
