#include <d3dx12.h>

#include <agz/d3d12/texture/texture2d.h>

AGZ_D3D12_BEGIN

void Texture2D::initialize(
    ID3D12Device *device,
    DXGI_FORMAT format,
    int width, int height,
    int depth, int mipCnt,
    int sampleCnt, int sampleQua,
    D3D12_RESOURCE_FLAGS flags,
    D3D12_RESOURCE_STATES states,
    const D3D12_CLEAR_VALUE *clearValue)
{
    destroy();

    AGZ_D3D12_CHECK_HR(
        device->CreateCommittedResource(
            get_temp_ptr(CD3DX12_HEAP_PROPERTIES(
                D3D12_HEAP_TYPE_DEFAULT)),
            D3D12_HEAP_FLAG_NONE,
            get_temp_ptr(CD3DX12_RESOURCE_DESC::Tex2D(
                format, width, height, depth, mipCnt, sampleCnt, sampleQua,
                flags)),
            states, clearValue,
            IID_PPV_ARGS(rsc_.GetAddressOf())));

    device_ = device;
}

bool Texture2D::isAvailable() const noexcept
{
    return rsc_ != nullptr;
}

void Texture2D::destroy()
{
    rsc_.Reset();
    device_.Reset();
}

void Texture2D::createSRV(
    D3D12_CPU_DESCRIPTOR_HANDLE output) const
{
    createSRV(
        rsc_->GetDesc().Format, 0, 1, output);
}

void Texture2D::createSRV(
    DXGI_FORMAT format,
    int mipBeg, int mipCnt,
    D3D12_CPU_DESCRIPTOR_HANDLE output) const
{
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
    srvDesc.Format                        = format;
    srvDesc.Shader4ComponentMapping       = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension                 = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels           = mipCnt;
    srvDesc.Texture2D.MostDetailedMip     = mipBeg;
    srvDesc.Texture2D.PlaneSlice          = 0;
    srvDesc.Texture2D.ResourceMinLODClamp = 0;

    device_->CreateShaderResourceView(
        rsc_.Get(), &srvDesc, output);
}

void Texture2D::createRTV(
    D3D12_CPU_DESCRIPTOR_HANDLE output) const
{
    createRTV(
        rsc_->GetDesc().Format, 0, output);
}

void Texture2D::createRTV(
    DXGI_FORMAT format,
    int mipIdx,
    D3D12_CPU_DESCRIPTOR_HANDLE output) const
{
    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc;
    rtvDesc.Format = format;

    const auto sampleDesc = rsc_->GetDesc().SampleDesc;
    if(sampleDesc.Count > 1)
    {
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;
        rtvDesc.Texture2DMS.UnusedField_NothingToDefine = 0;
    }
    else
    {
        rtvDesc.ViewDimension        = D3D12_RTV_DIMENSION_TEXTURE2D;
        rtvDesc.Texture2D.MipSlice   = mipIdx;
        rtvDesc.Texture2D.PlaneSlice = 0;
    }

    device_->CreateRenderTargetView(
        rsc_.Get(), &rtvDesc, output);
}

void Texture2D::createUAV(
    D3D12_CPU_DESCRIPTOR_HANDLE output) const
{
    createUAV(
        rsc_->GetDesc().Format, 0, output);
}

void Texture2D::createUAV(
    DXGI_FORMAT format,
    int mipIdx,
    D3D12_CPU_DESCRIPTOR_HANDLE output) const
{
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc;
    uavDesc.Format               = format;
    uavDesc.ViewDimension        = D3D12_UAV_DIMENSION_TEXTURE2D;
    uavDesc.Texture2D.MipSlice   = mipIdx;
    uavDesc.Texture2D.PlaneSlice = 0;

    device_->CreateUnorderedAccessView(
        rsc_.Get(), nullptr, &uavDesc, output);
}

DXGI_SAMPLE_DESC Texture2D::getMultisample() const noexcept
{
    return rsc_->GetDesc().SampleDesc;
}

Texture2D::operator struct ID3D12Resource*() const noexcept
{
    return rsc_.Get();
}

Texture2D::operator ComPtr<struct ID3D12Resource>() const noexcept
{
    return rsc_;
}

AGZ_D3D12_END
