#pragma once

#include <d3dx12.h>

AGZ_D3D12_FG_BEGIN

inline ResourceAllocator::ResourceAllocator(ComPtr<ID3D12Device> device)
    : device_(std::move(device))
{

}

inline ResourceAllocator::~ResourceAllocator()
{

}

inline ComPtr<ID3D12Resource> ResourceAllocator::allocResource(
    const ResourceDesc    &desc,
    D3D12_RESOURCE_STATES  expectedInitialState,
    D3D12_RESOURCE_STATES &actualInitialState)
{
    const auto it = unusedRscs_.find(desc);
    if(it == unusedRscs_.end())
    {
        auto rsc = newResource(desc, expectedInitialState);
        actualInitialState = expectedInitialState;

        allocatedRscs_[rsc] = desc;
        return rsc;
    }

    auto rsc = it->second.rsc;
    actualInitialState = it->second.state;

    allocatedRscs_[rsc] = it->first;
    unusedRscs_.erase(it);
    return rsc;
}

inline void ResourceAllocator::freeResource(
    ComPtr<ID3D12Resource> rsc,
    D3D12_RESOURCE_STATES  state)
{
    const auto it = allocatedRscs_.find(rsc);
    assert(it != allocatedRscs_.end());

    unusedRscs_.insert({ it->second, { rsc, state } });
    allocatedRscs_.erase(it);
}

inline ComPtr<ID3D12Resource> ResourceAllocator::newResource(
    const ResourceDesc &desc, D3D12_RESOURCE_STATES initialState)
{
    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
    ComPtr<ID3D12Resource> ret;
    if(desc.clear)
    {
        AGZ_D3D12_CHECK_HR(
            device_->CreateCommittedResource(
                &heapProps, D3D12_HEAP_FLAG_NONE, &desc.desc,
                initialState, &desc.clearValue,
                IID_PPV_ARGS(ret.GetAddressOf())));
    }
    else
    {
        AGZ_D3D12_CHECK_HR(
            device_->CreateCommittedResource(
                &heapProps, D3D12_HEAP_FLAG_NONE, &desc.desc,
                initialState, nullptr,
                IID_PPV_ARGS(ret.GetAddressOf())));
    }
    return ret;
}

AGZ_D3D12_FG_END
