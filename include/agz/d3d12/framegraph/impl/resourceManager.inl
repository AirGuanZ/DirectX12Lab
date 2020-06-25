#pragma once

#include <d3dx12.h>

AGZ_D3D12_FG_BEGIN

inline ResourceAllocator::ResourceAllocator(
    ID3D12Device *device, IDXGIAdapter *adaptor)
{
    D3D12MA::ALLOCATOR_DESC allocatorDesc = {};
    allocatorDesc.pDevice  = device;
    allocatorDesc.pAdapter = adaptor;

    D3D12MA::Allocator *allocator;
    AGZ_D3D12_CHECK_HR(CreateAllocator(&allocatorDesc, &allocator));
    d3d12MemAlloc_.reset(allocator);
}

inline ResourceAllocator::~ResourceAllocator()
{
    for(auto &rsc : allocatedRscs_)
        rsc.second->Release();
}

inline ComPtr<ID3D12Resource> ResourceAllocator::allocResource(
    const ResourceDesc    &desc,
    D3D12_RESOURCE_STATES  expectedInitialState)
{
    D3D12MA::ALLOCATION_DESC allocDesc = {};
    allocDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

    D3D12MA::Allocation *allocation;
    ComPtr<ID3D12Resource> ret;
    AGZ_D3D12_CHECK_HR(
        d3d12MemAlloc_->CreateResource(
            &allocDesc, &desc.desc,
            expectedInitialState,
            desc.clear ? &desc.clearValue : nullptr,
            &allocation, IID_PPV_ARGS(ret.GetAddressOf())));

    allocatedRscs_[ret] = allocation;
    return ret;
}

inline void ResourceAllocator::freeResource(
    ComPtr<ID3D12Resource> rsc)
{
    const auto it = allocatedRscs_.find(rsc);
    assert(it != allocatedRscs_.end());
    it->second->Release();
    allocatedRscs_.erase(it);
}

AGZ_D3D12_FG_END
