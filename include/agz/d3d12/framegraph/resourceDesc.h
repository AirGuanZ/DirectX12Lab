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

/**
 * - MipmapLevels. num of mipmap levels. default is 1
 * - Multisample. MS setting. default is { 1, 0 }
 * - D3D12_RESOURCE_FLAGS. rsc flag. default is NONE
 *      if a rsc is used as 'render target' / 'depth stencil'
 *      / 'unordered access rsc', the compiler will automatically infer
 *      the required resource flag. so these flags can be omitted.
 */
struct Tex2DDesc : RscDesc
{
    template<typename...Args>
    Tex2DDesc(
        DXGI_FORMAT format, UINT w, UINT h,
        const Args &...args) noexcept;
};

/**
 * - MipmapLevels. num of mipmap levels. default is 1
 * - Multisample. MS setting. default is { 1, 0 }
 * - D3D12_RESOURCE_FLAGS. rsc flag. default is NONE
 *      if a rsc is used as 'render target' / 'depth stencil'
 *      / 'unordered access rsc', the compiler will automatically infer
 *      the required resource flag. so these flags can be omitted.
 */
struct Tex2DArrDesc : RscDesc
{
    template<typename...Args>
    Tex2DArrDesc(
        DXGI_FORMAT format, UINT w, UINT h, UINT arrSize,
        const Args &...args) noexcept;
};

AGZ_D3D12_FG_END

#include "./impl/resourceDesc.inl"
