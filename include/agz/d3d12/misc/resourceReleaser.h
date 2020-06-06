#pragma once

#include <d3d12.h>

#include <agz/d3d12/common.h>

AGZ_D3D12_BEGIN

class ResourceReleaser : public misc::uncopyable_t
{
    struct Record
    {
        UINT64 fenceValue;
        ComPtr<ID3D12Object> rsc;
    };

    std::vector<Record> records_;

    ComPtr<ID3D12Fence> fence_;
    UINT64 nextFenceValue_ = 1;

    HANDLE fenceEvent_ = nullptr;

public:

    explicit ResourceReleaser(ID3D12Device *device = nullptr);

    ResourceReleaser(ResourceReleaser &&other) noexcept;

    ResourceReleaser &operator=(ResourceReleaser &&other) noexcept;

    ~ResourceReleaser();

    void swap(ResourceReleaser &other) noexcept;

    void add(
        ID3D12CommandQueue *queue,
        ComPtr<ID3D12Object> obj);

    void add(
        ID3D12CommandQueue *queue,
        std::initializer_list<ComPtr<ID3D12Object>> objs);

    void release();

    void waitUntilEmpty();

    bool empty() const noexcept;
};

inline ResourceReleaser::ResourceReleaser(ID3D12Device *device)
{
    if(!device)
        return;

    AGZ_D3D12_CHECK_HR(
        device->CreateFence(
            0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence_.GetAddressOf())));

    fenceEvent_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if(!fenceEvent_)
        throw D3D12LabException("ResourceReleaser: failed to create fence event");
}

inline ResourceReleaser::ResourceReleaser(ResourceReleaser &&other) noexcept
    : ResourceReleaser()
{
    swap(other);
}

inline ResourceReleaser &ResourceReleaser::operator=(
    ResourceReleaser &&other) noexcept
{
    swap(other);
    return *this;
}

inline ResourceReleaser::~ResourceReleaser()
{
    if(fence_)
    {
        waitUntilEmpty();
        CloseHandle(fenceEvent_);
    }
}

inline void ResourceReleaser::swap(ResourceReleaser &other) noexcept
{
    records_.swap(other.records_);
    fence_  .Swap(other.fence_);
    std::swap(nextFenceValue_, other.nextFenceValue_);
    std::swap(fenceEvent_,     other.fenceEvent_);
}

inline void ResourceReleaser::add(
    ID3D12CommandQueue *queue,
    ComPtr<ID3D12Object> obj)
{
    add(queue, { std::move(obj) });
}

inline void ResourceReleaser::add(
    ID3D12CommandQueue *queue,
    std::initializer_list<ComPtr<ID3D12Object>> objs)
{
    queue->Signal(fence_.Get(), nextFenceValue_);
    for(auto &obj : objs)
        records_.push_back({ nextFenceValue_, obj });
    ++nextFenceValue_;
}

inline void ResourceReleaser::release()
{
    std::vector<Record> newRecords;
    for(auto &r : records_)
    {
        if(fence_->GetCompletedValue() < r.fenceValue)
            newRecords.push_back(std::move(r));
    }
    records_.swap(newRecords);
}

inline void ResourceReleaser::waitUntilEmpty()
{
    for(auto &r : records_)
    {
        if(fence_->GetCompletedValue() < r.fenceValue)
        {
            fence_->SetEventOnCompletion(r.fenceValue, fenceEvent_);
            WaitForSingleObject(fenceEvent_, INFINITE);
        }
        r.rsc.Reset();
    }
    records_.clear();
}

inline bool ResourceReleaser::empty() const noexcept
{
    return records_.empty();
}

AGZ_D3D12_END
