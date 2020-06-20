#pragma once

AGZ_D3D12_FG_BEGIN

namespace detail
{

    template<typename S>
    void _initRTV(
        D3D12_RENDER_TARGET_VIEW_DESC &g, S &s,
        DXGI_FORMAT format) noexcept
    {
        g.Format = format;
    }

    template<typename S>
    void _initRTV(
        D3D12_RENDER_TARGET_VIEW_DESC &g, S &s,
        const MipmapSlice &mipmapSlice) noexcept
    {
        s.MipSlice = mipmapSlice;
    }

} // namespace detail

inline RTV::RTV(ResourceIndex rsc) noexcept
    : rsc(rsc), desc{}
{
    desc.Format = DXGI_FORMAT_UNKNOWN;
}

template<typename ... Args>
Tex2DRTV::Tex2DRTV(ResourceIndex rsc, const Args &... args) noexcept
    : RTV(rsc)
{
    desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    desc.Texture2D = { 0, 0 };
    InvokeAll([&] { detail::_initRTV(desc, desc.Texture2D, args); }...);
}

template<typename ... Args>
Tex2DMSRTV::Tex2DMSRTV(ResourceIndex rsc, const Args &... args) noexcept
    : RTV(rsc)
{
    desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;
    InvokeAll([&] { detail::_initRTV(desc, desc.Texture2DMS, args); }...);
}

AGZ_D3D12_FG_END
