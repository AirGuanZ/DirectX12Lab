#pragma once

#include <d3d12.h>

AGZ_D3D12_FG_BEGIN

struct UAV
{
    UAV() noexcept;

    D3D12_UNORDERED_ACCESS_VIEW_DESC desc;
};

struct Tex2DUAV : UAV
{
    // Args: Format
    //       MipmapSlice
    template<typename...Args>
    explicit Tex2DUAV(const Args &...args) noexcept;

    Tex2DUAV(const Tex2DUAV &) = default;
};

struct Tex2DArrUAV : UAV
{
    // Args: Format
    //       MipmapSlice
    //       ArraySlices
    template<typename...Args>
    explicit Tex2DArrUAV(const Args &...args) noexcept;

    Tex2DArrUAV(const Tex2DArrUAV &) = default;
};

AGZ_D3D12_FG_END

#include "./impl/unorderedAccessViewDesc.inl"
