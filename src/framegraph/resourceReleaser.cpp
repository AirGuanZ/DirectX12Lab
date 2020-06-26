#include <agz/d3d12/framegraph/resourceReleaser.h>

AGZ_D3D12_FG_BEGIN

ResourceReleaser::ResourceReleaser(ID3D12Device *device)
    : nextExpectedFenceValue_(1)
{
    AGZ_D3D12_CHECK_HR(
        device->CreateFence(
            0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence_.GetAddressOf())));
}

ResourceReleaser::~ResourceReleaser()
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

void ResourceReleaser::collect()
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

void ResourceReleaser::addReleasePoint(ID3D12CommandQueue *cmdQueue)
{
    cmdQueue->Signal(fence_.Get(), nextExpectedFenceValue_++);
}

void ResourceReleaser::add(ComPtr<ID3D12Resource> rsc)
{
    records_.push_back({ ObjRecord{ std::move(rsc) }, nextExpectedFenceValue_ });
}

void ResourceReleaser::add(
    fg::ResourceAllocator &rscAlloc,
    ComPtr<ID3D12Resource> rsc)
{
    records_.push_back(
        {
            RscAllocRecord{ &rscAlloc, std::move(rsc) },
            nextExpectedFenceValue_
        });
}

void ResourceReleaser::add(
    DescriptorSubHeap &subheap, DescriptorRange range)
{
    records_.push_back(
        {
            DescriptorRangeRecord{ &subheap, range },
            nextExpectedFenceValue_
        });
}

void ResourceReleaser::add(std::unique_ptr<DescriptorHeap> heap)
{
    records_.push_back(
        {
            DescriptorHeapRecord{ std::move(heap) },
            nextExpectedFenceValue_
        });
}

AGZ_D3D12_FG_END
