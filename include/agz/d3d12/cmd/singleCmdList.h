#pragma once

#include <d3d12.h>

#include <agz/d3d12/common.h>

AGZ_D3D12_BEGIN

class GraphicsCommandList
{
    ComPtr<ID3D12CommandAllocator> alloc_;
    ComPtr<ID3D12GraphicsCommandList> cmdList_;

public:

    explicit GraphicsCommandList(ID3D12Device *device = nullptr);

    void initialize(ID3D12Device *device);

    bool isAvailable() const noexcept;

    void destroy();

    void resetCommandList();

    ID3D12GraphicsCommandList *operator->() noexcept;

    ID3D12GraphicsCommandList *getCmdList() noexcept;

    ID3D12CommandAllocator *getAlloc() noexcept;
};

inline GraphicsCommandList::GraphicsCommandList(ID3D12Device *device)
{
    if(device)
        initialize(device);
}

inline void GraphicsCommandList::initialize(ID3D12Device *device)
{
    misc::scope_guard_t destroyGuard([&] { destroy(); });

    AGZ_D3D12_CHECK_HR(
        device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(alloc_.GetAddressOf())));

    AGZ_D3D12_CHECK_HR(
        device->CreateCommandList(
            0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc_.Get(), nullptr,
            IID_PPV_ARGS(cmdList_.GetAddressOf())));

    cmdList_->Close();

    destroyGuard.dismiss();
}

inline bool GraphicsCommandList::isAvailable() const noexcept
{
    return cmdList_ != nullptr;
}

inline void GraphicsCommandList::destroy()
{
    alloc_.Reset();
    cmdList_.Reset();
}

inline void GraphicsCommandList::resetCommandList()
{
    alloc_->Reset();
    cmdList_->Reset(alloc_.Get(), nullptr);
}

inline ID3D12GraphicsCommandList *GraphicsCommandList::operator->() noexcept
{
    return cmdList_.Get();
}

inline ID3D12GraphicsCommandList *GraphicsCommandList::getCmdList() noexcept
{
    return cmdList_.Get();
}

inline ID3D12CommandAllocator *GraphicsCommandList::getAlloc() noexcept
{
    return alloc_.Get();
}

AGZ_D3D12_END
