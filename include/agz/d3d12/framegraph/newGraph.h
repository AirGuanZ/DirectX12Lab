#pragma once

#include <agz/d3d12/framegraph/cmdListPool.h>
#include <agz/d3d12/descriptor/descriptorHeap.h>
#include <agz/d3d12/framegraph/resourceView/shaderResourceViewDesc.h>
#include <agz/d3d12/framegraph/resourceView/unorderedAccessViewDesc.h>
#include <agz/utility/misc.h>

AGZ_D3D12_FG_BEGIN

class FrameGraphPassContext;

using FrameGraphPassFunc = std::function<
    void(
        ID3D12GraphicsCommandList *,
        FrameGraphPassContext &
        )>;

class FrameGraphPassNode
{
public:

    struct PassResource
    {
        ComPtr<ID3D12Resource> rsc;

        bool doInitialTransition;
        bool doFinalTransition;

        D3D12_RESOURCE_STATES beforeState;
        D3D12_RESOURCE_STATES inState;
        D3D12_RESOURCE_STATES afterState;

        Descriptor descriptor;
    };

    FrameGraphPassNode(
        std::map<ResourceIndex, PassResource> rscs,
        FrameGraphPassFunc                    passFunc) noexcept;

    bool execute(ID3D12GraphicsCommandList *cmdList);

private:

    friend class FrameGraphPassContext;

    std::map<ResourceIndex, PassResource> rscs_;

    FrameGraphPassFunc passFunc_;
};

class FrameGraphPassContext
{
public:

    struct Resource
    {
        ComPtr<ID3D12Resource> rsc;
        D3D12_RESOURCE_STATES  currentState = D3D12_RESOURCE_STATE_COMMON;
        Descriptor             descriptor;
    };

    explicit FrameGraphPassContext(FrameGraphPassNode &passNode) noexcept;

    Resource getResource(ResourceIndex index) const;

    void requestCmdListSubmission() noexcept;

    bool isCmdListSubmissionRequested() const noexcept;

private:

    bool requestCmdListSubmission_;

    FrameGraphPassNode &passNode_;
};

class FrameGraphTaskScheduler : public misc::uncopyable_t
{
public:

    FrameGraphTaskScheduler(
        std::vector<FrameGraphPassNode> &passNodes,
        CommandListPool                 &cmdListPool,
        ComPtr<ID3D12CommandQueue>       cmdQueue);

    struct TaskRange
    {
        FrameGraphPassNode *begNode = nullptr;
        FrameGraphPassNode *endNode = nullptr;
    };

    void restart();

    TaskRange requestTask();

    void submitTask(
        TaskRange                         taskRange,
        ComPtr<ID3D12GraphicsCommandList> cmdList);

    void waitForAllTasksFinished();

private:

    std::vector<FrameGraphPassNode> &passNodes_;
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

    std::mutex tasksMutex_;
    std::vector<Task> tasks_;

    size_t dispatchedNodeCount_;
    size_t finishedNodeCount_;

    std::condition_variable waitingForAllTasks_;
};

inline FrameGraphPassNode::FrameGraphPassNode(
    std::map<ResourceIndex, PassResource> rscs,
    FrameGraphPassFunc                    passFunc) noexcept
    : rscs_(std::move(rscs)), passFunc_(std::move(passFunc))
{

}

inline bool FrameGraphPassNode::execute(ID3D12GraphicsCommandList *cmdList)
{
    // rsc barriers

    std::vector<D3D12_RESOURCE_BARRIER> inBarriers, outBarriers;
    inBarriers.reserve(rscs_.size());
    outBarriers.reserve(rscs_.size());

    for(auto &p : rscs_)
    {
        auto &r = p.second;

        if(r.doInitialTransition)
        {
            inBarriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
                r.rsc.Get(), r.beforeState, r.inState));
        }

        if(r.doFinalTransition)
        {
            outBarriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
                r.rsc.Get(), r.inState, r.afterState));
        }
    }

    cmdList->ResourceBarrier(
        static_cast<UINT>(inBarriers.size()), inBarriers.data());

    // pass func context

    FrameGraphPassContext passCtx(*this);

    // call pass func

    assert(passFunc_);
    passFunc_(cmdList, passCtx);

    // final state transitions

    cmdList->ResourceBarrier(
        static_cast<UINT>(outBarriers.size()), outBarriers.data());

    return passCtx.isCmdListSubmissionRequested();
}

inline FrameGraphPassContext::FrameGraphPassContext(
    FrameGraphPassNode &passNode) noexcept
    : requestCmdListSubmission_(false), passNode_(passNode)
{
    
}

inline FrameGraphPassContext::Resource FrameGraphPassContext::getResource(
    ResourceIndex index) const
{
    const auto it = passNode_.rscs_.find(index);
    if(it == passNode_.rscs_.end())
        return {};

    Resource ret;
    ret.rsc          = it->second.rsc;
    ret.currentState = it->second.afterState;
    ret.descriptor   = it->second.descriptor;
    return ret;
}

inline void FrameGraphPassContext::requestCmdListSubmission() noexcept
{
    requestCmdListSubmission_ = true;
}

inline bool FrameGraphPassContext::isCmdListSubmissionRequested() const noexcept
{
    return requestCmdListSubmission_;
}

inline FrameGraphTaskScheduler::FrameGraphTaskScheduler(
    std::vector<FrameGraphPassNode> &passNodes,
    CommandListPool                 &cmdListPool,
    ComPtr<ID3D12CommandQueue>       cmdQueue)
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
    std::lock_guard lk(tasksMutex_);
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

    {
        std::lock_guard lk(tasksMutex_);

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
    }

    assert(!cmdLists.empty());
    cmdQueue_->ExecuteCommandLists(
        static_cast<UINT>(cmdLists.size()), cmdLists.data());

    if(finishedNodeCount_ >= tasks_.size())
        waitingForAllTasks_.notify_all();

    for(auto &c : gCmdLists)
        cmdListPool_.addUnusedGraphicsCmdLists(c);
}

inline void FrameGraphTaskScheduler::waitForAllTasksFinished()
{
    std::unique_lock lk(tasksMutex_);

    while(finishedNodeCount_ < tasks_.size())
        waitingForAllTasks_.wait(lk);
}

AGZ_D3D12_FG_END
