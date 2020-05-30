#pragma once

#include <d3d12.h>

#include <agz/d3d12/common.h>

AGZ_D3D12_LAB_BEGIN

class CommandQueueWaiter : public misc::uncopyable_t
{
    ComPtr<ID3D12Fence> fence_;

    UINT64 fenceValue_;
    HANDLE fenceEvent_;

public:

    explicit CommandQueueWaiter(ID3D12Device *device)
    {
        AGZ_D3D12_CHECK_HR(
            device->CreateFence(
                0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence_.GetAddressOf())));

        fenceValue_ = 0;

        fenceEvent_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    }

    ~CommandQueueWaiter()
    {
        CloseHandle(fenceEvent_);
    }

    void waitIdle(ID3D12CommandQueue *cmdQueue)
    {
        ++fenceValue_;
        cmdQueue->Signal(fence_.Get(), fenceValue_);
        if(fence_->GetCompletedValue() < fenceValue_)
        {
            fence_->SetEventOnCompletion(fenceValue_, fenceEvent_);
            WaitForSingleObject(fenceEvent_, INFINITE);
        }
    }
};

AGZ_D3D12_LAB_END
