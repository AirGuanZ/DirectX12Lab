#pragma once

#include <d3d12.h>

#include <agz/d3d12/descriptor/descriptorHeap.h>
#include <agz/d3d12/framegraph/resourceManager.h>
#include <agz/utility/misc.h>

AGZ_D3D12_BEGIN

class ResourceReleaser
{
    struct ObjRecord
    {
        ComPtr<ID3D12Resource> rsc;
    };

    struct RscAllocRecord
    {
        void release()
        {
            rscAlloc->freeResource(rsc);
        }

        fg::ResourceAllocator *rscAlloc;
        ComPtr<ID3D12Resource> rsc;
    };

    struct DescriptorRangeRecord
    {
        void release()
        {
            subheap->freeRange(range);
        }

        DescriptorSubHeap *subheap;
        DescriptorRange range;
    };

    struct DescriptorHeapRecord
    {
        void release()
        {
            heap.reset();
        }

        std::unique_ptr<DescriptorHeap> heap;
    };

    struct Record
    {
        using Releaser = misc::variant_t<
            ObjRecord,
            RscAllocRecord,
            DescriptorRangeRecord,
            DescriptorHeapRecord>;

        Releaser releaser;
        UINT64 expectedFenceValue = 0;
    };

    std::vector<Record> records_;

    ComPtr<ID3D12Fence> fence_;
    UINT64 nextExpectedFenceValue_;

public:

    explicit ResourceReleaser(ID3D12Device *device);

    ~ResourceReleaser();

    void collect();

    void addReleasePoint(ID3D12CommandQueue *cmdQueue);

    void add(ComPtr<ID3D12Resource> rsc);

    void add(
        fg::ResourceAllocator &rscAlloc, ComPtr<ID3D12Resource> rsc);

    void add(DescriptorSubHeap &subheap, DescriptorRange range);

    void add(std::unique_ptr<DescriptorHeap> heap);
};

inline ResourceReleaser::ResourceReleaser(ID3D12Device *device)
    : nextExpectedFenceValue_(1)
{
    AGZ_D3D12_CHECK_HR(
        device->CreateFence(
            0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence_.GetAddressOf())));
}

inline ResourceReleaser::~ResourceReleaser()
{
    for(auto &r : records_)
    {
        fence_->SetEventOnCompletion(r.expectedFenceValue, nullptr);
        match_variant(r.releaser,
            [&](ObjRecord             &   ) {                },
            [&](RscAllocRecord        &rar) { rar.release(); },
            [&](DescriptorRangeRecord &drr) { drr.release(); },
            [&](DescriptorHeapRecord  &dhr) { dhr.release(); });
    }
}

inline void ResourceReleaser::collect()
{
    std::vector<Record> newRecords;
    for(auto &r : records_)
    {
        if(fence_->GetCompletedValue() >= r.expectedFenceValue)
        {
            match_variant(r.releaser,
                [&](ObjRecord             &   ) {                },
                [&](RscAllocRecord        &rar) { rar.release(); },
                [&](DescriptorRangeRecord &drr) { drr.release(); },
                [&](DescriptorHeapRecord  &dhr) { dhr.release(); });
        }
        else
            newRecords.push_back(std::move(r));
    }
    records_.swap(newRecords);
}

inline void ResourceReleaser::addReleasePoint(ID3D12CommandQueue *cmdQueue)
{
    cmdQueue->Signal(fence_.Get(), nextExpectedFenceValue_++);
}

inline void ResourceReleaser::add(ComPtr<ID3D12Resource> rsc)
{
    records_.push_back({ ObjRecord{ std::move(rsc) }, nextExpectedFenceValue_ });
}

inline void ResourceReleaser::add(
    fg::ResourceAllocator &rscAlloc,
    ComPtr<ID3D12Resource> rsc)
{
    records_.push_back(
        {
            RscAllocRecord{ &rscAlloc, std::move(rsc) },
            nextExpectedFenceValue_
        });
}

inline void ResourceReleaser::add(
    DescriptorSubHeap &subheap, DescriptorRange range)
{
    records_.push_back(
        {
            DescriptorRangeRecord{ &subheap, range },
            nextExpectedFenceValue_
        });
}

inline void ResourceReleaser::add(std::unique_ptr<DescriptorHeap> heap)
{
    records_.push_back(
        {
            DescriptorHeapRecord{ std::move(heap) },
            nextExpectedFenceValue_
        });
}

AGZ_D3D12_END
