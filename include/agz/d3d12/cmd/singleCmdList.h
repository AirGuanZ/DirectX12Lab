#pragma once

#include <d3d12.h>

#include <agz/d3d12/common.h>

AGZ_D3D12_BEGIN

class SingleCommandList
{
    ComPtr<ID3D12CommandAllocator> alloc_;
    ComPtr<ID3D12GraphicsCommandList> cmdList_;

public:

    explicit SingleCommandList(
        ID3D12Device           *device = nullptr,
        D3D12_COMMAND_LIST_TYPE type   = D3D12_COMMAND_LIST_TYPE_DIRECT);

    void initialize(
        ID3D12Device           *device,
        D3D12_COMMAND_LIST_TYPE type = D3D12_COMMAND_LIST_TYPE_DIRECT);

    bool isAvailable() const noexcept;

    void destroy();

    void resetCommandList();

    ID3D12GraphicsCommandList *operator->() noexcept;

    ID3D12GraphicsCommandList *getCmdList() noexcept;

    ID3D12CommandAllocator *getAlloc() noexcept;

    operator ID3D12GraphicsCommandList *() noexcept;
};

inline SingleCommandList::SingleCommandList(
    ID3D12Device           *device,
    D3D12_COMMAND_LIST_TYPE type)
{
    if(device)
        initialize(device, type);
}

inline void SingleCommandList::initialize(
    ID3D12Device           *device,
    D3D12_COMMAND_LIST_TYPE type)
{
    misc::scope_guard_t destroyGuard([&] { destroy(); });

    AGZ_D3D12_CHECK_HR(
        device->CreateCommandAllocator(
            type, IID_PPV_ARGS(alloc_.GetAddressOf())));

    AGZ_D3D12_CHECK_HR(
        device->CreateCommandList(
            0, type, alloc_.Get(), nullptr,
            IID_PPV_ARGS(cmdList_.GetAddressOf())));

    cmdList_->Close();

    destroyGuard.dismiss();
}

inline bool SingleCommandList::isAvailable() const noexcept
{
    return cmdList_ != nullptr;
}

inline void SingleCommandList::destroy()
{
    alloc_.Reset();
    cmdList_.Reset();
}

inline void SingleCommandList::resetCommandList()
{
    alloc_->Reset();
    cmdList_->Reset(alloc_.Get(), nullptr);
}

inline ID3D12GraphicsCommandList *SingleCommandList::operator->() noexcept
{
    return cmdList_.Get();
}

inline ID3D12GraphicsCommandList *SingleCommandList::getCmdList() noexcept
{
    return cmdList_.Get();
}

inline ID3D12CommandAllocator *SingleCommandList::getAlloc() noexcept
{
    return alloc_.Get();
}

inline SingleCommandList::operator struct ID3D12GraphicsCommandList*() noexcept
{
    return cmdList_.Get();
}

AGZ_D3D12_END
