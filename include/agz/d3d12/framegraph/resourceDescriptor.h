#pragma once

#include <agz/d3d12/framegraph/resourceView/shaderResourceViewDesc.h>
#include <agz/d3d12/framegraph/resourceView/unorderedAccessViewDesc.h>
#include <agz/utility/misc.h>

AGZ_D3D12_FG_BEGIN

// SRV/UAVs
struct ContinuousGPUDescriptors
{
    template<typename...Args>
    ContinuousGPUDescriptors(const Args &...args);

    using Desc = misc::variant_t<SRV, UAV>;

    std::vector<Desc> descs;
};

AGZ_D3D12_FG_END

#include "./impl/resourceDescriptor.inl"
