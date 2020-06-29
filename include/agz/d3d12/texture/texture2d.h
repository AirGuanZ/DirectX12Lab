#pragma once

#include <d3d12.h>

#include <agz/d3d12/common.h>
#include <agz/utility/texture.h>

AGZ_D3D12_BEGIN

class Texture2D : public misc::uncopyable_t
{
public:

    Texture2D() = default;

    Texture2D(Texture2D &&) noexcept = default;

    Texture2D &operator=(Texture2D &&) noexcept = default;

    void initialize(
        ID3D12Device *device,
        DXGI_FORMAT format,
        int width, int height,
        int depth, int mipCnt,
        int sampleCnt, int sampleQua,
        D3D12_RESOURCE_FLAGS flags,
        D3D12_RESOURCE_STATES states,
        const D3D12_CLEAR_VALUE *clearValue = nullptr);

    bool isAvailable() const noexcept;

    void destroy();

    void createSRV(
        D3D12_CPU_DESCRIPTOR_HANDLE output) const;

    void createSRV(
        DXGI_FORMAT format,
        int mipBeg, int mipCnt,
        D3D12_CPU_DESCRIPTOR_HANDLE output) const;

    void createRTV(
        D3D12_CPU_DESCRIPTOR_HANDLE output) const;

    void createRTV(
        DXGI_FORMAT format,
        int mipIdx,
        D3D12_CPU_DESCRIPTOR_HANDLE output) const;

    void createUAV(
        D3D12_CPU_DESCRIPTOR_HANDLE output) const;

    void createUAV(
        DXGI_FORMAT format,
        int mipIdx,
        D3D12_CPU_DESCRIPTOR_HANDLE output) const;

    DXGI_SAMPLE_DESC getMultisample() const noexcept;

    operator ID3D12Resource *() const noexcept;

    operator ComPtr<ID3D12Resource>() const noexcept;

private:

    ComPtr<ID3D12Device> device_;
    ComPtr<ID3D12Resource> rsc_;
};

AGZ_D3D12_END
