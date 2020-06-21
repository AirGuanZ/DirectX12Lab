#pragma once

#include <map>

#include <d3d12.h>

#include <agz/d3d12/framegraph/common.h>

AGZ_D3D12_FG_BEGIN

// no method is thread-safe
// a resource can be:
//      1. allocated from the allocator
//      2. being freed which means it cannot be used by cpu anymore.
//         however the gpu may still use it.
class ResourceAllocator : public misc::uncopyable_t
{
public:

    struct ResourceDesc
    {
        D3D12_RESOURCE_DESC desc     = {};
        bool clear                   = false;
        D3D12_CLEAR_VALUE clearValue = {};

        bool operator<(const ResourceDesc &rhs) const noexcept
        {
            return std::memcmp(this, &rhs, sizeof(rhs)) < 0;
        }
    };

    explicit ResourceAllocator(ComPtr<ID3D12Device> device);

    ~ResourceAllocator();

    ComPtr<ID3D12Resource> allocResource(
        const ResourceDesc    &desc,
        D3D12_RESOURCE_STATES  expectedInitialState,
        D3D12_RESOURCE_STATES &actualInitialState);

    void freeResource(
        ComPtr<ID3D12Resource> rsc,
        D3D12_RESOURCE_STATES  state);

private:

    ComPtr<ID3D12Resource> newResource(
        const ResourceDesc &desc, D3D12_RESOURCE_STATES initialState);

    struct UnusedRsc
    {
        ComPtr<ID3D12Resource> rsc;
        D3D12_RESOURCE_STATES  state;
    };

    struct AllocatedRsc
    {
        ResourceDesc           desc;
        ComPtr<ID3D12Resource> rsc;
    };

    ComPtr<ID3D12Device> device_;

    std::multimap<ResourceDesc, UnusedRsc>         unusedRscs_;
    std::map<ComPtr<ID3D12Resource>, ResourceDesc> allocatedRscs_;
};

AGZ_D3D12_FG_END

#include "./impl/resourceManager.inl"
