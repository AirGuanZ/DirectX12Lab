#pragma once

AGZ_D3D12_FG_BEGIN

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

inline FrameGraphPassNode::FrameGraphPassNode(
    std::map<ResourceIndex, PassResource> &&rscs,
    std::vector<PassRenderTarget>         &&renderTargetBindings,
    std::optional<PassDepthStencil>       &&depthStencilBinding,
    ComPtr<ID3D12PipelineState>             pipelineState,
    ComPtr<ID3D12RootSignature>             rootSignature,
    FrameGraphPassFunc                    &&passFunc) noexcept
    : rscs_(std::move(rscs)),
      renderTargetBindings_(std::move(renderTargetBindings)),
      depthStencilBinding_(std::move(depthStencilBinding)),
      pipelineState_(std::move(pipelineState)),
      rootSignature_(std::move(rootSignature)),
      passFunc_(std::move(passFunc))
{
    
}

inline bool FrameGraphPassNode::execute(
    ID3D12Device *device, ID3D12GraphicsCommandList *cmdList)
{
    std::vector<D3D12_RESOURCE_BARRIER> inBarriers, outBarriers;
    inBarriers.reserve(rscs_.size());
    outBarriers.reserve(rscs_.size());

    // pipeline & root signature

    if(pipelineState_)
        cmdList->SetPipelineState(pipelineState_.Get());

    if(rootSignature_)
        cmdList->SetGraphicsRootSignature(rootSignature_.Get());

    // initial state transitions

    for(auto &p : rscs_)
    {
        auto &r = p.second;

        if(r.doInitialTransition)
        {
            inBarriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
                r.rsc.Get(), r.beforeState, r.afterState));
        }

        if(r.doFinalTransition)
        {
            outBarriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
                r.rsc.Get(), r.afterState, r.finalState));
        }
    }

    cmdList->ResourceBarrier(
        static_cast<UINT>(inBarriers.size()), inBarriers.data());

    // resource descriptors & bindings

    for(auto &p : rscs_)
    {
        auto &r = p.second;
        match_variant(r.viewDesc,
            [&](const std::monostate &) {},
            [&](const SRV &srv)
        {
            device->CreateShaderResourceView(
                r.rsc.Get(), &srv.desc, r.descriptor.getCPUHandle());
        },
            [&](const UAV &uav)
        {
            device->CreateUnorderedAccessView(
                r.rsc.Get(), nullptr, &uav.desc, r.descriptor.getCPUHandle());
        },
            [&](const RTV &rtv)
        {
            device->CreateRenderTargetView(
                r.rsc.Get(), &rtv.desc, r.descriptor.getCPUHandle());
        },
            [&](const DSV &dsv)
        {
            device->CreateDepthStencilView(
                r.rsc.Get(), &dsv.desc, r.descriptor.getCPUHandle());
        });

        switch(r.rootSignatureParamType)
        {
        case PassResource::ParamType::DescriptorTable:
            cmdList->SetGraphicsRootDescriptorTable(
                r.rootSignatureParamIndex, r.descriptor.getGPUHandle());
            break;
        case PassResource::ParamType::None:
            break;
        }
    }

    for(auto &r : renderTargetBindings_)
    {
        device->CreateRenderTargetView(
            r.rsc.Get(), &r.rtv.desc, r.descriptor.getCPUHandle());
    }

    if(depthStencilBinding_)
    {
        auto &d = *depthStencilBinding_;
        device->CreateDepthStencilView(
            d.rsc.Get(), &d.dsv.desc, d.descriptor.getCPUHandle());
    }

    if(!renderTargetBindings_.empty())
    {
        if(depthStencilBinding_)
        {
            cmdList->OMSetRenderTargets(
                static_cast<UINT>(renderTargetBindings_.size()),
                &renderTargetBindings_.front().descriptor.getCPUHandle(),
                true, &depthStencilBinding_->descriptor.getCPUHandle());
        }
        else
        {
            cmdList->OMSetRenderTargets(
                static_cast<UINT>(renderTargetBindings_.size()),
                &renderTargetBindings_.front().descriptor.getCPUHandle(),
                true, nullptr);
        }
    }
    else if(depthStencilBinding_)
    {
        cmdList->OMSetRenderTargets(
            0, nullptr, false,
            &depthStencilBinding_->descriptor.getCPUHandle());
    }
    else
        cmdList->OMSetRenderTargets(0, nullptr, false, nullptr);

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
