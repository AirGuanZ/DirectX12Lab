#pragma once

#include <d3d12.h>

#include <agz/d3d12/framegraph/common.h>

AGZ_D3D12_FG_BEGIN

struct _internalNoViewport { };
struct _internalNoScissor  { };

constexpr _internalNoViewport NO_VIEWPORT = {};
constexpr _internalNoScissor  NO_SCISSOR  = {};

using Viewport = D3D12_VIEWPORT;
using Scissor  = D3D12_RECT;

AGZ_D3D12_FG_END
