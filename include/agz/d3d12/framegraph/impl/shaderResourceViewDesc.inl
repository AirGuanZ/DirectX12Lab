#pragma once

AGZ_D3D12_FG_BEGIN

namespace detail
{

    template<typename S>
    void _initTex2DSRV(
        D3D12_SHADER_RESOURCE_VIEW_DESC &g, S &s,
        const MipmapSlices &mipmapSlices) noexcept
    {
        s.MipLevels           = mipmapSlices.mipmapCount;
        s.MostDetailedMip     = mipmapSlices.mostDetailed;
        s.ResourceMinLODClamp = mipmapSlices.minLODClamp;
    }
    
    template<typename S>
    void _initTex2DSRV(
        D3D12_SHADER_RESOURCE_VIEW_DESC &g, S &s,
        const MipmapSlice &mipmapSlice) noexcept
    {
        s.MipLevels           = 1;
        s.MostDetailedMip     = mipmapSlice.sliceIdx;
    }

    template<typename S>
    void _initTex2DSRV(
        D3D12_SHADER_RESOURCE_VIEW_DESC &g, S &s,
        const ArraySlices &arraySlices) noexcept
    {
        s.FirstArraySlice = arraySlices.firstElem;
        s.ArraySize = arraySlices.elemCount;
    }

    template<typename S>
    void _initTex2DSRV(
        D3D12_SHADER_RESOURCE_VIEW_DESC &g, S &s,
        DXGI_FORMAT fmt)
    {
        g.Format = fmt;
    }

} // namespace detail

inline SRV::SRV() noexcept
    : desc{}
{
    desc.Format                  = DXGI_FORMAT_UNKNOWN;
    desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
}

template<typename ... Args>
Tex2DSRV::Tex2DSRV(const Args &... args) noexcept
{
    desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    desc.Texture2D     = { 0, UINT(-1), 0, 0 };
    InvokeAll([&] { detail::_initTex2DSRV(desc, desc.Texture2D, args); }...);
}

template<typename ... Args>
Tex2DArrSRV::Tex2DArrSRV(const Args &... args) noexcept
{
    desc.ViewDimension  = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
    desc.Texture2DArray = { 0, UINT(-1), 0, 1, 0, 0 };
    InvokeAll([&] { detail::_initTex2DSRV(desc, desc.Texture2DArray, args); }...);
}

template<typename ... Args>
Tex2DMSSRV::Tex2DMSSRV(const Args &... args) noexcept
{
    desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
    InvokeAll([&] { detail::_initTex2DSRV(desc, desc.Texture2DMS, args); }...);
}

template<typename ... Args>
Tex2DMSArrSRV::Tex2DMSArrSRV(const Args &... args) noexcept
{
    desc.ViewDimension    = D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;
    desc.Texture2DMSArray = { 0, 1 };
    InvokeAll([&] { detail::_initTex2DSRV(desc, desc.Texture2DMSArray, args); }...);
}

AGZ_D3D12_FG_END
