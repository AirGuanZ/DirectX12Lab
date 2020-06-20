#pragma once

AGZ_D3D12_FG_BEGIN

namespace detail
{

    inline void _initStaticSampler(
        D3D12_STATIC_SAMPLER_DESC &d, D3D12_FILTER filter) noexcept
    {
        d.Filter = filter;
    }

    inline void _initStaticSampler(
        D3D12_STATIC_SAMPLER_DESC &d, const SamplerAddressMode &m) noexcept
    {
        d.AddressU = m.u;
        d.AddressV = m.v;
        d.AddressW = m.w;
    }

    inline void _initStaticSampler(
        D3D12_STATIC_SAMPLER_DESC &d, const SamplerLOD &lod) noexcept
    {
        d.MipLODBias = lod.mipLODBias;
        d.MinLOD = lod.minLOD;
        d.MaxLOD = lod.maxLOD;
    }

    inline void _initStaticSampler(
        D3D12_STATIC_SAMPLER_DESC &d, SamplerMaxAnisotropy a) noexcept
    {
        d.MaxAnisotropy = a.maxAnisotropy;
    }

    inline void _initStaticSampler(
        D3D12_STATIC_SAMPLER_DESC &d, D3D12_COMPARISON_FUNC f) noexcept
    {
        d.ComparisonFunc = f;
    }

} // namespace detail

template<typename ... Args>
StaticSampler::StaticSampler(
    D3D12_SHADER_VISIBILITY vis,
    const SRegister &registerBinding,
    Args &&... args)
    : desc{}
{
    desc.ShaderVisibility = vis;
    desc.RegisterSpace    = registerBinding.registerSpace;
    desc.ShaderRegister   = registerBinding.registerNumber;
    desc.Filter           = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    desc.AddressU         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    desc.AddressV         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    desc.AddressW         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    desc.MipLODBias       = 0;
    desc.MaxAnisotropy    = 16;
    desc.ComparisonFunc   = D3D12_COMPARISON_FUNC_ALWAYS;
    desc.BorderColor      = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
    desc.MinLOD           = 0;
    desc.MaxLOD           = FLT_MAX;

    InvokeAll([&]
    {
        detail::_initStaticSampler(desc, std::forward<Args>(args));
    }...);
}

AGZ_D3D12_FG_END
