#pragma once

#include <d3d12.h>

#include <agz/d3d12/framegraph/common.h>

AGZ_D3D12_FG_BEGIN

static constexpr UINT ALL_MIPMAPS = -1;

struct ArraySlices
{
    UINT firstElem = 0;
    UINT elemCount = 1;
};

struct MipmapSlice
{
    UINT sliceIdx = 0;
};

struct MipmapSlices
{
    UINT mostDetailed = 0;
    UINT mipmapCount  = -1;
    float minLODClamp = 0;
};

struct SRV
{
    SRV() noexcept;

    D3D12_SHADER_RESOURCE_VIEW_DESC desc;
};

struct Tex2DSRV : SRV
{
    // Args: MipmapSlice
    //       MipmapSlice
    //       Format
    template<typename...Args>
    explicit Tex2DSRV(const Args &...args) noexcept;

    Tex2DSRV(const Tex2DSRV &) = default;
};

struct Tex2DArrSRV : SRV
{
    // Args: MipmapSlice
    //       MipmapSlices
    //       ArraySlices
    //       Format
    template<typename...Args>
    explicit Tex2DArrSRV(const Args &...args) noexcept;

    Tex2DArrSRV(const Tex2DArrSRV &) = default;
};

struct Tex2DMSSRV : SRV
{
    // Args: Format
    template<typename...Args>
    explicit Tex2DMSSRV(const Args &...args) noexcept;

    Tex2DMSSRV(const Tex2DMSSRV &) = default;
};

struct Tex2DMSArrSRV : SRV
{
    // Args: ArraySlices
    //       Format
    template<typename...Args>
    explicit Tex2DMSArrSRV(const Args &...args) noexcept;

    Tex2DMSArrSRV(const Tex2DMSArrSRV &) = default;
};

AGZ_D3D12_FG_END

#include "./impl/shaderResourceViewDesc.inl"
