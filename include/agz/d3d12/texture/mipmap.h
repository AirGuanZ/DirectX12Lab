#pragma once

#include <agz/d3d12/common.h>
#include <agz/utility/texture.h>

AGZ_D3D12_BEGIN

int computeMaxMipmapChainLength(int width, int height) noexcept;

std::vector<texture::texture2d_t<math::color4b>> constructMipmapChain(
    texture::texture2d_t<math::color4b> lod0Data, int maxChainLength);

AGZ_D3D12_END
