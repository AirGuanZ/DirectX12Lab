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

    explicit PerFrameCommandList(
        Window &window,
        D3D12_COMMAND_LIST_TYPE cmdListType = D3D12_COMMAND_LIST_TYPE_DIRECT);

    ~PerFrameCommandList();

    void startFrame();

    void endFrame(ID3D12CommandQueue *cmdQueue = nullptr);

    void resetCommandList();

    ID3D12GraphicsCommandList *getCmdList() noexcept;

    operator ID3D12GraphicsCommandList *() noexcept;

    ID3D12GraphicsCommandList *operator->() noexcept;

    int getFrameIndex() const noexcept;
};

inline PerFrameCommandList::PerFrameCommandList(
    Window &window, D3D12_COMMAND_LIST_TYPE cmdListType)
    : window_(window), curFrameIndex_(0)
{
    for(int i = 0; i < window.getImageCount(); ++i)
    {
        // cmd allocator

        ComPtr<ID3D12CommandAllocator> ca;
        AGZ_D3D12_CHECK_HR(
            window.getDevice()->CreateCommandAllocator(
                cmdListType,
                IID_PPV_ARGS(ca.GetAddressOf())));
        cmdAllocators_.push_back(std::move(ca));

        // fence

        ComPtr<ID3D12Fence> f;
        AGZ_D3D12_CHECK_HR(
            window.getDevice()->CreateFence(
                0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(f.GetAddressOf())));
        fences_.push_back(std::move(f));

        fenceValues_.push_back(0);
    }

    fenceEvent_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if(!fenceEvent_)
        throw D3D12LabException("failed to create fence event");

    AGZ_D3D12_CHECK_HR(
        window.getDevice()->CreateCommandList(
            0, cmdListType, cmdAllocators_[0].Get(), nullptr,
            IID_PPV_ARGS(cmdList_.GetAddressOf())));

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

inline void PerFrameCommandList::endFrame(ID3D12CommandQueue *cmdQueue)
{
    if(cmdQueue)
    {
        cmdQueue->Signal(fences_[curFrameIndex_].Get(),
            fenceValues_[curFrameIndex_]);
    }
    else
    {
        window_.getCommandQueue()->Signal(
            fences_[curFrameIndex_].Get(),
            fenceValues_[curFrameIndex_]);
    }
}

inline void PerFrameCommandList::resetCommandList()
{
    cmdList_->Reset(cmdAllocators_[curFrameIndex_].Get(), nullptr);
}

inline ID3D12GraphicsCommandList *PerFrameCommandList::getCmdList() noexcept
{
    return cmdList_.Get();
}

inline PerFrameCommandList::operator struct ID3D12GraphicsCommandList*() noexcept
{
    return cmdList_.Get();
}

inline ID3D12GraphicsCommandList *PerFrameCommandList::operator->() noexcept
{
    return cmdList_.Get();
}

inline int PerFrameCommandList::getFrameIndex() const noexcept
{
    return curFrameIndex_;
}

AGZ_D3D12_END
