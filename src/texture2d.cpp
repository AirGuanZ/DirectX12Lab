#include <d3dx12.h>

#include <agz/d3d12/texture/texture2d.h>

AGZ_D3D12_BEGIN

Texture2D::Texture2D()
{
    device_ = nullptr;
    state_  = D3D12_RESOURCE_STATE_COMMON;
}

void Texture2D::attach(
    ID3D12Device          *device,
    ComPtr<ID3D12Resource> rsc,
    D3D12_RESOURCE_STATES  state)
{
    destroy();

    device_ = device;
    rsc_    = rsc;
    state_  = state;
}

ComPtr<ID3D12Resource> Texture2D::initializeShaderResource(
    ID3D12Device              *device,
    DXGI_FORMAT                format,
    int                        width,
    int                        height,
    ID3D12GraphicsCommandList *cmdList,
    const SubresourceInitData &initData)
{
    const InitData iD = { cmdList, &initData };
    return initializeShaderResource(
        device, format, width, height, 1, 1, false, false, iD);
}

void Texture2D::initializeDefault(
    ID3D12Device            *device,
    DXGI_FORMAT              format,
    int                      width,
    int                      height,
    int                      arraySize,
    int                      mipmapCount,
    SpecialUsage             specialUsage,
    int                      sampleCount,
    int                      sampleQuality,
    D3D12_RESOURCE_STATES    initStates,
    const D3D12_CLEAR_VALUE *clearValue)
{
    destroy();

    D3D12_RESOURCE_FLAGS createFlags = D3D12_RESOURCE_FLAG_NONE;
    switch(specialUsage)
    {
    case None:
        createFlags = D3D12_RESOURCE_FLAG_NONE;
        break;
    case UnorderedAccess:
        createFlags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        break;
    case DepthStencil:
        createFlags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        break;
    case RenderTarget:
        createFlags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        break;
    }

    const auto desc = CD3DX12_RESOURCE_DESC::Tex2D(
        format, width, height, arraySize, mipmapCount,
        sampleCount, sampleQuality, createFlags);

    device->CreateCommittedResource(
        get_temp_ptr(CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT)),
        D3D12_HEAP_FLAG_NONE, &desc, initStates, clearValue,
        IID_PPV_ARGS(rsc_.GetAddressOf()));

    device_ = device;
    state_ = initStates;
}

