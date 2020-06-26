#include <agz/d3d12/framegraph/executer.h>

AGZ_D3D12_FG_BEGIN

FrameGraphExecuter::FrameGraphExecuter(
    ID3D12Device *device, int threadCount, int frameCount)
    : device_(device),
      threadCount_(threadCount),
      threadGroup_(threadCount),
      cmdListPool_(device, threadCount, frameCount)
{
    
}

void FrameGraphExecuter::startFrame(int frameIndex)
{
    cmdListPool_.startFrame(frameIndex);
}

void FrameGraphExecuter::execute(
    ID3D12DescriptorHeap *gpuRawHeap,
    FrameGraphData       &graph,
    DescriptorRange       allGPUDescs,
    DescriptorRange       allRTVDescs,
    DescriptorRange       allDSVDescs,
    ID3D12CommandQueue   *cmdQueue)
{
    FrameGraphTaskScheduler scheduler(
        graph.passNodes, cmdListPool_, cmdQueue);

    threadGroup_.run(
        threadCount_,
        [&](int threadIndex)
    {
        FrameGraphTaskScheduler::TaskRange task;

        {
            std::lock_guard lk(schedulerMutex_);
            task = scheduler.requestTask();
        }

        if(!task.begNode)
            return;

        auto cmdList = cmdListPool_.requireGraphicsCommandList(threadIndex);
        if(gpuRawHeap)
            cmdList->SetDescriptorHeaps(1, &gpuRawHeap);

        for(auto n = task.begNode; n != task.endNode; ++n)
        {
            n->execute(
                device_, graph.rscNodes,
                allGPUDescs, allRTVDescs, allDSVDescs,
                cmdList.Get());
        }

        cmdList->Close();

        {
            std::lock_guard lk(schedulerMutex_);
            scheduler.submitTask(task, cmdList);
        }
    });
}

AGZ_D3D12_FG_END
