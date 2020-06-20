#pragma once

#include <d3d12.h>

#include <agz/d3d12/framegraph/common.h>

AGZ_D3D12_FG_BEGIN

struct MipmapLevels
{
    UINT16 mipLevels = 1;
};

struct Multisample
{
    UINT count   = 1;
    UINT quality = 0;
};

struct RscDesc
{
    D3D12_RESOURCE_DESC desc = {};
};

struct Tex2DDesc : RscDesc
{
    template<typename...Args>
    Tex2DDesc(
        DXGI_FORMAT format, UINT w, UINT h,
        const Args &...args) noexcept;
};

struct Tex2DArrDesc : RscDesc
{
    template<typename...Args>
    Tex2DArrDesc(
        DXGI_FORMAT format, UINT w, UINT h, UINT arrSize,
        const Args &...args) noexcept;
};

AGZ_D3D12_FG_END

#include "./impl/resourceDesc.inl"
