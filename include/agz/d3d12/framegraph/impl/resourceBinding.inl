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
        RenderTargetBinding &rtb, const RTV &rtv)
    {
        assert(!rtb.rtv);
        rtb.rtv = rtv;
    }

    inline void _initRTB(
        RenderTargetBinding &rtb, ResourceIndex rsc) noexcept
    {
        assert(!rtb.rtv);
        rtb.rsc = rsc;
    }

    inline void _initDSB(
        DepthStencilBinding &dsb, const ClearDepthStencil &clearValue) noexcept
    {
        dsb.clearDepthStencil      = true;
        dsb.clearDepthStencilValue = clearValue;
    }

    inline void _initDSB(
        DepthStencilBinding &dsb, const DSV &dsv)
    {
        assert(!dsb.rsc);
        dsb.dsv = dsv;
    }

    inline void _initDSB(
            DepthStencilBinding &dsb, ResourceIndex rsc) noexcept
    {
        assert(!dsb.dsv);
        dsb.rsc = rsc;
    }

} // namespace detail

template<typename ... Args>
RenderTargetBinding::RenderTargetBinding(
    const Args &... args) noexcept
    : clearColor(false)
{
    InvokeAll([&] { detail::_initRTB(*this, args); }...);
}

template<typename ... Args>
DepthStencilBinding::DepthStencilBinding(
    const Args &... args) noexcept
    : clearDepthStencil(false)
{
    InvokeAll([&] { detail::_initDSB(*this, args); }...);
}

AGZ_D3D12_FG_END
