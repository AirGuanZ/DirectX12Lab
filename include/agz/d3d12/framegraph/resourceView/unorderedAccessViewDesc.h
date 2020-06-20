#pragma once

#include <d3d12.h>

#include <agz/d3d12/framegraph/resourceView/viewDescCommon.h>

AGZ_D3D12_FG_BEGIN

struct UAV
{
    explicit UAV(ResourceIndex rsc) noexcept;

    ResourceIndex rsc;

    D3D12_UNORDERED_ACCESS_VIEW_DESC desc;
};

struct Tex2DUAV : UAV
{
    // Args: Format
    //       MipmapSlice
    template<typename...Args>
    explicit Tex2DUAV(ResourceIndex rsc, const Args &...args) noexcept;

    Tex2DUAV(const Tex2DUAV &) = default;
};

struct Tex2DArrUAV : UAV
{
    // Args: Format
    //       MipmapSlice
    //       ArraySlices
    template<typename...Args>
    explicit Tex2DArrUAV(ResourceIndex rsc, const Args &...args) noexcept;

    Tex2DArrUAV(const Tex2DArrUAV &) = default;
};

AGZ_D3D12_FG_END

#include "./impl/unorderedAccessViewDesc.inl"
