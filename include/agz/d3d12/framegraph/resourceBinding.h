#pragma once

#include <d3d12.h>

#include <agz/d3d12/framegraph/common.h>

AGZ_D3D12_FG_BEGIN

struct ClearColor
{
    float r = 0, g = 0, b = 0, a = 0;
};

struct ClearDepthStencil
{
    float depth = 1;
    UINT8 stencil = 0;
};

// read rsc
struct InputTexture2D
{
    ResourceIndex rsc;
};

// write rsc
struct OutputTexture2D
{
    ResourceIndex rsc;
};

// read 'PIXEL_SHADER_RESOURCE' rsc
struct PixelShaderResourceTexture2D
{
    ResourceIndex rsc  = RESOURCE_NIL;
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
};

// read 'NONPIXEL_SHADER_RESOURCE' rsc
struct NonPixelShaderResourceTexture2D
{
    ResourceIndex rsc  = RESOURCE_NIL;
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
};

// read 'PIXEL & NOPIXEL SHADER_RESOURCE' rsc
struct ShaderResourceTexture2D
{
    ResourceIndex rsc  = RESOURCE_NIL;
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
};

// write 'UNORDERED_ACCESS' rsc
struct UnorderedAccessTexture2D
{
    ResourceIndex rsc = RESOURCE_NIL;
};

// write 'RENDER_TARGET' rsc
struct RenderTarget
{
    explicit RenderTarget(ResourceIndex rsc) noexcept;

    RenderTarget(ResourceIndex rsc, const ClearColor &clearValue) noexcept;

    ResourceIndex rsc;

    bool clear;
    ClearColor clearValue;
};

// write 'DEPTH_WRITE' rsc
struct DepthStencil
{
    template<typename...Args>
    explicit DepthStencil(ResourceIndex rsc, Args&&...args) noexcept;

    ResourceIndex rsc;

    // dsvFormat != UNKNOWN -> assert(externalDSV == nil),   create a dsv with format
    // externalDSV != nil   -> assert(dsvFormat == UNKNOWN), use external dsv
    // otherwise, create a dsv with format of rsc

    DXGI_FORMAT dsvFormat;
    D3D12_CPU_DESCRIPTOR_HANDLE externalDSV;

    bool clear;
    ClearDepthStencil clearValue;
};

inline RenderTarget::RenderTarget(ResourceIndex rsc) noexcept
    : rsc(rsc), clear(false)
{
    
}

inline RenderTarget::RenderTarget(
    ResourceIndex rsc, const ClearColor &clearValue) noexcept
    : rsc(rsc), clear(true), clearValue(clearValue)
{
    
}

namespace detail
{

    inline void _initDS(
        DepthStencil &ds, DXGI_FORMAT fmt) noexcept
    {
        ds.dsvFormat = fmt;
    }

    inline void _initDS(
        DepthStencil &ds, D3D12_CPU_DESCRIPTOR_HANDLE externalDSV) noexcept
    {
        ds.externalDSV = externalDSV;
    }

    inline void _initDS(
        DepthStencil &ds, const ClearDepthStencil &clearValue) noexcept
    {
        ds.clear = true;
        ds.clearValue = clearValue;
    }

} // namespace detail

template<typename...Args>
DepthStencil::DepthStencil(ResourceIndex rsc, Args &&... args) noexcept
    : rsc(rsc), dsvFormat(DXGI_FORMAT_UNKNOWN), externalDSV{ 0 }, clear(false)
{
    InvokeAll([&] { detail::_initDS(*this, std::forward<Args>(args)); }...);
}

AGZ_D3D12_FG_END
