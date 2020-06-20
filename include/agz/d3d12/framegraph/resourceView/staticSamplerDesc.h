#pragma once

#include <d3d12.h>

#include <agz/d3d12/framegraph/common.h>

AGZ_D3D12_FG_BEGIN

struct SamplerAddressMode
{
    D3D12_TEXTURE_ADDRESS_MODE u = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    D3D12_TEXTURE_ADDRESS_MODE v = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    D3D12_TEXTURE_ADDRESS_MODE w = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
};

struct SamplerLOD
{
    float mipLODBias = 0;

    float minLOD = 0;
    float maxLOD = FLT_MAX;
};

struct SamplerMaxAnisotropy
{
    UINT maxAnisotropy;
};

struct StaticSampler
{
    // Args: D3D12_FILTER
    //       SamplerAddressMode
    //       SamplerLOD
    //       SamplerMaxAnisotropy
    //       D3D12_COMPARISON_FUNC
    template<typename...Args>
    StaticSampler(
        D3D12_SHADER_VISIBILITY vis,
        const SRegister &registerBinding,
        Args &&...args);

    D3D12_STATIC_SAMPLER_DESC desc;
};

AGZ_D3D12_FG_END

#include "./impl/staticSamplerDesc.inl"
