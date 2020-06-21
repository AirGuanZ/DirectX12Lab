#pragma once

#include <agz/d3d12/framegraph/resourceView/depthStencilViewDesc.h>
#include <agz/d3d12/framegraph/resourceView/renderTargetViewDesc.h>

AGZ_D3D12_FG_BEGIN

struct ClearColor : math::color4f
{
    using math::color4f::color4f;
};

struct ClearDepthStencil
{
    float depth   = 1;
    UINT8 stencil = 0;
};

struct ClearDepth
{
    float depth = 1;
};

struct ClearStencil
{
    UINT8 stencil = 0;
};

struct RenderTargetBinding
{
    template<typename...Args>
    explicit RenderTargetBinding(const Args &...args) noexcept;

    RTV rtv;

    bool clearColor;
    ClearColor clearColorValue;
};

struct DepthStencilBinding
{
    template<typename...Args>
    explicit DepthStencilBinding(const Args &...args) noexcept;

    DSV dsv;

    bool clearDepth;
    bool clearStencil;
    ClearDepthStencil clearDepthStencilValue;
};

AGZ_D3D12_FG_END

#include "./impl/resourceBinding.inl"
