#pragma once

AGZ_D3D12_FG_BEGIN

namespace detail
{

    template<typename S>
    void _initDSV(
        D3D12_DEPTH_STENCIL_VIEW_DESC &g, S &s,
        DXGI_FORMAT format) noexcept
    {
        g.Format = format;
    }

    template<typename S>
    void _initDSV(
        D3D12_DEPTH_STENCIL_VIEW_DESC &g, S &s,
        const MipmapSlice &mipmapSlice) noexcept
    {
        s.MipSlice = mipmapSlice.sliceIdx;
    }

} // namespace detail

inline _internalDSV::_internalDSV(ResourceIndex rsc) noexcept
    : rsc(rsc), desc{}
{
    desc.Format = DXGI_FORMAT_UNKNOWN;
}

template<typename ... Args>
Tex2DDSV::Tex2DDSV(ResourceIndex rsc, const Args &... args) noexcept
    : _internalDSV(rsc)
{
    desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    desc.Texture2D     = { 0 };
    InvokeAll([&] { detail::_initDSV(desc, desc.Texture2D, args); }...);
}

template<typename ... Args>
Tex2DMSDSV::Tex2DMSDSV(ResourceIndex rsc, const Args &... args) noexcept
    : _internalDSV(rsc)
{
    desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;
    InvokeAll([&] { detail::_initDSV(desc, desc.Texture2DMS, args); }...);
}

AGZ_D3D12_FG_END
