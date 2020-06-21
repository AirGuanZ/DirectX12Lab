#pragma once

#include <agz/d3d12/framegraph/cmdListPool.h>
#include <agz/d3d12/framegraph/compiler.h>
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
        FrameGraphCompiler::CompileResult &graph,
        DescriptorRange                    allGPUDescs,
        DescriptorRange                    allRTVDescs,
        DescriptorRange                    allDSVDescs,
        ID3D12CommandQueue                *cmdQueue);

private:

    ID3D12Device *device_;

    int threadCount_;
    thread::thread_group_t threadGroup_;

    CommandListPool cmdListPool_;

    std::mutex schedulerMutex_;
};

inline FrameGraphExecuter::FrameGraphExecuter(
    ID3D12Device *device, int threadCount, int frameCount)
    : device_(device),
      threadCount_(threadCount),
      threadGroup_(threadCount),
      cmdListPool_(device, threadCount, frameCount)
{
    
}

inline void FrameGraphExecuter::startFrame(int frameIndex)
{
    cmdListPool_.startFrame(frameIndex);
}

inline void FrameGraphExecuter::execute(
    FrameGraphCompiler::CompileResult &graph,
    DescriptorRange                    allGPUDescs,
    DescriptorRange                    allRTVDescs,
    DescriptorRange                    allDSVDescs,
    ID3D12CommandQueue                *cmdQueue)
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

        auto cmdList = cmdListPool_.requireUnusedCommandList(threadIndex);
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
