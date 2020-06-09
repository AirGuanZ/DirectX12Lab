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

class Texture2D
{
public:

    enum class InitialState
    {
        PixelShaderResource,
        RenderTarget
    };

    struct ShaderResourceInitData
    {
        ShaderResourceInitData() noexcept;

        ShaderResourceInitData(
            const void                *initData,
            size_t                     initDataRowSize = 0) noexcept;

        const void *initData;
        size_t initDataRowSize;
    };

    struct RenderTargetInfo
    {
        float clearColor[4] = { 0, 0, 0, 0 };
        int sampleCount     = 1;
        int sampleQuality   = 0;
    };

    struct DepthStencilInfo
    {
        float clearDepth   = 1;
        UINT8 clearStencil = 0;
        int sampleCount    = 1;
        int sampleQuality  = 0;
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
        ID3D12Device                 *device,
        ID3D12GraphicsCommandList    *copyCmdList,
        int                           width,
        int                           height,
        DXGI_FORMAT                   format,
        const ShaderResourceInitData &initData);

    ComPtr<ID3D12Resource> initializeShaderResource(
        ID3D12Device                 *device,
        ID3D12GraphicsCommandList    *copyCmdList,
        int                           width,
        int                           height,
        int                           arraySize,
        int                           mipmapCount,
        DXGI_FORMAT                   format,
        const ShaderResourceInitData *initData);

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
        D3D12_RESOURCE_STATES      newState);

    ID3D12Resource *getResource() const noexcept;

    D3D12_RESOURCE_STATES getState() const noexcept;

    void createShaderResourceView(
        D3D12_CPU_DESCRIPTOR_HANDLE srvHandle,
        int baseMipmapIdx = 0,
        int mipmapCount   = -1,
        int arrayOffset   = 0,
        int arrayCount    = 1) const;

    void createRenderTargetView(D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle) const;

    void createDepthStencilView(D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle) const;

private:

    struct NoRTDSInfo { };

    template<typename RTDSInfo>
    ComPtr<ID3D12Resource> initialize(
        ID3D12Device                 *device,
        ID3D12GraphicsCommandList    *copyCmdList,
        int                           width,
        int                           height,
        int                           arraySize,
        int                           mipmapCount,
        DXGI_FORMAT                   format,
        const ShaderResourceInitData *initData,
        const RTDSInfo               &rtdsInfo);

    ID3D12Device *device_;

    ComPtr<ID3D12Resource> rsc_;

    D3D12_RESOURCE_STATES state_;
};

AGZ_D3D12_END
