#pragma once

AGZ_D3D12_FG_BEGIN

namespace detail
{

    inline void _initContinuousGPUDescriptors(
        ContinuousGPUDescriptors &c, const SRV &srv)
    {
        c.descs.push_back(srv);
    }

    inline void _initContinuousGPUDescriptors(
        ContinuousGPUDescriptors &c, const UAV &uav)
    {
        c.descs.push_back(uav);
    }

} // namespace detail

template<typename ... Args>
ContinuousGPUDescriptors::ContinuousGPUDescriptors(const Args &... args)
{
    InvokeAll([&] { detail::_initContinuousGPUDescriptors(*this, args); }...);
}

AGZ_D3D12_FG_END
