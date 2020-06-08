#pragma once

#include <d3d12.h>

#include <agz/d3d12/common.h>
#include <agz/utility/texture.h>

AGZ_D3D12_BEGIN

struct LoadTextureResult
{
    ComPtr<ID3D12Resource> textureResource;
    ComPtr<ID3D12Resource> uploadResource;
};

LoadTextureResult loadTexture2DFromMemory(
    ID3D12Device                              *device,
    ID3D12GraphicsCommandList                 *cmdList,
    const texture::texture2d_t<math::color4b> &data);

class Texture2D : public misc::uncopyable_t
{
public:

    enum class InitialState
    {
        PixelShaderResource,
        RenderTarget
    };

    struct InitData
    {
        InitData() noexcept;

        InitData(
            ID3D12GraphicsCommandList *copyCmdList,
            const void                *initData,
            size_t                     initDataRowSize = 0) noexcept;

        ID3D12GraphicsCommandList *copyCmdList;
        const void *initData;

        size_t initDataRowSize;
    };

    struct RenderTargetInfo
    {
        float clearColor[4] = { 0, 0, 0, 0 };
    };

    struct DepthStencilInfo
    {
        float clearDepth   = 1;
        UINT8 clearStencil = 0;
    };

    Texture2D();

    Texture2D(Texture2D &&other) noexcept;

    Texture2D &operator=(Texture2D &&other) noexcept;

    void swap(Texture2D &other) noexcept;

    void attach(
        ID3D12Device          *device,
        ComPtr<ID3D12Resource> rsc,
        D3D12_RESOURCE_STATES  state);

    ComPtr<ID3D12Resource> initializeShaderResource(
        ID3D12Device   *device,
        int             width,
        int             height,
        DXGI_FORMAT     format,
        const InitData &initData);

    void initializeRenderTarget(
        ID3D12Device           *device,
        int                     width,
        int                     height,
        DXGI_FORMAT             format,
        const RenderTargetInfo &rtInfo);

    void initializeDepthStencil(
        ID3D12Device           *device,
        int                     width,
        int                     height,
        DXGI_FORMAT             format,
        const DepthStencilInfo &dsInfo);

    bool isAvailable() const noexcept;

    void destroy();

    void setState(D3D12_RESOURCE_STATES state) noexcept;

    void transit(
        ID3D12GraphicsCommandList *cmdList,
        D3D12_RESOURCE_STATES newState);

    ID3D12Resource *getResource() const noexcept;

    D3D12_RESOURCE_STATES getState() const noexcept;

    void createShaderResourceView(D3D12_CPU_DESCRIPTOR_HANDLE srvHandle) const;

    void createRenderTargetView(D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle) const;

    void createDepthStencilView(D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle) const;

private:

    struct NoRTDSInfo { };

    template<typename RTDSInfo>
    ComPtr<ID3D12Resource> initialize(
        ID3D12Device      *device,
        int                width,
        int                height,
        DXGI_FORMAT        format,
        const InitData    &initData,
        const RTDSInfo    &rtdsInfo);

    ID3D12Device *device_;

    ComPtr<ID3D12Resource> rsc_;

    D3D12_RESOURCE_STATES state_;
};

inline Texture2D::InitData::InitData() noexcept
    : InitData(nullptr, nullptr, 0)
{

}

inline Texture2D::InitData::InitData(
    ID3D12GraphicsCommandList *copyCmdList,
    const void                *initData,
    size_t                     initDataRowSize) noexcept
    : copyCmdList(copyCmdList),
      initData(initData),
      initDataRowSize(initDataRowSize)
{

}

inline Texture2D::Texture2D()
{
    device_ = nullptr;
    state_ = D3D12_RESOURCE_STATE_COMMON;
}

inline Texture2D::Texture2D(Texture2D &&other) noexcept
    : Texture2D()
{
    swap(other);
}

inline Texture2D &Texture2D::operator=(Texture2D &&other) noexcept
{
    swap(other);
    return *this;
}

inline void Texture2D::swap(Texture2D &other) noexcept
{
    std::swap(device_, other.device_);
    std::swap(rsc_,    other.rsc_);
    std::swap(state_,  other.state_);
}

inline void Texture2D::attach(
    ID3D12Device          *device,
    ComPtr<ID3D12Resource> rsc,
    D3D12_RESOURCE_STATES  state)
{
    destroy();

    device_ = device;
    rsc_    = std::move(rsc);
    state_  = state;
}

