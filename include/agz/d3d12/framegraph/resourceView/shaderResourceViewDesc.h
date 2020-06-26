#pragma once

#include <d3d12.h>

#include <agz/d3d12/framegraph/resourceView/viewDescCommon.h>

AGZ_D3D12_FG_BEGIN

enum SRVScope
{
    // the shader resource will only be used in pixel shader stage through this descriptors
    PixelSRV,
    // the shader resource will only be used in non-pixel shader stage through this descriptors
    NonPixelSRV,
    // the shader resource will only be used in both pixel and non-pixel shader stage through this descriptors
    PixelAndNonPixelSRV,
    // use this if you are unsure
    DefaultSRVScope
};

struct _internalSRV
{
    explicit _internalSRV(ResourceIndex rsc, SRVScope scope) noexcept;

    D3D12_RESOURCE_STATES getRequiredRscState() const noexcept;

    ResourceIndex rsc;

    D3D12_SHADER_RESOURCE_VIEW_DESC desc;

    SRVScope scope;
};

/**
 * - DXGI_FORMAT. default value is UNKNOWN (inferred from rsc)
 * - MipmapSlice. use single mipmap slice of rsc. default is 0
 * - MipmapSlices. use mipmap slices of rsc
 * - SRVScope. accessible shader stages. default is 'DEFAULT' in which all
 *   shader stages can access it
 */
struct Tex2DSRV : _internalSRV
{
    template<typename...Args>
    explicit Tex2DSRV(ResourceIndex rsc, const Args &...args) noexcept;

    Tex2DSRV(const Tex2DSRV &) = default;
};

/**
 * - DXGI_FORMAT. default value is UNKNOWN (inferred from rsc)
 * - MipmapSlice. use single mipmap slice of rsc. default is 0
 * - MipmapSlices. use mipmap slices of rsc
 * - ArraySlices. use array elems of rsc. default is [0]
 * - SRVScope. accessible shader stages. default is 'DEFAULT' in which all
 *   shader stages can access it
 */
struct Tex2DArrSRV : _internalSRV
{
    template<typename...Args>
    explicit Tex2DArrSRV(ResourceIndex rsc, const Args &...args) noexcept;

    Tex2DArrSRV(const Tex2DArrSRV &) = default;
};

/**
 * - DXGI_FORMAT. default value is UNKNOWN (inferred from rsc)
 * - SRVScope. accessible shader stages. default is 'DEFAULT' in which all
 *   shader stages can access it
 */
struct Tex2DMSSRV : _internalSRV
{
    template<typename...Args>
    explicit Tex2DMSSRV(ResourceIndex rsc, const Args &...args) noexcept;

    Tex2DMSSRV(const Tex2DMSSRV &) = default;
};

struct Tex2DMSArrSRV : _internalSRV
{
    /**
     * - DXGI_FORMAT. default value is UNKNOWN (inferred from rsc)
     * - ArraySlices. use array elems of rsc. default is [0]
     * - SRVScope. accessible shader stages. default is 'DEFAULT' in which all
     *   shader stages can access it
     */
    template<typename...Args>
    explicit Tex2DMSArrSRV(ResourceIndex rsc, const Args &...args) noexcept;

    Tex2DMSArrSRV(const Tex2DMSArrSRV &) = default;
};

AGZ_D3D12_FG_END

#include "./impl/shaderResourceViewDesc.inl"
