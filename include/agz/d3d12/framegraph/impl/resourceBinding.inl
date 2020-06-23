#pragma once

AGZ_D3D12_FG_BEGIN

namespace detail
{

    inline void _initRTB(
        RenderTargetBinding &rtb, const ClearColor &clearColor) noexcept
    {
        rtb.clearColor      = true;
        rtb.clearColorValue = clearColor;
    }

    inline void _initRTB(
        RenderTargetBinding &rtb, const _internalRTV &rtv)
    {
        rtb.rtv = rtv;
    }

    inline void _initDSB(
        DepthStencilBinding &dsb, const ClearDepthStencil &clearValue) noexcept
    {
        dsb.clearDepth             = true;
        dsb.clearStencil           = true;
        dsb.clearDepthStencilValue = clearValue;
    }

    inline void _initDSB(
        DepthStencilBinding &dsb, const ClearDepth &clearDepth) noexcept
    {
        dsb.clearDepth                   = true;
        dsb.clearDepthStencilValue.depth = clearDepth.depth;
    }

    inline void _initDSB(
        DepthStencilBinding &dsb, const ClearStencil &clearStencil) noexcept
    {
        dsb.clearStencil                 = true;
        dsb.clearDepthStencilValue.depth = clearStencil.stencil;
    }

    inline void _initDSB(
        DepthStencilBinding &dsb, const _internalDSV &dsv)
    {
        dsb.dsv = dsv;
    }

} // namespace detail

template<typename ... Args>
RenderTargetBinding::RenderTargetBinding(
    const Args &... args) noexcept
    : rtv(RESOURCE_NIL), clearColor(false)
{
    InvokeAll([&] { detail::_initRTB(*this, args); }...);
}

template<typename ... Args>
DepthStencilBinding::DepthStencilBinding(
    const Args &... args) noexcept
    : dsv(RESOURCE_NIL), clearDepth(false), clearStencil(false)
{
    InvokeAll([&] { detail::_initDSB(*this, args); }...);
}

AGZ_D3D12_FG_END
