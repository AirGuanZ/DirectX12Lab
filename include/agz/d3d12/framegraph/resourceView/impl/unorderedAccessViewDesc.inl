#pragma once

AGZ_D3D12_FG_BEGIN

namespace detail
{

    template<typename S>
    void _initUAV(
        D3D12_UNORDERED_ACCESS_VIEW_DESC &desc, S &s,
        DXGI_FORMAT format) noexcept
    {
        desc.Format = format;
    }

    template<typename S>
    void _initUAV(
        D3D12_UNORDERED_ACCESS_VIEW_DESC &desc, S &s,
        const MipmapSlice &mipmapSlice) noexcept
    {
        s.MipSlice = mipmapSlice.sliceIdx;
    }

    template<typename S>
    void _initUAV(
        D3D12_UNORDERED_ACCESS_VIEW_DESC &desc, S &s, 
        const ArraySlices &arraySlices) noexcept
    {
        s.FirstArraySlice = arraySlices.firstElem;
        s.ArraySize       = arraySlices.elemCount;
    }

} // namespace detail

inline _internalUAV::_internalUAV(ResourceIndex rsc) noexcept
    : rsc(rsc), desc{}
{
    desc.Format = DXGI_FORMAT_UNKNOWN;
}

template<typename ... Args>
Tex2DUAV::Tex2DUAV(ResourceIndex rsc, const Args &... args) noexcept
    : _internalUAV(rsc)
{
    desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    desc.Texture2D     = { 0, 0 };
    InvokeAll([&] { detail::_initUAV(desc, desc.Texture2D, args); }...);
}

template<typename ... Args>
Tex2DArrUAV::Tex2DArrUAV(ResourceIndex rsc, const Args &... args) noexcept
    : _internalUAV(rsc)
{
    desc.ViewDimension  = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
    desc.Texture2DArray = { 0, 0, 1, 0 };
    InvokeAll([&] { detail::_initUAV(desc, desc.Texture2DArray, args); }...);
}

template<typename ... Args>
BufUAV::BufUAV(ResourceIndex rsc, UINT elemSize, UINT elemCnt) noexcept
    : _internalUAV(rsc)
{
    desc.ViewDimension               = D3D12_UAV_DIMENSION_BUFFER;
    desc.Buffer.CounterOffsetInBytes = 0;
    desc.Buffer.FirstElement         = 0;
    desc.Buffer.Flags                = D3D12_BUFFER_UAV_FLAG_NONE;
    desc.Buffer.NumElements          = elemCnt;
    desc.Buffer.StructureByteStride  = elemSize;
}

AGZ_D3D12_FG_END
