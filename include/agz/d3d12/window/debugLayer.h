#pragma once

#include <agz/d3d12/common.h>
#include <agz/utility/system.h>

AGZ_D3D12_BEGIN

void enableD3D12DebugLayer(bool gpuBasedValidation = false);

inline void enableD3D12DebugLayerInDebugMode(bool gpuBasedValidation = false)
{
    AGZ_WHEN_DEBUG(enableD3D12DebugLayer(gpuBasedValidation));
}

AGZ_D3D12_END
