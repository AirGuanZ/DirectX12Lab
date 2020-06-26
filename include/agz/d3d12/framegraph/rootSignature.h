#pragma once

#include <d3d12.h>

#include <agz/d3d12/framegraph/resourceView/descriptorTableRangeDesc.h>
#include <agz/d3d12/framegraph/resourceView/staticSamplerDesc.h>
#include <agz/utility/misc.h>
#include <agz/utility/string.h>

AGZ_D3D12_FG_BEGIN

struct ConstantBufferView
{
    ConstantBufferView(
        D3D12_SHADER_VISIBILITY  vis,
        const BRegister         &reg) noexcept;

    D3D12_SHADER_VISIBILITY vis;
    BRegister reg;

    D3D12_ROOT_PARAMETER toRootParameter() const;
};

struct ImmediateConstants
{
    ImmediateConstants(
        D3D12_SHADER_VISIBILITY  vis,
        const BRegister         &reg,
        UINT32                   num32Bits) noexcept;

    D3D12_SHADER_VISIBILITY vis;
    BRegister reg;
    UINT32 num32Bits;

    D3D12_ROOT_PARAMETER toRootParameter() const;
};

/**
 * - CBVRange
 * - SRVRange
 * - UAVRange
 */
struct DescriptorTable
{
    template<typename...Args>
    explicit DescriptorTable(D3D12_SHADER_VISIBILITY vis, Args&&...args);

    D3D12_SHADER_VISIBILITY vis;

    using DescriptorRange = misc::variant_t<
        CBVRange,
        SRVRange,
        UAVRange>;

    std::vector<DescriptorRange> ranges;
};

/**
 * - ConstantBufferView
 * - ImmediateConstants
 * - DescriptorTable
 * - StaticSampler
 */
struct RootSignature
{
    template<typename...Args>
    explicit RootSignature(
        D3D12_ROOT_SIGNATURE_FLAGS flags, Args&&...args);

    using RootParameter = misc::variant_t<
        ConstantBufferView,
        ImmediateConstants,
        DescriptorTable>;

    D3D12_ROOT_SIGNATURE_FLAGS flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

    std::vector<RootParameter> rootParameters;
    std::vector<StaticSampler> staticSamplers;

    ComPtr<ID3D12RootSignature> createRootSignature(ID3D12Device *device) const;
};

AGZ_D3D12_FG_END

#include "./impl/rootSignature.inl"
