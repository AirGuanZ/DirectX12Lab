#pragma once

#include <agz/d3d12/framegraph/cmdListPool.h>
#include <agz/d3d12/framegraph/graphData.h>

AGZ_D3D12_FG_BEGIN

class FrameGraphTaskScheduler : public misc::uncopyable_t
{
public:

    FrameGraphTaskScheduler(
        const std::vector<FrameGraphPassNode> &passNodes,
        CommandListPool                       &cmdListPool,
        ComPtr<ID3D12CommandQueue>             cmdQueue);

    struct TaskRange
    {
        const FrameGraphPassNode *begNode = nullptr;
        const FrameGraphPassNode *endNode = nullptr;
    };

    void restart();

    TaskRange requestTask();

    void submitTask(
        TaskRange                         taskRange,
        ComPtr<ID3D12GraphicsCommandList> cmdList);

    bool isAllFinished() const noexcept;

private:

    const std::vector<FrameGraphPassNode> &passNodes_;
    CommandListPool                       &cmdListPool_;
    ComPtr<ID3D12CommandQueue>             cmdQueue_;

    enum class TaskState
    {
        NotFinished,
        Pending,
        Submitted
    };

    struct Task
    {
        TaskState taskState = TaskState::NotFinished;
        size_t nodeCount = 0;
        ComPtr<ID3D12GraphicsCommandList> cmdList;
    };

    std::vector<Task> tasks_;

    size_t dispatchedNodeCount_;
    size_t finishedNodeCount_;
};

AGZ_D3D12_FG_END
