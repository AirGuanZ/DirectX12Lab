#pragma once

#include <d3d12.h>

#include <agz/d3d12/framegraph/common.h>
#include <agz/utility/misc.h>
#include <agz/utility/string.h>

AGZ_D3D12_FG_BEGIN

struct ConstantBufferView
{
    ConstantBufferView(
        D3D12_SHADER_VISIBILITY vis,
        std::string             registerBinding) noexcept;

    D3D12_SHADER_VISIBILITY vis;
    std::string registerBinding;

    D3D12_ROOT_PARAMETER toRootParameter() const;
};

struct ImmediateConstants
{
    ImmediateConstants(
        D3D12_SHADER_VISIBILITY vis,
        std::string             registerBinding,
        UINT32                  num32Bits) noexcept;

    D3D12_SHADER_VISIBILITY vis;
    std::string registerBinding;
    UINT32 num32Bits;

    D3D12_ROOT_PARAMETER toRootParameter() const;
};

struct RangeSize
{
    UINT size;
};

struct ViewRangeBase
{
    // Args: RangeSize, ResourceIndex, DXGI_FORMAT
    template<typename...Args>
    explicit ViewRangeBase(std::string registerBinding, Args&&...args) noexcept;

    std::string registerBinding;
    UINT rangeSize;

    ResourceIndex singleRsc;
    union
    {
        D3D12_CONSTANT_BUFFER_VIEW_DESC  cbvDesc;
        D3D12_SHADER_RESOURCE_VIEW_DESC  srvDesc;
        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc;
    };
};

struct ConstantBufferViewRange : ViewRangeBase
{
    using ViewRangeBase::ViewRangeBase;

    D3D12_DESCRIPTOR_RANGE toDescriptorRange() const;
};

struct ShaderResourceViewRange : ViewRangeBase
{
    using ViewRangeBase::ViewRangeBase;

    D3D12_DESCRIPTOR_RANGE toDescriptorRange() const;
};

struct UnorderedAccessViewRange : ViewRangeBase
{
    using ViewRangeBase::ViewRangeBase;

    D3D12_DESCRIPTOR_RANGE toDescriptorRange() const;
};

struct DescriptorTable
{
    // Args: ConstantBufferViewRange
    //       ShaderResourceViewRange
    //       UnorderedAccessViewRange
    template<typename...Args>
    explicit DescriptorTable(D3D12_SHADER_VISIBILITY vis, Args&&...args);

    D3D12_SHADER_VISIBILITY vis;

    using DescriptorRange = misc::variant_t<
        ConstantBufferViewRange,
        ShaderResourceViewRange,
        UnorderedAccessViewRange>;

    std::vector<DescriptorRange> ranges;
};

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
        std::string registerBinding,
        Args &&...args);

    D3D12_STATIC_SAMPLER_DESC desc;
};

