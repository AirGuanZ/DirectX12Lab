#include <agz/d3d12/pipeline/depthStencilBuffer.h>

AGZ_D3D12_BEGIN

ComPtr<ID3D12Resource> createDepthStencilBuffer(
    ID3D12Device             *device,
    int                       width,
    int                       height,
    DXGI_FORMAT               format,
    D3D12_DEPTH_STENCIL_VALUE expectedClearValue,
    const DXGI_SAMPLE_DESC   &sampleDesc)
{
    D3D12_RESOURCE_DESC rscDesc;
    rscDesc.Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    rscDesc.Alignment        = 0;
    rscDesc.Width            = width;
    rscDesc.Height           = height;
    rscDesc.DepthOrArraySize = 1;
    rscDesc.MipLevels        = 1;
    rscDesc.Format           = format;
    rscDesc.SampleDesc       = sampleDesc;
    rscDesc.Layout           = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    rscDesc.Flags            = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);

    D3D12_CLEAR_VALUE clearValue;
    clearValue.Format       = format;
    clearValue.DepthStencil = expectedClearValue;

    ComPtr<ID3D12Resource> ret;
    AGZ_D3D12_CHECK_HR(device->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &rscDesc,
        D3D12_RESOURCE_STATE_COMMON, &clearValue,
        IID_PPV_ARGS(ret.GetAddressOf())));

    return ret;
}

AGZ_D3D12_END
