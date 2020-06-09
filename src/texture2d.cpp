#include <d3dx12.h>

#include <agz/d3d12/texture/texture2d.h>

AGZ_D3D12_BEGIN

Texture2D::ShaderResourceInitData::ShaderResourceInitData() noexcept
    : ShaderResourceInitData(nullptr, 0)
{

}

Texture2D::ShaderResourceInitData::ShaderResourceInitData(
    const void                *initData,
    size_t                     initDataRowSize) noexcept
    : initData(initData),
      initDataRowSize(initDataRowSize)
{

}

Texture2D::Texture2D()
{
    device_ = nullptr;
    state_ = D3D12_RESOURCE_STATE_COMMON;
}

Texture2D::Texture2D(Texture2D &&other) noexcept
    : Texture2D()
{
    swap(other);
}

Texture2D &Texture2D::operator=(Texture2D &&other) noexcept
{
    swap(other);
    return *this;
}

void Texture2D::swap(Texture2D &other) noexcept
{
    std::swap(device_, other.device_);
    std::swap(rsc_,    other.rsc_);
    std::swap(state_,  other.state_);
}

void Texture2D::attach(
    ID3D12Device          *device,
    ComPtr<ID3D12Resource> rsc,
    D3D12_RESOURCE_STATES  state)
{
    destroy();

    device_ = device;
    rsc_    = std::move(rsc);
    state_  = state;
}

ComPtr<ID3D12Resource> Texture2D::initializeShaderResource(
    ID3D12Device              *device,
    ID3D12GraphicsCommandList *copyCmdList,
    int                        width,
    int                        height,
    DXGI_FORMAT                format,
    const ShaderResourceInitData &initData)
{
    return initialize(
        device, copyCmdList, width, height,
        1, 1, format, &initData, NoRTDSInfo{});
}

ComPtr<ID3D12Resource> Texture2D::initializeShaderResource(
    ID3D12Device                 *device,
    ID3D12GraphicsCommandList    *copyCmdList,
    int                           width,
    int                           height,
    int                           arraySize,
    int                           mipmapCount,
    DXGI_FORMAT                   format,
    const ShaderResourceInitData *initData)
{
    return initialize(
        device, copyCmdList, width, height,
        arraySize, mipmapCount, format, initData, NoRTDSInfo{});
}

void Texture2D::initializeRenderTarget(
    ID3D12Device           *device,
    int                     width,
    int                     height,
    DXGI_FORMAT             format,
    const RenderTargetInfo &rtInfo)
{
    initialize(device, nullptr, width, height, 1, 1, format, nullptr, rtInfo);
}

void Texture2D::initializeDepthStencil(
    ID3D12Device           *device,
    int                     width,
    int                     height,
    DXGI_FORMAT             format,
    const DepthStencilInfo &dsInfo)
{
    initialize(device, nullptr, width, height, 1, 1, format, nullptr, dsInfo);
}

bool Texture2D::isAvailable() const noexcept
{
    return rsc_ != nullptr;
}

void Texture2D::destroy()
{
    device_ = nullptr;
    rsc_.Reset();
    state_ = D3D12_RESOURCE_STATE_COMMON;
}

void Texture2D::setState(D3D12_RESOURCE_STATES state) noexcept
{
    state_ = state;
}

void Texture2D::transit(
    ID3D12GraphicsCommandList *cmdList, 
    D3D12_RESOURCE_STATES      newState)
{
    if(newState != state_)
    {
        const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            rsc_.Get(),
            state_,
            newState);
        cmdList->ResourceBarrier(1, &barrier);
        state_ = newState;
    }
}

ID3D12Resource *Texture2D::getResource() const noexcept
{
    return rsc_.Get();
}

D3D12_RESOURCE_STATES Texture2D::getState() const noexcept
{
    return state_;
}

void Texture2D::createShaderResourceView(
    D3D12_CPU_DESCRIPTOR_HANDLE srvHandle,
    int                         baseMipmapIdx,
    int                         mipmapCount,
    int                         arrayOffset,
    int                         arrayCount) const
{
    const auto rscDesc = rsc_->GetDesc();

    const UINT mipmapLevels = mipmapCount < 0 ?
                              UINT(-1):
                              (std::min)(UINT(mipmapCount),
                                         UINT(rscDesc.MipLevels - baseMipmapIdx));

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
    srvDesc.Format                  = rscDesc.Format;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

    if(arrayCount < 2)
    {
        srvDesc.ViewDimension                 = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MostDetailedMip     = baseMipmapIdx;
        srvDesc.Texture2D.MipLevels           = mipmapLevels;
        srvDesc.Texture2D.PlaneSlice          = 0;
        srvDesc.Texture2D.ResourceMinLODClamp = 0;
    }
    else
    {
        srvDesc.ViewDimension                      = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
        srvDesc.Texture2DArray.MostDetailedMip     = baseMipmapIdx;
        srvDesc.Texture2DArray.MipLevels           = mipmapLevels;
        srvDesc.Texture2DArray.FirstArraySlice     = arrayOffset;
        srvDesc.Texture2DArray.ArraySize           = arrayCount;
        srvDesc.Texture2DArray.PlaneSlice          = 0;
        srvDesc.Texture2DArray.ResourceMinLODClamp = 0;
    }

    device_->CreateShaderResourceView(
        rsc_.Get(), &srvDesc, srvHandle);
}

void Texture2D::createRenderTargetView(
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle) const
{
    device_->CreateRenderTargetView(
        rsc_.Get(), nullptr, rtvHandle);
}

void Texture2D::createDepthStencilView(
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle) const
{
    device_->CreateDepthStencilView(
        rsc_.Get(), nullptr, dsvHandle);
}

