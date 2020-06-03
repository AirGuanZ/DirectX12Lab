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

AGZ_D3D12_END
