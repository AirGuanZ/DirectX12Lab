#pragma once

AGZ_D3D12_FG_BEGIN

namespace detail
{

    template<typename G>
    void _initDTRange(
        G &g, D3D12_DESCRIPTOR_RANGE &range,
        const RangeSize &rangeSize) noexcept
    {
        range.NumDescriptors = rangeSize.size;
    }

    //template<typename G>
    //void _initDTRange(
    //    G &g, D3D12_DESCRIPTOR_RANGE &range,
    //    const SRV &singleSRV) noexcept
    //{
    //    assert(g.NumDescriptors == 1);
    //    g.singleSRV = singleSRV;
    //}
    //
    //template<typename G>
    //void _initDTRange(
    //    G &g, D3D12_DESCRIPTOR_RANGE &range,
    //    const UAV &singleUAV) noexcept
    //{
    //    assert(g.NumDescriptors == 1);
    //    g.singleUAV = singleUAV;
    //}

} // namespace detail

template<typename ... Args>
CBVRange::CBVRange(const BRegister &reg, const Args &... args) noexcept
{
    range.RangeType                         = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
    range.NumDescriptors                    = 1;
    range.BaseShaderRegister                = reg.registerNumber;
    range.RegisterSpace                     = reg.registerSpace;
    range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    InvokeAll([&] { detail::_initDTRange(*this, range, args); }...);
}

template<typename ... Args>
SRVRange::SRVRange(const TRegister &reg, const Args &... args) noexcept
    //: singleSRV(RESOURCE_NIL, DefaultSRVScope)
{
    range.RangeType                         = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    range.NumDescriptors                    = 1;
    range.BaseShaderRegister                = reg.registerNumber;
    range.RegisterSpace                     = reg.registerSpace;
    range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    InvokeAll([&] { detail::_initDTRange(*this, range, args); }...);
}

template<typename ... Args>
UAVRange::UAVRange(const URegister &reg, const Args &... args) noexcept
    //: singleUAV(RESOURCE_NIL)
{
    range.RangeType                         = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    range.NumDescriptors                    = 1;
    range.BaseShaderRegister                = reg.registerNumber;
    range.RegisterSpace                     = reg.registerSpace;
    range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    InvokeAll([&] { detail::_initDTRange(*this, range, args); }...);
}

AGZ_D3D12_FG_END
