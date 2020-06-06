#include <agz/d3d12/texture/depthStencilBuffer.h>

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

DepthStencilBuffer::DepthStencilBuffer()
    : customedDSVHandle_{ 0 }
{
    
}

DepthStencilBuffer::DepthStencilBuffer(DepthStencilBuffer &&other) noexcept
    : DepthStencilBuffer()
{
    swap(other);
}

DepthStencilBuffer &DepthStencilBuffer::operator=(
    DepthStencilBuffer &&other) noexcept
{
    swap(other);
    return *this;
}

void DepthStencilBuffer::swap(DepthStencilBuffer &other) noexcept
{
    std::swap(rsc_,               other.rsc_);
    std::swap(ownedDSVHeap_,      other.ownedDSVHeap_);
    std::swap(customedDSVHandle_, other.customedDSVHandle_);
}

void DepthStencilBuffer::initialize(
    ID3D12Device               *device,
    int                         width,
    int                         height,
    DXGI_FORMAT                 format,
    D3D12_CPU_DESCRIPTOR_HANDLE customedDSVHandle,
    D3D12_DEPTH_STENCIL_VALUE   expectedClearValue,
    const DXGI_SAMPLE_DESC     &sampleDesc)
{
    rsc_ = createDepthStencilBuffer(
        device, width, height, format, expectedClearValue, sampleDesc);

    customedDSVHandle_ = customedDSVHandle;
    device->CreateDepthStencilView(
        rsc_.Get(), nullptr, customedDSVHandle);
}

void DepthStencilBuffer::initialize(
    ID3D12Device             *device,
    int                       width,
    int                       height,
    DXGI_FORMAT               format,
    D3D12_DEPTH_STENCIL_VALUE expectedClearValue,
    const DXGI_SAMPLE_DESC   &sampleDesc)
{
    rsc_ = createDepthStencilBuffer(
        device, width, height, format, expectedClearValue, sampleDesc);

    ownedDSVHeap_ = std::make_unique<RawDescriptorHeap>(
        device, 1, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, false);
    device->CreateDepthStencilView(
        rsc_.Get(), nullptr, ownedDSVHeap_->getCPUHandle(0));
}

bool DepthStencilBuffer::isAvailable() const noexcept
{
    return rsc_ != nullptr;
}

void DepthStencilBuffer::destroy()
{
    rsc_.Reset();
    ownedDSVHeap_.reset();
    customedDSVHandle_ = { 0 };

}

ID3D12Resource *DepthStencilBuffer::getResource() const noexcept
{
    return rsc_.Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilBuffer::getDSVHandle() const noexcept
{
    return ownedDSVHeap_ ? ownedDSVHeap_->getCPUHandle(0) : customedDSVHandle_;
}

AGZ_D3D12_END
