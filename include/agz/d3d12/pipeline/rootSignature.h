#pragma once

#include <d3d12.h>

#include <agz/d3d12/common.h>

AGZ_D3D12_BEGIN

class RootSignatureParamPool
{
    // IMPROVE: performance optimization
    std::vector<std::unique_ptr<D3D12_DESCRIPTOR_RANGE[]>>    ranges_;
    std::vector<std::unique_ptr<D3D12_ROOT_PARAMETER[]>>      params_;
    std::vector<std::unique_ptr<D3D12_STATIC_SAMPLER_DESC[]>> staticSamplers_;

public:

    D3D12_DESCRIPTOR_RANGE *allocRange(size_t count)
    {
        ranges_.push_back(std::make_unique<D3D12_DESCRIPTOR_RANGE[]>(count));
        return ranges_.back().get();
    }

    D3D12_ROOT_PARAMETER *allocParams(size_t count)
    {
        params_.push_back(std::make_unique<D3D12_ROOT_PARAMETER[]>(count));
        return params_.back().get();
    }

    D3D12_STATIC_SAMPLER_DESC *allocSamplers(size_t count)
    {
        staticSamplers_.push_back(
            std::make_unique<D3D12_STATIC_SAMPLER_DESC[]>(count));
        return staticSamplers_.back().get();
    }
};

class RootSignatureBuilder
{
    D3D12_ROOT_SIGNATURE_DESC desc_;

    RootSignatureParamPool pool_;

public:

    explicit RootSignatureBuilder(std::string rootSignature);

    const D3D12_ROOT_SIGNATURE_DESC &getDesc() const noexcept;

    ComPtr<ID3D12RootSignature> createSignature(ID3D12Device *device) const;
};

ComPtr<ID3D12RootSignature> createRootSignature(
    ID3D12Device *device, std::string rootSignature);

AGZ_D3D12_END
