#pragma once

#include <agz/utility/misc.h>

AGZ_D3D12_FG_BEGIN

namespace detail
{

    template<typename S>
    void _initTex2DSRV(
        _internalSRV &g, S &s,
        const MipmapSlices &mipmapSlices) noexcept
    {
        s.MipLevels           = mipmapSlices.mipmapCount;
        s.MostDetailedMip     = mipmapSlices.mostDetailed;
        s.ResourceMinLODClamp = mipmapSlices.minLODClamp;
    }
    
    template<typename S>
    void _initTex2DSRV(
        _internalSRV &g, S &s,
        const MipmapSlice &mipmapSlice) noexcept
    {
        s.MipLevels           = 1;
        s.MostDetailedMip     = mipmapSlice.sliceIdx;
    }

    template<typename S>
    void _initTex2DSRV(
        _internalSRV &g, S &s,
        const ArraySlices &arraySlices) noexcept
    {
        s.FirstArraySlice = arraySlices.firstElem;
        s.ArraySize = arraySlices.elemCount;
    }

    template<typename S>
    void _initTex2DSRV(
        _internalSRV &g, S &s,
        DXGI_FORMAT fmt)
    {
        g.desc.Format = fmt;
    }

    template<typename S>
    void _initTex2DSRV(
        _internalSRV &g, S &s,
        SRVScope scope) noexcept
    {
        g.scope = scope;
    }

    template<typename S>
    void _initBufSRV(
        _internalSRV &g, S &s,
        SRVScope scope) noexcept
    {
        g.scope = scope;
    }

} // namespace detail

inline _internalSRV::_internalSRV(ResourceIndex rsc, SRVScope scope) noexcept
    : rsc(rsc), desc{}, scope(scope)
{
    desc.Format                  = DXGI_FORMAT_UNKNOWN;
    desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
}

inline D3D12_RESOURCE_STATES _internalSRV::getRequiredRscState() const noexcept
{
    switch(scope)
    {
    case PixelSRV:
        return D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    case NonPixelSRV:
        return D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    case PixelAndNonPixelSRV:
    case DefaultSRVScope:
        return D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
               D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    }

    misc::unreachable();
}

template<typename ... Args>
Tex2DSRV::Tex2DSRV(ResourceIndex rsc, const Args &... args) noexcept
    : _internalSRV(rsc, DefaultSRVScope)
{
    desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    desc.Texture2D     = { 0, UINT(-1), 0, 0 };
    InvokeAll([&] { detail::_initTex2DSRV(*this, desc.Texture2D, args); }...);
}

template<typename ... Args>
Tex2DArrSRV::Tex2DArrSRV(ResourceIndex rsc, const Args &... args) noexcept
    : _internalSRV(rsc, DefaultSRVScope)
{
    desc.ViewDimension  = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
    desc.Texture2DArray = { 0, UINT(-1), 0, 1, 0, 0 };
    InvokeAll([&] { detail::_initTex2DSRV(*this, desc.Texture2DArray, args); }...);
}

template<typename ... Args>
Tex2DMSSRV::Tex2DMSSRV(ResourceIndex rsc, const Args &... args) noexcept
    : _internalSRV(rsc, DefaultSRVScope)
{
    desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
    InvokeAll([&] { detail::_initTex2DSRV(*this, desc.Texture2DMS, args); }...);
}

template<typename ... Args>
Tex2DMSArrSRV::Tex2DMSArrSRV(ResourceIndex rsc, const Args &... args) noexcept
    : _internalSRV(rsc, DefaultSRVScope)
{
    desc.ViewDimension    = D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;
    desc.Texture2DMSArray = { 0, 1 };
    InvokeAll([&] { detail::_initTex2DSRV(*this, desc.Texture2DMSArray, args); }...);
}

template<typename...Args>
BufSRV::BufSRV(
    ResourceIndex rsc, UINT elemSize, UINT elemCnt,
    const Args &...args) noexcept
    : _internalSRV(rsc, DefaultSRVScope)
{
    desc.ViewDimension              = D3D12_SRV_DIMENSION_BUFFER;
    desc.Buffer.FirstElement        = 0;
    desc.Buffer.Flags               = D3D12_BUFFER_SRV_FLAG_NONE;
    desc.Buffer.NumElements         = elemCnt;
    desc.Buffer.StructureByteStride = elemSize;
    InvokeAll([&] { detail::_initBufSRV(*this, desc.Buffer, args); }...);
}

AGZ_D3D12_FG_END
