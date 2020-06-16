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

struct MipSlice
{
    UINT slice = 0;
};

// write 'RENDER_TARGET' rsc
struct RenderTarget
{
    // Args: ClearColor
    //       MipSlice
    template<typename...Args>
    RenderTarget(ResourceIndex rsc, Args&&...args) noexcept;

    ResourceIndex rsc;

    bool clear;
    ClearColor clearValue;

    UINT mipSlice;
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

namespace passrsc
{

    struct MSAAAResolveSrc
    {
        ResourceIndex rsc;
    };

    struct MSAAResolveDst
    {
        ResourceIndex rsc;
    };

    // read 'PIXEL_SHADER_RESOURCE' rsc with SRV
    struct PixelShaderResource
    {
        ResourceIndex rsc;
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
    };

    // read 'NONPIXEL_SHADER_RESOURCE' rsc with SRV
    struct NonPixelShaderResource
    {
        ResourceIndex rsc;
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
    };

    // read 'PIXEL & NOPIXEL SHADER_RESOURCE' rsc with SRV
    struct ShaderResource
    {
        ResourceIndex rsc;
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
    };

    // write 'UNORDERED_ACCESS' rsc with UAV
    struct UnorderedAccess
    {
        ResourceIndex rsc = RESOURCE_NIL;
        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc;
    };

} // namespace passrsc

namespace detail
{

    inline void _initRT(RenderTarget &rt, const ClearColor &clearColor) noexcept
    {
        rt.clear      = true;
        rt.clearValue = clearColor;
    }

    inline void _initRT(RenderTarget &rt, const MipSlice &mipSlice) noexcept
    {
        rt.mipSlice = mipSlice.slice;
    }

} // namespace detail

template<typename ... Args>
RenderTarget::RenderTarget(ResourceIndex rsc, Args &&... args) noexcept
    : rsc(rsc), clear(false), mipSlice(0)
{
    InvokeAll([&] { detail::_initRT(*this, std::forward<Args>(args)); }...);
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