template<typename RTDSInfo>
ComPtr<ID3D12Resource> Texture2D::initialize(
    ID3D12Device                 *device,
    ID3D12GraphicsCommandList    *copyCmdList,
    int                           width,
    int                           height,
    int                           arraySize,
    int                           mipmapCount,
    DXGI_FORMAT                   format,
    const ShaderResourceInitData *initData,
    const RTDSInfo               &rtdsInfo)
{
    destroy();
    misc::scope_guard_t destroyGuard([&] { destroy(); });

    constexpr bool IS_RT = std::is_same_v<RTDSInfo, RenderTargetInfo>;
    constexpr bool IS_DS = std::is_same_v<RTDSInfo, DepthStencilInfo>;
    constexpr bool IS_SR = std::is_same_v<RTDSInfo, NoRTDSInfo>;

    device_ = device;

    // resource desc

    D3D12_RESOURCE_DESC rscDesc;
    rscDesc.Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    rscDesc.Alignment        = 0;
    rscDesc.Width            = UINT(width);
    rscDesc.Height           = UINT(height);
    rscDesc.DepthOrArraySize = arraySize;
    rscDesc.MipLevels        = mipmapCount;
    rscDesc.Format           = format;
    rscDesc.Layout           = D3D12_TEXTURE_LAYOUT_UNKNOWN;

    if constexpr(IS_RT || IS_DS)
    {
        rscDesc.SampleDesc = {
            UINT(rtdsInfo.sampleCount),
            UINT(rtdsInfo.sampleQuality)
        };
    }
    else
        rscDesc.SampleDesc = { 1, 0 };

    if constexpr(IS_RT)
        rscDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    else if constexpr(IS_DS)
        rscDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    else
        rscDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    // resource heap

    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);

    // clear value

    D3D12_CLEAR_VALUE clearValue;
    if constexpr(IS_RT)
    {
        clearValue.Format = format;
        std::memcpy(clearValue.Color, rtdsInfo.clearColor, sizeof(float) * 4);
    }
    else if constexpr(IS_DS)
    {
        clearValue.Format = format;
        clearValue.DepthStencil.Depth = rtdsInfo.clearDepth;
        clearValue.DepthStencil.Stencil = rtdsInfo.clearStencil;
    }

    // initial state

    D3D12_RESOURCE_STATES initState;
    if constexpr(IS_RT)
    {
        initState = D3D12_RESOURCE_STATE_RENDER_TARGET;
        state_ = D3D12_RESOURCE_STATE_RENDER_TARGET;
    }
    else if constexpr(IS_DS)
    {
        initState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
        state_ = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    }
    else
    {
        initState = D3D12_RESOURCE_STATE_COPY_DEST;
        state_ = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    }

    // create resource

    AGZ_D3D12_CHECK_HR(device->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &rscDesc,
        initState, IS_SR ? nullptr : &clearValue,
        IID_PPV_ARGS(rsc_.GetAddressOf())));

    if constexpr(!IS_SR)
    {
        destroyGuard.dismiss();
        return {};
    }

    assert(initData);

    // texel size in initData

    UINT texelSize;
    switch(format)
    {
    case DXGI_FORMAT_R8G8B8A8_UNORM:     texelSize = 4;  break;
    case DXGI_FORMAT_R32G32B32_FLOAT:    texelSize = 12; break;
    case DXGI_FORMAT_R32G32B32A32_FLOAT: texelSize = 16; break;
    default:
        throw D3D12LabException(
            "unsupported shader resource format: " +
            std::to_string(format));
    }

    // subresource data

    std::vector<D3D12_SUBRESOURCE_DATA> allSubrscData;

    for(int arrIdx = 0; arrIdx < arraySize; ++arrIdx)
    {
        for(int mipIdx = 0; mipIdx < mipmapCount; ++mipIdx)
        {
            const int subrscIdx = arrIdx * mipmapCount + mipIdx;
            auto &iData = initData[subrscIdx];

            const int mipWidth  = (std::max)(1, width >> mipIdx);
            const int mipHeight = (std::max)(1, height >> mipIdx);

            const UINT initDataRowSize = iData.initDataRowSize ?
                                         UINT(iData.initDataRowSize) :
                                         UINT(texelSize * mipWidth);

            D3D12_SUBRESOURCE_DATA subrscData;
            subrscData.pData      = iData.initData;
            subrscData.RowPitch   = initDataRowSize;
            subrscData.SlicePitch = initDataRowSize * mipHeight;

            allSubrscData.push_back(subrscData);
        }
    }

    // upload heap

    const UINT subrscCnt = UINT(arraySize * mipmapCount);
    const UINT64 uploadSize = GetRequiredIntermediateSize(
        rsc_.Get(), 0, UINT(arraySize * mipmapCount));
    const auto uploadHeapDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadSize);

    CD3DX12_HEAP_PROPERTIES uploadHeapProps(D3D12_HEAP_TYPE_UPLOAD);

    ComPtr<ID3D12Resource> uploadHeap;
    AGZ_D3D12_CHECK_HR(
        device->CreateCommittedResource(
            &uploadHeapProps, D3D12_HEAP_FLAG_NONE, &uploadHeapDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(uploadHeap.GetAddressOf())));

    // upload data

    UpdateSubresources(
        copyCmdList, rsc_.Get(), uploadHeap.Get(),
        0, 0, subrscCnt, allSubrscData.data());

    const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        rsc_.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    copyCmdList->ResourceBarrier(1, &barrier);

    destroyGuard.dismiss();
    return uploadHeap;
}

AGZ_D3D12_END
