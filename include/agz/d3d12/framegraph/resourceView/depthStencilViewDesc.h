#pragma once

#include <d3d12.h>

#include <agz/d3d12/framegraph/resourceView/viewDescCommon.h>

AGZ_D3D12_FG_BEGIN

struct _internalDSV
{
    explicit _internalDSV(ResourceIndex rsc) noexcept;

    ResourceIndex rsc;

    D3D12_DEPTH_STENCIL_VIEW_DESC desc;
};

struct Tex2DDSV : _internalDSV
{
    template<typename...Args>
    explicit Tex2DDSV(ResourceIndex rsc, const Args &...args) noexcept;

    Tex2DDSV(const Tex2DDSV &) = default;
};

struct Tex2DMSDSV : _internalDSV
{
    template<typename...Args>
    explicit Tex2DMSDSV(ResourceIndex rsc, const Args &...args) noexcept;

    Tex2DMSDSV(const Tex2DMSDSV &) = default;
};

AGZ_D3D12_FG_END

#include "./impl/depthStencilViewDesc.inl"
