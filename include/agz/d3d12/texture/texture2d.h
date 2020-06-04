#pragma once

#include <d3d12.h>

#include <agz/d3d12/common.h>
#include <agz/utility/texture.h>

AGZ_D3D12_BEGIN

struct LoadTextureResult
{
    ComPtr<ID3D12Resource> textureResource;
    ComPtr<ID3D12Resource> uploadResource;
};

LoadTextureResult  loadTexture2DFromMemory(
    ID3D12Device                              *device,
    ID3D12GraphicsCommandList                 *cmdList,
    const texture::texture2d_t<math::color4b> &data);

AGZ_D3D12_END
