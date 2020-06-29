#pragma once

AGZ_D3D12_FG_BEGIN

namespace detail
{

    inline void _initRscDesc(
        D3D12_RESOURCE_DESC &desc, const MipmapLevels &mip) noexcept
    {
        desc.MipLevels = mip.mipLevels;
    }

    inline void _initRscDesc(
        D3D12_RESOURCE_DESC &desc, const Multisample &ms) noexcept
    {
        desc.SampleDesc = { ms.count, ms.quality };
    }

    inline void _initRscDesc(
        D3D12_RESOURCE_DESC &desc, D3D12_RESOURCE_FLAGS flags) noexcept
    {
        desc.Flags |= flags;
    }

} // namespace detail

template<typename ... Args>
Tex2DDesc::Tex2DDesc(
    DXGI_FORMAT format, UINT w, UINT h,
    const Args &... args) noexcept
{
    desc.Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Alignment        = 0;
    desc.Width            = w;
    desc.Height           = h;
    desc.DepthOrArraySize = 1;
    desc.MipLevels        = 1;
    desc.Format           = format;
    desc.SampleDesc       = { 1, 0 };
    desc.Layout           = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags            = D3D12_RESOURCE_FLAG_NONE;

    InvokeAll([&] { detail::_initRscDesc(desc, args); }...);
}

template<typename ... Args>
Tex2DArrDesc::Tex2DArrDesc(
    DXGI_FORMAT format, UINT w, UINT h, UINT arrSize,
    const Args &... args) noexcept
{
    desc.Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Alignment        = 0;
    desc.Width            = w;
    desc.Height           = h;
    desc.DepthOrArraySize = arrSize;
    desc.MipLevels        = 1;
    desc.Format           = format;
    desc.SampleDesc       = { 1, 0 };
    desc.Layout           = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags            = D3D12_RESOURCE_FLAG_NONE;

    InvokeAll([&] { detail::_initRscDesc(desc, args); }...);
}

template<typename ... Args>
BufDesc::BufDesc(size_t byteSize, const Args &... args) noexcept
{
    desc = CD3DX12_RESOURCE_DESC::Buffer(byteSize);

    InvokeAll([&] { detail::_initRscDesc(desc, args); }...);
}

AGZ_D3D12_FG_END
