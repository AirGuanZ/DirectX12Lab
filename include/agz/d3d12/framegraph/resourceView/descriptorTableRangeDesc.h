#pragma once

#include <d3d12.h>

#include <agz/d3d12/framegraph/resourceView/shaderResourceViewDesc.h>
#include <agz/d3d12/framegraph/resourceView/unorderedAccessViewDesc.h>

AGZ_D3D12_FG_BEGIN

struct RangeSize
{
    UINT size = 1;
};

/**
 * - RangeSize elem count of the descriptor range
 */
struct CBVRange
{
    template<typename...Args>
    explicit CBVRange(
        const BRegister &reg,
        const Args &...args) noexcept;

    CBVRange(const CBVRange &) = default;

    D3D12_DESCRIPTOR_RANGE range;
};

/**
 * - RangeSize elem count of the descriptor range
 */
struct SRVRange
{
    template<typename...Args>
    explicit SRVRange(
        const TRegister &reg,
        const Args &...args) noexcept;

    SRVRange(const SRVRange &) = default;

    D3D12_DESCRIPTOR_RANGE range;
};

/**
 * - RangeSize elem count of the descriptor range
 */
struct UAVRange
{
    template<typename...Args>
    explicit UAVRange(
        const URegister &reg,
        const Args &...args) noexcept;

    UAVRange(const UAVRange &) = default;

    D3D12_DESCRIPTOR_RANGE range;

    //UAV singleUAV;
};

AGZ_D3D12_FG_END

#include "./impl/descriptorTableRangeDesc.inl"
