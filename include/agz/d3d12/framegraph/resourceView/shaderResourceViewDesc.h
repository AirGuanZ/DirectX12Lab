#pragma once

#include <d3d12.h>

#include <agz/d3d12/framegraph/resourceView/viewDescCommon.h>

AGZ_D3D12_FG_BEGIN

enum SRVScope
{
    PixelSRV,
    NonPixelSRV,
    PixelAndNonPixelSRV,
    DefaultSRVScope
};

struct SRV
{
    explicit SRV(ResourceIndex rsc, SRVScope scope) noexcept;

    D3D12_RESOURCE_STATES getRequiredRscState() const noexcept;

    ResourceIndex rsc;

    D3D12_SHADER_RESOURCE_VIEW_DESC desc;

    SRVScope scope;
};

struct Tex2DSRV : SRV
{
    // Args: MipmapSlice
    //       MipmapSlice
    //       Format
    template<typename...Args>
    explicit Tex2DSRV(ResourceIndex rsc, const Args &...args) noexcept;

    Tex2DSRV(const Tex2DSRV &) = default;
};

struct Tex2DArrSRV : SRV
{
    // Args: MipmapSlice
    //       MipmapSlices
    //       ArraySlices
    //       Format
    template<typename...Args>
    explicit Tex2DArrSRV(ResourceIndex rsc, const Args &...args) noexcept;

    Tex2DArrSRV(const Tex2DArrSRV &) = default;
};

struct Tex2DMSSRV : SRV
{
    // Args: Format
    template<typename...Args>
    explicit Tex2DMSSRV(ResourceIndex rsc, const Args &...args) noexcept;

    Tex2DMSSRV(const Tex2DMSSRV &) = default;
};

struct Tex2DMSArrSRV : SRV
{
    // Args: ArraySlices
    //       Format
    template<typename...Args>
    explicit Tex2DMSArrSRV(ResourceIndex rsc, const Args &...args) noexcept;

    Tex2DMSArrSRV(const Tex2DMSArrSRV &) = default;
};

AGZ_D3D12_FG_END

#include "./impl/shaderResourceViewDesc.inl"
