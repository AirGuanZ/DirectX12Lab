#pragma once

#include <optional>

#include <agz/d3d12/framegraph/resourceView/depthStencilViewDesc.h>
#include <agz/d3d12/framegraph/resourceView/renderTargetViewDesc.h>

AGZ_D3D12_FG_BEGIN

struct ClearColor : math::color4b
{
    using math::color4b::color4b;
};

struct ClearDepthStencil
{
    float depth   = 1;
    UINT8 stencil = 0;
};

struct RenderTargetBinding
{
    template<typename...Args>
    explicit RenderTargetBinding(const Args &...args) noexcept;

    // only one of rsc & rtv can be non-nil
    std::optional<ResourceIndex> rsc;
    std::optional<RTV> rtv;

    bool clearColor;
    ClearColor clearColorValue;
};

struct DepthStencilBinding
{
    template<typename...Args>
    explicit DepthStencilBinding(const Args &...args) noexcept;

    // only one of rsc & dsv can be non-nil
    std::optional<ResourceIndex> rsc;
    std::optional<DSV> dsv;

    bool clearDepthStencil;
    ClearDepthStencil clearDepthStencilValue;
};

AGZ_D3D12_FG_END

#include "./impl/resourceBinding.inl"
