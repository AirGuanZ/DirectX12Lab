#pragma once

#include <agz/d3d12/framegraph/common.h>

AGZ_D3D12_FG_BEGIN

static constexpr UINT ALL_MIPMAPS = -1;

struct ArraySlices
{
    UINT firstElem = 0;
    UINT elemCount = 1;
};

struct MipmapSlice
{
    UINT sliceIdx = 0;
};

struct MipmapSlices
{
    UINT mostDetailed = 0;
    UINT mipmapCount  = -1;
    float minLODClamp = 0;
};

AGZ_D3D12_FG_END
