#pragma once

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
    CommandListPool                 &cmdListPool_;
    ComPtr<ID3D12CommandQueue>       cmdQueue_;

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

inline FrameGraphTaskScheduler::FrameGraphTaskScheduler(
    const std::vector<FrameGraphPassNode> &passNodes,
    CommandListPool                       &cmdListPool,
    ComPtr<ID3D12CommandQueue>             cmdQueue)
    : passNodes_(passNodes),
      cmdListPool_(cmdListPool),
      cmdQueue_(std::move(cmdQueue)),
      dispatchedNodeCount_(0),
      finishedNodeCount_(0)
{
    tasks_.resize(passNodes_.size());
    restart();
}

inline void FrameGraphTaskScheduler::restart()
{
    for(auto &t : tasks_)
    {
        t.taskState = TaskState::NotFinished;
        t.nodeCount = 0;
        t.cmdList.Reset();
    }

    dispatchedNodeCount_ = 0;
    finishedNodeCount_ = 0;
}

inline FrameGraphTaskScheduler::TaskRange FrameGraphTaskScheduler::requestTask()
{
    if(dispatchedNodeCount_ >= passNodes_.size())
        return { nullptr, nullptr };

    // IMPROVE: dispatch more nodes per request

    tasks_[dispatchedNodeCount_].taskState = TaskState::NotFinished;
    const auto passNode = &passNodes_[dispatchedNodeCount_++];

    return { passNode, passNode + 1 };
}

inline void FrameGraphTaskScheduler::submitTask(
    TaskRange                         taskRange,
    ComPtr<ID3D12GraphicsCommandList> cmdList)
{
    std::vector<ID3D12CommandList *> cmdLists;
    std::vector<ID3D12GraphicsCommandList *> gCmdLists;

    const size_t begNodeIdx = taskRange.begNode - &passNodes_[0];
    const size_t endNodeIdx = taskRange.endNode - &passNodes_[0];
    const size_t nodeCount  = taskRange.endNode - taskRange.begNode;

    // pend

    if(begNodeIdx > 0 && tasks_[begNodeIdx - 1].taskState != TaskState::Submitted)
    {
        tasks_[begNodeIdx].cmdList = std::move(cmdList);
        tasks_[begNodeIdx].nodeCount = nodeCount;

        for(size_t i = begNodeIdx; i < endNodeIdx; ++i)
            tasks_[i].taskState = TaskState::Pending;

        return;
    }

    // submit

    cmdLists.push_back(cmdList.Get());
    gCmdLists.push_back(cmdList.Get());

    for(size_t i = begNodeIdx; i < endNodeIdx; ++i)
        tasks_[i].taskState = TaskState::Submitted;

    finishedNodeCount_ = endNodeIdx;

    size_t nodeIdx = endNodeIdx;
    for(;;)
    {
        if(nodeIdx >= tasks_.size())
            break;

        if(tasks_[nodeIdx].taskState != TaskState::Pending)
            break;

        assert(tasks_[nodeIdx].nodeCount > 0);
        assert(tasks_[nodeIdx].cmdList);
        cmdLists.push_back(tasks_[nodeIdx].cmdList.Get());
        gCmdLists.push_back(tasks_[nodeIdx].cmdList.Get());

        const size_t nodeEnd = nodeIdx + tasks_[nodeIdx].nodeCount;
        for(size_t i = nodeIdx; i < nodeEnd; ++i)
            tasks_[i].taskState = TaskState::Submitted;

        nodeIdx = nodeEnd;
        finishedNodeCount_ = nodeEnd;
    }

    assert(!cmdLists.empty());
    cmdQueue_->ExecuteCommandLists(
        static_cast<UINT>(cmdLists.size()), cmdLists.data());

    for(auto &c : gCmdLists)
        cmdListPool_.addUnusedGraphicsCmdLists(c);
}

inline bool FrameGraphTaskScheduler::isAllFinished() const noexcept
{
    return finishedNodeCount_ >= tasks_.size();
}

AGZ_D3D12_FG_END
