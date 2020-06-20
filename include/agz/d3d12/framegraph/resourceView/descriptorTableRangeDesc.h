#pragma once

#include <optional>

#include <d3d12.h>

#include <agz/d3d12/framegraph/resourceView/shaderResourceViewDesc.h>
#include <agz/d3d12/framegraph/resourceView/unorderedAccessViewDesc.h>

AGZ_D3D12_FG_BEGIN

struct RangeSize
{
    UINT size = 1;
};

struct CBVRange
{
    template<typename...Args>
    CBVRange(
        const BRegister &reg,
        const Args &...args) noexcept;

    CBVRange(const CBVRange &) = default;

    D3D12_DESCRIPTOR_RANGE range;
};

struct SRVRange
{
    template<typename...Args>
    SRVRange(
        const TRegister &reg,
        const Args &...args) noexcept;

    SRVRange(const SRVRange &) = default;

    D3D12_DESCRIPTOR_RANGE range;

    SRV singleSRV;
};

struct UAVRange
{
    template<typename...Args>
    UAVRange(
        const URegister &reg,
        const Args &...args) noexcept;

    UAVRange(const UAVRange &) = default;

    D3D12_DESCRIPTOR_RANGE range;

    UAV singleUAV;
};

AGZ_D3D12_FG_END

#include "./impl/descriptorTableRangeDesc.inl"