inline ComPtr<ID3D12Resource> Texture2D::initializeShaderResource(
    ID3D12Device   *device,
    int             width,
    int             height,
    DXGI_FORMAT     format,
    const InitData &initData)
{
    return initialize(device, width, height, format, initData, NoRTDSInfo{});
}

inline void Texture2D::initializeRenderTarget(
    ID3D12Device           *device,
    int                     width,
    int                     height,
    DXGI_FORMAT             format,
    const RenderTargetInfo &rtInfo)
{
    initialize(device, width, height, format, {}, rtInfo);
}

inline void Texture2D::initializeDepthStencil(
    ID3D12Device           *device,
    int                     width,
    int                     height,
    DXGI_FORMAT             format,
    const DepthStencilInfo &dsInfo)
{
    initialize(device, width, height, format, {}, dsInfo);
}

inline bool Texture2D::isAvailable() const noexcept
{
    return rsc_ != nullptr;
}

inline void Texture2D::destroy()
{
    device_ = nullptr;
    rsc_.Reset();
    state_ = D3D12_RESOURCE_STATE_COMMON;
}

inline void Texture2D::setState(D3D12_RESOURCE_STATES state) noexcept
{
    state_ = state;
}

inline void Texture2D::transit(
    ID3D12GraphicsCommandList *cmdList, 
    D3D12_RESOURCE_STATES newState)
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

inline ID3D12Resource *Texture2D::getResource() const noexcept
{
    return rsc_.Get();
}

inline D3D12_RESOURCE_STATES Texture2D::getState() const noexcept
{
    return state_;
}

inline void Texture2D::createShaderResourceView(
    D3D12_CPU_DESCRIPTOR_HANDLE srvHandle) const
{
    const auto rscDesc = rsc_->GetDesc();
    
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
    srvDesc.Format                        = rscDesc.Format;
    srvDesc.ViewDimension                 = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping       = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MostDetailedMip     = 0;
    srvDesc.Texture2D.MipLevels           = rscDesc.MipLevels;
    srvDesc.Texture2D.PlaneSlice          = 0;
    srvDesc.Texture2D.ResourceMinLODClamp = 0;

    device_->CreateShaderResourceView(
        rsc_.Get(), &srvDesc, srvHandle);
}

inline void Texture2D::createRenderTargetView(
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle) const
{
    device_->CreateRenderTargetView(
        rsc_.Get(), nullptr, rtvHandle);
}

inline void Texture2D::createDepthStencilView(
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle) const
{
    device_->CreateDepthStencilView(
        rsc_.Get(), nullptr, dsvHandle);
}

template<typename RTDSInfo>
ComPtr<ID3D12Resource> Texture2D::initialize(
    ID3D12Device   *device,
    int             width,
    int             height,
    DXGI_FORMAT     format,
    const InitData &initData,
    const RTDSInfo &rtdsInfo)
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
    rscDesc.DepthOrArraySize = 1;
    rscDesc.MipLevels        = 1;
    rscDesc.Format           = format;
    rscDesc.SampleDesc       = { 1, 0 };
    rscDesc.Layout           = D3D12_TEXTURE_LAYOUT_UNKNOWN;

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

    assert(initData.initData);

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

    // row size in initData

    const UINT initDataRowSize = initData.initDataRowSize ?
                                 UINT(initData.initDataRowSize) :
                                 UINT(texelSize * width);

    // upload heap
    
    const size_t rawRowSize    = width * texelSize;
    const size_t paddedRowSize = (rawRowSize + 255) & ~255;
    const size_t uploadSize    = paddedRowSize * (height - 1) + rawRowSize;

    const auto uploadHeapDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadSize);

    CD3DX12_HEAP_PROPERTIES uploadHeapProps(D3D12_HEAP_TYPE_UPLOAD);

    ComPtr<ID3D12Resource> uploadHeap;
    AGZ_D3D12_CHECK_HR(
        device->CreateCommittedResource(
            &uploadHeapProps, D3D12_HEAP_FLAG_NONE, &uploadHeapDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(uploadHeap.GetAddressOf())));

    // upload data

    D3D12_SUBRESOURCE_DATA subrscData;
    subrscData.pData      = initData.initData;
    subrscData.RowPitch   = initDataRowSize;
    subrscData.SlicePitch = initDataRowSize * height;

    UpdateSubresources(
        initData.copyCmdList, rsc_.Get(), uploadHeap.Get(), 0, 0, 1, &subrscData);

    const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        rsc_.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    initData.copyCmdList->ResourceBarrier(1, &barrier);

    destroyGuard.dismiss();
    return uploadHeap;
}

AGZ_D3D12_END