ComPtr<ID3D12Resource> Texture2D::initializeShaderResource(
    ID3D12Device   *device,
    DXGI_FORMAT     format,
    int             width,
    int             height,
    int             arraySize,
    int             mipmapCount,
    bool            allowUAV,
    bool            withUAVState,
    const InitData &initData)
{
    return initialize(
        device, format,
        width, height, arraySize, mipmapCount, 1, 0,
        initData, allowUAV ? UnorderedAccess : None,
        CD3DX12_CLEAR_VALUE(),
        withUAVState ? D3D12_RESOURCE_STATE_UNORDERED_ACCESS :
                       D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void Texture2D::initializeDepthStencil(
    ID3D12Device *device,
    DXGI_FORMAT   format,
    int           width,
    int           height,
    int           sampleCount,
    int           sampleQuality,
    float         expectedClearDepth,
    UINT8         expectedClearStencil)
{
    initialize(
        device, format, width, height,
        1, 1, sampleCount, sampleQuality, {}, DepthStencil,
        CD3DX12_CLEAR_VALUE(format, expectedClearDepth, expectedClearStencil),
        D3D12_RESOURCE_STATE_DEPTH_WRITE);
}

void Texture2D::initializeRenderTarget(
    ID3D12Device        *device,
    DXGI_FORMAT          format,
    int                  width,
    int                  height,
    int                  sampleCount,
    int                  sampleQuality,
    const math::color4f &expectedClearColor)
{
    initialize(
        device, format, width, height,
        1, 1, sampleCount, sampleQuality, {}, RenderTarget,
        CD3DX12_CLEAR_VALUE(format, &expectedClearColor.r),
        D3D12_RESOURCE_STATE_RENDER_TARGET);
}

ComPtr<ID3D12Resource> Texture2D::initialize(
    ID3D12Device            *device,
    DXGI_FORMAT              texelFormat,
    int                      width,
    int                      height,
    int                      arraySize,
    int                      mipmapCount,
    int                      sampleCount,
    int                      sampleQuality,
    const InitData          &initData,
    SpecialUsage             specialUsage,
    const D3D12_CLEAR_VALUE &expectedClearValue,
    D3D12_RESOURCE_STATES    initState)
{
    destroy();
    misc::scope_guard_t destroyGuard([&] { destroy(); });

    device_ = device;

    D3D12_RESOURCE_DESC desc;
    desc.Dimension          = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Alignment          = 0;
    desc.Width              = UINT(width);
    desc.Height             = UINT(height);
    desc.DepthOrArraySize   = arraySize;
    desc.MipLevels          = UINT16(mipmapCount);
    desc.Format             = texelFormat;
    desc.Layout             = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.SampleDesc.Count   = UINT(sampleCount);
    desc.SampleDesc.Quality = UINT(sampleQuality);
    
    switch(specialUsage)
    {
    case RenderTarget:
        desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET; break;
    case DepthStencil:
        desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL; break;
    case UnorderedAccess:
        desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS; break;
    case None:
        desc.Flags = D3D12_RESOURCE_FLAG_NONE; break;
    }

    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);

    const D3D12_CLEAR_VALUE *clearValue = (specialUsage == RenderTarget ||
                                           specialUsage == DepthStencil) ?
                                          &expectedClearValue : nullptr;

    D3D12_RESOURCE_STATES createState;
    if(specialUsage == RenderTarget)
    {
        assert(initState == D3D12_RESOURCE_STATE_RENDER_TARGET);
        createState = D3D12_RESOURCE_STATE_RENDER_TARGET;
    }
    else if(specialUsage == DepthStencil)
    {
        assert(initState == D3D12_RESOURCE_STATE_DEPTH_WRITE);
        createState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    }
    else if(initData.subrscInitData)
    {
        assert(initState == D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE ||
               initState == D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        createState = D3D12_RESOURCE_STATE_COPY_DEST;
    }
    else
    {
        assert(initState == D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE ||
               initState == D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        createState = initState;
    }

    AGZ_D3D12_CHECK_HR(
        device->CreateCommittedResource(
            &heapProps, D3D12_HEAP_FLAG_NONE, &desc,
            createState, clearValue,
            IID_PPV_ARGS(rsc_.GetAddressOf())));

    ComPtr<ID3D12Resource> uploadHeap;

    if(initData.subrscInitData)
    {
        // texel size in initData

        UINT texelSize;
        switch(texelFormat)
        {
        case DXGI_FORMAT_R8G8B8A8_UNORM:     texelSize = 4;  break;
        case DXGI_FORMAT_R32G32B32_FLOAT:    texelSize = 12; break;
        case DXGI_FORMAT_R32G32B32A32_FLOAT: texelSize = 16; break;
        default:
            throw D3D12LabException(
                "unsupported shader resource format: " +
                std::to_string(texelFormat));
        }

        // subresource data

        std::vector<D3D12_SUBRESOURCE_DATA> allSubrscData;

        for(int arrIdx = 0; arrIdx < arraySize; ++arrIdx)
        {
            for(int mipIdx = 0; mipIdx < mipmapCount; ++mipIdx)
            {
                const int subrscIdx = arrIdx * mipmapCount + mipIdx;
                auto &iData = initData.subrscInitData[subrscIdx];

                const int mipWidth  = (std::max)(1, width >> mipIdx);
                const int mipHeight = (std::max)(1, height >> mipIdx);

                const UINT initDataRowSize = iData.rowSize ?
                                             UINT(iData.rowSize) :
                                             UINT(texelSize * mipWidth);

                D3D12_SUBRESOURCE_DATA subrscData;
                subrscData.pData      = iData.data;
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

        AGZ_D3D12_CHECK_HR(
            device->CreateCommittedResource(
                &uploadHeapProps, D3D12_HEAP_FLAG_NONE, &uploadHeapDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                IID_PPV_ARGS(uploadHeap.GetAddressOf())));

        // upload data

        UpdateSubresources(
            initData.copyCmdList, rsc_.Get(), uploadHeap.Get(),
            0, 0, subrscCnt, allSubrscData.data());

        const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            rsc_.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST,
            initState);

        initData.copyCmdList->ResourceBarrier(1, &barrier);
    }

    state_ = initState;

    destroyGuard.dismiss();
    return uploadHeap;
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

ID3D12Resource *Texture2D::getResource() const noexcept
{
    return rsc_.Get();
}

D3D12_RESOURCE_STATES Texture2D::getState() const noexcept
{
    return state_;
}

void Texture2D::setState(D3D12_RESOURCE_STATES state) noexcept
{
    state_ = state;
}

void Texture2D::transit(
    ID3D12GraphicsCommandList *cmdList, D3D12_RESOURCE_STATES newState)
{
    if(state_ != newState)
    {
        cmdList->ResourceBarrier(
            1, get_temp_ptr(CD3DX12_RESOURCE_BARRIER::Transition(
                rsc_.Get(), state_, newState)));
        state_ = newState;
    }
}

void Texture2D::createShaderResourceView(
    D3D12_CPU_DESCRIPTOR_HANDLE srvHandle,
    int                         baseMipmapIdx,
    int                         mipmapCount,
    int                         arrayStart,
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
        srvDesc.Texture2DArray.FirstArraySlice     = arrayStart;
        srvDesc.Texture2DArray.ArraySize           = arrayCount;
        srvDesc.Texture2DArray.PlaneSlice          = 0;
        srvDesc.Texture2DArray.ResourceMinLODClamp = 0;
    }

    device_->CreateShaderResourceView(
        rsc_.Get(), &srvDesc, srvHandle);
}

void Texture2D::createRenderTargetView(
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle,
    int                         mipmapIdx) const
{
    const auto rscDesc = rsc_->GetDesc();

    D3D12_RENDER_TARGET_VIEW_DESC desc;
    desc.Format = rscDesc.Format;

    if(rscDesc.SampleDesc.Count == 1)
    {
        desc.ViewDimension        = D3D12_RTV_DIMENSION_TEXTURE2D;
        desc.Texture2D.MipSlice   = mipmapIdx;
        desc.Texture2D.PlaneSlice = 0;
    }
    else
        desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;

    device_->CreateRenderTargetView(rsc_.Get(), &desc, rtvHandle);
}

void Texture2D::createDepthStencilView(
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle,
    int                         mipmapIdx) const
{
    const auto rscDesc = rsc_->GetDesc();

    D3D12_DEPTH_STENCIL_VIEW_DESC desc;
    desc.Format = rscDesc.Format;
    desc.Flags = D3D12_DSV_FLAG_NONE;

    if(rscDesc.SampleDesc.Count == 1)
    {
        desc.ViewDimension      = D3D12_DSV_DIMENSION_TEXTURE2D;
        desc.Texture2D.MipSlice = mipmapIdx;
        
    }
    else
        desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;

    device_->CreateDepthStencilView(rsc_.Get(), &desc, dsvHandle);
}

void Texture2D::createUnorderedAccessView(
    D3D12_CPU_DESCRIPTOR_HANDLE uavHandle,
    int                         mipmapIdx,
    int                         arrayIdx,
    int                         arraySize) const
{
    const auto rscDesc = rsc_->GetDesc();

    D3D12_UNORDERED_ACCESS_VIEW_DESC desc;
    desc.Format = rscDesc.Format;

    if(arrayIdx == 0 && arraySize == 1)
    {
        desc.ViewDimension        = D3D12_UAV_DIMENSION_TEXTURE2D;
        desc.Texture2D.MipSlice   = mipmapIdx;
        desc.Texture2D.PlaneSlice = 0;
    }
    else
    {
        desc.ViewDimension                  = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
        desc.Texture2DArray.MipSlice        = mipmapIdx;
        desc.Texture2DArray.PlaneSlice      = 0;
        desc.Texture2DArray.ArraySize       = arraySize;
        desc.Texture2DArray.FirstArraySlice = arrayIdx;
    }

    device_->CreateUnorderedAccessView(
        rsc_.Get(), nullptr, &desc, uavHandle);
}

std::pair<int, int> Texture2D::getMultisample() const noexcept
{
    const auto desc = rsc_->GetDesc();
    return {
        int(desc.SampleDesc.Count),
        int(desc.SampleDesc.Quality)
    };
}

AGZ_D3D12_END
