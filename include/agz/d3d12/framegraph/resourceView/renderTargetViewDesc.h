#pragma once

#include <d3d12.h>

#include <agz/d3d12/framegraph/resourceView/viewDescCommon.h>

AGZ_D3D12_FG_BEGIN

struct _internalRTV
{
    explicit _internalRTV(ResourceIndex rsc) noexcept;

    ResourceIndex rsc;

    D3D12_RENDER_TARGET_VIEW_DESC desc;
};

/**
 * - DXGI_FORMAT. default value is UNKNOWN (inferred from the rsc)
 * - MipmapSlice. mipmap slice of rsc
 */
struct Tex2DRTV : _internalRTV
{
    template<typename...Args>
    explicit Tex2DRTV(ResourceIndex rsc, const Args &...args) noexcept;

    Tex2DRTV(const Tex2DRTV &) = default;
};

/**
 * - DXGI_FORMAT. default value is UNKNOWN (inferred from the rsc)
 * - MipmapSlice. mipmap slice of rsc
 */
struct Tex2DMSRTV : _internalRTV
{
    template<typename...Args>
    explicit Tex2DMSRTV(ResourceIndex rsc, const Args &...args) noexcept;

    Tex2DMSRTV(const Tex2DMSRTV &) = default;
};

AGZ_D3D12_FG_END

#include "./impl/renderTargetViewDesc.inl"
