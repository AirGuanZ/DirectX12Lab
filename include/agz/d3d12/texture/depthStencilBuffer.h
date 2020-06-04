#pragma once

#include <d3d12.h>

#include <agz/d3d12/pipeline/descriptorHeap.h>

AGZ_D3D12_BEGIN

ComPtr<ID3D12Resource> createDepthStencilBuffer(
    ID3D12Device             *device,
    int                       width,
    int                       height,
    DXGI_FORMAT               format,
    D3D12_DEPTH_STENCIL_VALUE expectedClearValue = { 1, 0 },
    const DXGI_SAMPLE_DESC   &sampleDesc         = { 1, 0 });

class DepthStencilBuffer
{
    ComPtr<ID3D12Resource> rsc_;

    std::unique_ptr<DescriptorHeap> ownedDSVHeap_;

    D3D12_CPU_DESCRIPTOR_HANDLE customedDSVHandle_;

public:

    DepthStencilBuffer();

    DepthStencilBuffer(DepthStencilBuffer &&other) noexcept;

    DepthStencilBuffer &operator=(DepthStencilBuffer &&other) noexcept;

    void swap(DepthStencilBuffer &other) noexcept;

    void initialize(
        ID3D12Device             *device,
        int                       width,
        int                       height,
        DXGI_FORMAT               format,
        D3D12_DEPTH_STENCIL_VALUE expectedClearValue = { 1, 0 },
        const DXGI_SAMPLE_DESC   &sampleDesc         = { 1, 0 });
    
    void initialize(
        ID3D12Device               *device,
        int                         width,
        int                         height,
        DXGI_FORMAT                 format,
        D3D12_CPU_DESCRIPTOR_HANDLE customedDSVHandle,
        D3D12_DEPTH_STENCIL_VALUE   expectedClearValue = { 1, 0 },
        const DXGI_SAMPLE_DESC     &sampleDesc         = { 1, 0 });

    bool isAvailable() const noexcept;

    void destroy();

    ID3D12Resource *getResource() const noexcept;

    D3D12_CPU_DESCRIPTOR_HANDLE getDSVHandle() const noexcept;
};

AGZ_D3D12_END
