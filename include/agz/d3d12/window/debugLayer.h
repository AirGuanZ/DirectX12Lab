#pragma once

#include <agz/d3d12/common.h>
#include <agz/utility/system.h>

AGZ_D3D12_LAB_BEGIN

void enableD3D12DebugLayer();

inline void enableD3D12DebugLayerInDebugMode()
{
    AGZ_WHEN_DEBUG(enableD3D12DebugLayer());
}

AGZ_D3D12_LAB_END
