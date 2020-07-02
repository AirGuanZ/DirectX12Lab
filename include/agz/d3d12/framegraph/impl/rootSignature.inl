#pragma once

AGZ_D3D12_FG_BEGIN

inline ConstantBufferView::ConstantBufferView(
    D3D12_SHADER_VISIBILITY  vis,
    const BRegister         &reg) noexcept
    : vis(vis), reg(reg)
{
    
}

inline ShaderResourceView::ShaderResourceView(
    D3D12_SHADER_VISIBILITY vis,
    const TRegister        &reg) noexcept
    : vis(vis), reg(reg)
{
    
}

inline UnorderedAccessView::UnorderedAccessView(
    D3D12_SHADER_VISIBILITY vis,
    const URegister        &reg) noexcept
    : vis(vis), reg(reg)
{
    
}

inline ImmediateConstants::ImmediateConstants(
    D3D12_SHADER_VISIBILITY  vis,
    const BRegister         &reg,
    UINT32                   num32Bits) noexcept
    : vis(vis), reg(reg), num32Bits(num32Bits)
{
    
}

template<typename ... Args>
DescriptorTable::DescriptorTable(D3D12_SHADER_VISIBILITY vis, Args &&... args)
    : vis(vis)
{
    InvokeAll([&] { ranges.push_back(std::forward<Args>(args)); }...);
}

namespace detail
{

    inline void _initRootSignature(RootSignature &rg, ConstantBufferView cbv)
    {
        rg.rootParameters.emplace_back(std::move(cbv));
    }

    inline void _initRootSignature(RootSignature &rg, ShaderResourceView srv)
    {
        rg.rootParameters.emplace_back(std::move(srv));
    }

    inline void _initRootSignature(RootSignature &rg, UnorderedAccessView srv)
    {
        rg.rootParameters.emplace_back(std::move(srv));
    }

    inline void _initRootSignature(RootSignature &rg, ImmediateConstants ics)
    {
        rg.rootParameters.emplace_back(std::move(ics));
    }

    inline void _initRootSignature(RootSignature &rg, DescriptorTable dt)
    {
        rg.rootParameters.emplace_back(std::move(dt));
    }

    inline void _initRootSignature(RootSignature &rg, StaticSampler s)
    {
        rg.staticSamplers.push_back(std::move(s));
    }

} // namespace detail

template<typename ... Args>
RootSignature::RootSignature(D3D12_ROOT_SIGNATURE_FLAGS flags, Args &&... args)
    : flags(flags)
{
    InvokeAll([&]
    {
        detail::_initRootSignature(*this, std::forward<Args>(args));
    }...);
}

inline D3D12_ROOT_PARAMETER ConstantBufferView::toRootParameter() const noexcept
{
    CD3DX12_ROOT_PARAMETER ret;
    ret.InitAsConstantBufferView(reg.registerNumber, reg.registerSpace, vis);
    return ret;
}

inline D3D12_ROOT_PARAMETER ShaderResourceView::toRootParameter() const noexcept
{
    CD3DX12_ROOT_PARAMETER ret;
    ret.InitAsShaderResourceView(reg.registerNumber, reg.registerSpace, vis);
    return ret;
}

inline D3D12_ROOT_PARAMETER UnorderedAccessView::toRootParameter() const noexcept
{
    CD3DX12_ROOT_PARAMETER ret;
    ret.InitAsUnorderedAccessView(reg.registerNumber, reg.registerSpace, vis);
    return ret;
}

inline D3D12_ROOT_PARAMETER ImmediateConstants::toRootParameter() const noexcept
{
    CD3DX12_ROOT_PARAMETER ret;
    ret.InitAsConstants(num32Bits, reg.registerNumber, reg.registerSpace, vis);
    return ret;
}

inline ComPtr<ID3D12RootSignature> RootSignature::createRootSignature(
    ID3D12Device *device) const
{
    std::vector<D3D12_ROOT_PARAMETER>                      rootParamStorage;
    std::vector<D3D12_STATIC_SAMPLER_DESC>                 staticSamplerStorage;
    std::vector<std::unique_ptr<D3D12_DESCRIPTOR_RANGE[]>> rangeStorage;

    for(auto &rp : rootParameters)
    {
        match_variant(rp,
            [&](const ConstantBufferView &cbv)
        {
            rootParamStorage.push_back(cbv.toRootParameter());
        },
            [&](const ShaderResourceView &srv)
        {
            rootParamStorage.push_back(srv.toRootParameter());
        },
            [&](const UnorderedAccessView &uav)
        {
            rootParamStorage.push_back(uav.toRootParameter());
        },
            [&](const ImmediateConstants &ics)
        {
            rootParamStorage.push_back(ics.toRootParameter());
        },
            [&](const DescriptorTable &dt)
        {
            std::vector<D3D12_DESCRIPTOR_RANGE> ranges;
            for(auto &r : dt.ranges)
            {
                match_variant(r,
                    [&](const CBVRange &cbvr)
                {
                    ranges.push_back(cbvr.range);
                },
                    [&](const SRVRange &srvr)
                {
                    ranges.push_back(srvr.range);
                },
                    [&](const UAVRange &uavr)
                {
                    ranges.push_back(uavr.range);
                });
            }

            rangeStorage.push_back(
                std::make_unique<D3D12_DESCRIPTOR_RANGE[]>(ranges.size()));
            std::memcpy(
                rangeStorage.back().get(), ranges.data(),
                sizeof(D3D12_DESCRIPTOR_RANGE) *ranges.size());

            CD3DX12_ROOT_PARAMETER result;
            result.InitAsDescriptorTable(
                UINT(ranges.size()),
                rangeStorage.back().get(),
                dt.vis);

            rootParamStorage.push_back(result);
        });
    }

    for(auto &s : staticSamplers)
        staticSamplerStorage.push_back(s.desc);

    CD3DX12_ROOT_SIGNATURE_DESC desc;
    desc.Init(
        UINT(rootParameters.size()), rootParamStorage.data(),
        UINT(staticSamplers.size()), staticSamplerStorage.data(),
        flags);

    ComPtr<ID3D10Blob> serilized, err;
    if(FAILED(D3D12SerializeRootSignature(
        &desc, D3D_ROOT_SIGNATURE_VERSION_1_0,
        serilized.GetAddressOf(), err.GetAddressOf())))
    {
        throw D3D12LabException(
            "failed to serilize root signature. err = " +
        std::string(reinterpret_cast<const char*>(err->GetBufferPointer())));
    }

    ComPtr<ID3D12RootSignature> ret;
    AGZ_D3D12_CHECK_HR(
        device->CreateRootSignature(
            0, serilized->GetBufferPointer(), serilized->GetBufferSize(),
            IID_PPV_ARGS(ret.GetAddressOf())));

    return ret;
}

AGZ_D3D12_FG_END
