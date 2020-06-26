#pragma once

#include <map>

#include <d3d12.h>

#include <D3D12MemAlloc.h>

#include <agz/d3d12/framegraph/common.h>

AGZ_D3D12_FG_BEGIN

// no method is thread-safe
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

    ResourceAllocator(ID3D12Device *device, IDXGIAdapter *adaptor);

    ~ResourceAllocator();

    ComPtr<ID3D12Resource> allocResource(
        const ResourceDesc    &desc,
        D3D12_RESOURCE_STATES  expectedInitialState);

    void freeResource(ComPtr<ID3D12Resource> rsc);

private:

    struct D3D12MADeleter
    {
        void operator()(D3D12MA::Allocator *allocator) const
        {
            if(allocator)
                allocator->Release();
        }
    };

    std::unique_ptr<D3D12MA::Allocator, D3D12MADeleter> d3d12MemAlloc_;

    std::map<ComPtr<ID3D12Resource>, D3D12MA::Allocation*> allocatedRscs_;
};

AGZ_D3D12_FG_END

#include "./impl/resourceManager.inl"
