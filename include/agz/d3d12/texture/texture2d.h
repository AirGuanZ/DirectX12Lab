#pragma once

#include <d3d12.h>

#include <agz/d3d12/common.h>
#include <agz/utility/texture.h>

AGZ_D3D12_BEGIN

class Texture2D
{
public:

    enum SpecialUsage
    {
        None,
        RenderTarget,
        DepthStencil,
        UnorderedAccess
    };

    struct SubresourceInitData
    {
        SubresourceInitData(
            const void *data = nullptr, size_t rowSize = 0) noexcept
            : data(data), rowSize(rowSize)
        {
            
        }

        const void *data;
        size_t rowSize;
    };

    struct InitData
    {
        ID3D12GraphicsCommandList *copyCmdList = nullptr;
        const SubresourceInitData *subrscInitData = nullptr;
    };

    Texture2D();

    void attach(
        ID3D12Device          *device,
        ComPtr<ID3D12Resource> rsc,
        D3D12_RESOURCE_STATES  state);
    
    ComPtr<ID3D12Resource> initializeShaderResource(
        ID3D12Device              *device,
        DXGI_FORMAT                format,
        int                        width,
        int                        height,
        ID3D12GraphicsCommandList *cmdList,
        const SubresourceInitData &initData);
    
    ComPtr<ID3D12Resource> initializeShaderResource(
        ID3D12Device   *device,
        DXGI_FORMAT     format,
        int             width,
        int             height,
        int             arraySize,
        int             mipmapCount,
        bool            allowUAV,
        bool            withUAVState,
        const InitData &initData);

    void initializeDepthStencil(
        ID3D12Device *device,
        DXGI_FORMAT   format,
        int           width,
        int           height,
        int           sampleCount,
        int           sampleQuality,
        float         expectedClearDepth,
        UINT8         expectedClearStencil);

    void initializeRenderTarget(
        ID3D12Device        *device,
        DXGI_FORMAT          format,
        int                  width,
        int                  height,
        int                  sampleCount,
        int                  sampleQuality,
        const math::color4f &expectedClearColor);
    
    ComPtr<ID3D12Resource> initialize(
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
        D3D12_RESOURCE_STATES    initState);

    bool isAvailable() const noexcept;

    void destroy();

    ID3D12Resource *getResource() const noexcept;

    D3D12_RESOURCE_STATES getState() const noexcept;

    void setState(D3D12_RESOURCE_STATES state) noexcept;

    void transit(
        ID3D12GraphicsCommandList *cmdList,
        D3D12_RESOURCE_STATES      newState);

    void createShaderResourceView(
        D3D12_CPU_DESCRIPTOR_HANDLE srvHandle,
        int                         baseMipmapIdx = 0,
        int                         mipmapCount   = -1,
        int                         arrayStart    = 0,
        int                         arrayCount    = 1) const;

    void createRenderTargetView(
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle,
        int                         mipmapIdx = 0) const;

    void createDepthStencilView(
        D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle,
        int                         mipmapIdx = 0) const;

    void createUnorderedAccessView(
        D3D12_CPU_DESCRIPTOR_HANDLE uavHandle,
        int                         mipmapIdx = 0,
        int                         arrayIdx  = 0,
        int                         arraySize = 1) const;

    std::pair<int, int> getMultisample() const noexcept;

private:

    ID3D12Device *device_;

    ComPtr<ID3D12Resource> rsc_;

    D3D12_RESOURCE_STATES state_;
};

AGZ_D3D12_END
