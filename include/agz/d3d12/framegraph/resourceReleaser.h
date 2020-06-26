#pragma once

#include <d3d12.h>

#include <agz/d3d12/descriptor/descriptorHeap.h>
#include <agz/d3d12/framegraph/resourceAllocator.h>
#include <agz/utility/misc.h>

AGZ_D3D12_FG_BEGIN

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
        ResourceAllocator &rscAlloc, ComPtr<ID3D12Resource> rsc);

    void add(DescriptorSubHeap &subheap, DescriptorRange range);

    void add(std::unique_ptr<DescriptorHeap> heap);
};

AGZ_D3D12_FG_END
