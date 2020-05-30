#pragma once

#include <d3d12.h>

#include <agz/d3d12/common.h>

AGZ_D3D12_LAB_BEGIN

class GraphicsPipelineStateBuilder
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc_;

public:

    GraphicsPipelineStateBuilder();

    ComPtr<ID3D12PipelineState> createPipelineState(
        ID3D12Device *device) const;

    void setInputLayoutElements(
        const D3D12_INPUT_ELEMENT_DESC *elems, int count);

    void setRenderTargetFormats(
        std::initializer_list<DXGI_FORMAT> formats);

    void setRenderTargetFormats(
        const DXGI_FORMAT *formats, int count);

    void setDepthStencilBufferFormat(
        DXGI_FORMAT FORMAT);
};

AGZ_D3D12_LAB_END
