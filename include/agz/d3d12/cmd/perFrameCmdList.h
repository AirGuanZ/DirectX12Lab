#pragma once

#include <agz/d3d12/window/window.h>

AGZ_D3D12_BEGIN

class PerFrameCommandList : public misc::uncopyable_t
{
    Window &window_;

    std::vector<ComPtr<ID3D12CommandAllocator>> cmdAllocators_;
    ComPtr<ID3D12GraphicsCommandList> cmdList_;

    std::vector<ComPtr<ID3D12Fence>> fences_;
    std::vector<UINT64> fenceValues_;
    HANDLE fenceEvent_;

    int curFrameIndex_;

public:

    explicit PerFrameCommandList(Window &window);

    ~PerFrameCommandList();

    void startFrame();

    void endFrame();

    void resetCommandList();

    ID3D12GraphicsCommandList *getCmdList();

    ID3D12GraphicsCommandList *operator->() const noexcept;

    int getFrameIndex() const noexcept;
};

inline PerFrameCommandList::PerFrameCommandList(Window &window)
    : window_(window), curFrameIndex_(0)
{
    for(int i = 0; i < window.getImageCount(); ++i)
    {
        // cmd allocator

        cmdAllocators_.push_back(
            window.createCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT));

        // fence

        fences_.push_back(window.createFence(0, D3D12_FENCE_FLAG_NONE));
        fenceValues_.push_back(0);
    }

    fenceEvent_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if(!fenceEvent_)
        throw D3D12LabException("failed to create fence event");

    cmdList_ = window.createGraphicsCommandList(
        0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdAllocators_[0].Get(), nullptr);
    cmdList_->Close();
}

inline PerFrameCommandList::~PerFrameCommandList()
{
    CloseHandle(fenceEvent_);
}

inline void PerFrameCommandList::startFrame()
{
    curFrameIndex_ = window_.getCurrentImageIndex();

    if(fences_[curFrameIndex_]->GetCompletedValue() <
        fenceValues_[curFrameIndex_])
    {
        fences_[curFrameIndex_]->SetEventOnCompletion(
            fenceValues_[curFrameIndex_], fenceEvent_);
        WaitForSingleObject(fenceEvent_, INFINITE);
    }

    ++fenceValues_[curFrameIndex_];

    cmdAllocators_[curFrameIndex_]->Reset();
}

inline void PerFrameCommandList::endFrame()
{
    window_.getCommandQueue()->Signal(
        fences_[curFrameIndex_].Get(),
        fenceValues_[curFrameIndex_]);
}

inline void PerFrameCommandList::resetCommandList()
{
    cmdList_->Reset(cmdAllocators_[curFrameIndex_].Get(), nullptr);
}

inline ID3D12GraphicsCommandList *PerFrameCommandList::getCmdList()
{
    return cmdList_.Get();
}

inline ID3D12GraphicsCommandList *PerFrameCommandList::operator->() const noexcept
{
    return cmdList_.Get();
}

inline int PerFrameCommandList::getFrameIndex() const noexcept
{
    return curFrameIndex_;
}

AGZ_D3D12_END