struct RootSignature
{
    template<typename...Args>
    RootSignature(
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

inline ConstantBufferView::ConstantBufferView(
    D3D12_SHADER_VISIBILITY vis,
    std::string             registerBinding) noexcept
    : vis(vis), registerBinding(std::move(registerBinding))
{
    
}

inline ImmediateConstants::ImmediateConstants(
    D3D12_SHADER_VISIBILITY vis,
    std::string             registerBinding,
    UINT32                  num32Bits) noexcept
    : vis(vis), registerBinding(registerBinding), num32Bits(num32Bits)
{
    
}

namespace detail
{

    inline void _initVRB(ViewRangeBase &vrb, RangeSize rangeSize) noexcept
    {
        assert(rangeSize.size == 1 || vrb.singleRsc.isNil());
        vrb.rangeSize = rangeSize.size;
    }

    inline void _initVRB(ViewRangeBase &vrb, ResourceIndex singleRsc) noexcept
    {
        assert(vrb.rangeSize == 1);
        vrb.singleRsc = singleRsc;
    }

    inline void _initVRB(
        ViewRangeBase &vrb,
        const D3D12_CONSTANT_BUFFER_VIEW_DESC &cbvDesc) noexcept
    {
        assert(vrb.rangeSize == 1);
        vrb.cbvDesc = cbvDesc;
    }

    inline void _initVRB(
        ViewRangeBase &vrb,
        const D3D12_SHADER_RESOURCE_VIEW_DESC &srvDesc) noexcept
    {
        assert(vrb.rangeSize == 1);
        vrb.srvDesc = srvDesc;
    }

    inline void _initVRB(
        ViewRangeBase &vrb,
        const D3D12_UNORDERED_ACCESS_VIEW_DESC &uavDesc) noexcept
    {
        assert(vrb.rangeSize == 1);
        vrb.uavDesc = uavDesc;
    }

} // namespace detail

template<typename ... Args>
ViewRangeBase::ViewRangeBase(
    std::string registerBinding,
    Args &&... args) noexcept
    : registerBinding(std::move(registerBinding)), rangeSize(1),
      singleRsc(RESOURCE_NIL), cbvDesc{}
{
    InvokeAll([&] { detail::_initVRB(*this, std::forward<Args>(args)); }...);
}

template<typename ... Args>
DescriptorTable::DescriptorTable(D3D12_SHADER_VISIBILITY vis, Args &&... args)
    : vis(vis)
{
    InvokeAll([&] { ranges.push_back(std::forward<Args>(args)); }...);
}

namespace detail
{
    inline bool parseRegister(
        const std::string &reg, char regType,
        UINT &space, UINT &num) noexcept
    {
        try
        {
            if(reg[0] != 's')
                return false;

            const auto ss = stdstr::split(reg.substr(1), regType);
            if(ss.size() != 2)
                return false;

            space = std::stoul(ss[0]);
            num = std::stoul(ss[1]);
        }
        catch(...)
        {
            return false;
        }

        return true;
    }

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
    std::string registerBinding,
    Args &&... args)
    : desc{}
{
    desc.ShaderVisibility = vis;
    if(!detail::parseRegister(
        registerBinding, 's', desc.RegisterSpace, desc.ShaderRegister))
    {
        throw D3D12LabException("invalid static sampler register: " +
                                registerBinding);
    }

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

namespace detail
{

    inline void _initRootSignature(RootSignature &rg, ConstantBufferView cbv)
    {
        rg.rootParameters.emplace_back(std::move(cbv));
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

inline D3D12_ROOT_PARAMETER ConstantBufferView::toRootParameter() const
{
    UINT registerSpace, registerNum;
    if(!detail::parseRegister(registerBinding, 'b', registerSpace, registerNum))
    {
        throw D3D12LabException("invalid constant buffer view register: " +
                                registerBinding);
    }

    CD3DX12_ROOT_PARAMETER ret;
    ret.InitAsConstantBufferView(registerNum, registerSpace, vis);
    return ret;
}

inline D3D12_ROOT_PARAMETER ImmediateConstants::toRootParameter() const
{
    UINT registerSpace, registerNum;
    if(!detail::parseRegister(registerBinding, 'b', registerSpace, registerNum))
    {
        throw D3D12LabException("invalid constant buffer view register: " +
                                registerBinding);
    }

    CD3DX12_ROOT_PARAMETER ret;
    ret.InitAsConstants(num32Bits, registerNum, registerSpace, vis);
    return ret;
}

inline D3D12_DESCRIPTOR_RANGE
    ConstantBufferViewRange::toDescriptorRange() const
{
    UINT registerSpace, registerNum;
    if(!detail::parseRegister(registerBinding, 'b', registerSpace, registerNum))
    {
        throw D3D12LabException("invalid constant buffer view (range) register: " +
                                registerBinding);
    }

    CD3DX12_DESCRIPTOR_RANGE ret;
    ret.Init(
        D3D12_DESCRIPTOR_RANGE_TYPE_CBV, rangeSize,
        registerNum, registerSpace,
        D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND);
    return ret;
}

inline D3D12_DESCRIPTOR_RANGE
    ShaderResourceViewRange::toDescriptorRange() const
{
    UINT registerSpace, registerNum;
    if(!detail::parseRegister(registerBinding, 't', registerSpace, registerNum))
    {
        throw D3D12LabException("invalid shader resource view (range) register: " +
                                registerBinding);
    }

    CD3DX12_DESCRIPTOR_RANGE ret;
    ret.Init(
        D3D12_DESCRIPTOR_RANGE_TYPE_SRV, rangeSize,
        registerNum, registerSpace,
        D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND);
    return ret;
}

inline D3D12_DESCRIPTOR_RANGE
    UnorderedAccessViewRange::toDescriptorRange() const
{
    UINT registerSpace, registerNum;
    if(!detail::parseRegister(registerBinding, 'u', registerSpace, registerNum))
    {
        throw D3D12LabException("invalid unordered access view (range) register: " +
                                registerBinding);
    }

    CD3DX12_DESCRIPTOR_RANGE ret;
    ret.Init(
        D3D12_DESCRIPTOR_RANGE_TYPE_UAV, rangeSize,
        registerNum, registerSpace,
        D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND);
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
                    [&](const ConstantBufferViewRange &cbvr)
                {
                    ranges.push_back(cbvr.toDescriptorRange());
                },
                    [&](const ShaderResourceViewRange &srvr)
                {
                    ranges.push_back(srvr.toDescriptorRange());
                },
                    [&](const UnorderedAccessViewRange &uavr)
                {
                    ranges.push_back(uavr.toDescriptorRange());
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
