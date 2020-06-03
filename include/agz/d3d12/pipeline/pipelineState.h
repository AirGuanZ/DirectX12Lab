#pragma once

#include <d3d12.h>
#include <d3dx12.h>

#include <agz/d3d12/pipeline/shader.h>

AGZ_D3D12_LAB_BEGIN

class InputLayoutBuilder
{
    std::vector<D3D12_INPUT_ELEMENT_DESC> elemDescs_;

public:

    InputLayoutBuilder &operator()(
        const D3D12_INPUT_ELEMENT_DESC &desc);

    InputLayoutBuilder &append(
        const D3D12_INPUT_ELEMENT_DESC &desc) { return (*this)(desc); }

    InputLayoutBuilder &set(const D3D12_INPUT_ELEMENT_DESC *elems, int count);

    InputLayoutBuilder &clear();

    D3D12_INPUT_LAYOUT_DESC getDesc() const noexcept;
};

class GraphicsPipelineStateBuilder : misc::uncopyable_t
{
    ID3D12Device *device_;

    mutable D3D12_GRAPHICS_PIPELINE_STATE_DESC desc_;

    InputLayoutBuilder inputLayout_;

    ComPtr<ID3D12RootSignature> rootSignature_;

    ComPtr<ID3D10Blob> vertexShader_;
    ComPtr<ID3D10Blob> pixelShader_;

public:

    explicit GraphicsPipelineStateBuilder(ID3D12Device *device);

    ComPtr<ID3D12PipelineState> createPipelineState() const;

    // root signature

    GraphicsPipelineStateBuilder &setRootSignature(
        ComPtr<ID3D12RootSignature> rs) noexcept;

    // input layout

    InputLayoutBuilder &getInputLayoutBuilder() noexcept;

    GraphicsPipelineStateBuilder &setInputElements(
        const D3D12_INPUT_ELEMENT_DESC *descs, int count);

    template<int N>
    GraphicsPipelineStateBuilder &setInputElements(
        const D3D12_INPUT_ELEMENT_DESC (&descs)[N]);

    // input assembler

    GraphicsPipelineStateBuilder &setPrimitiveTopology(
        D3D12_PRIMITIVE_TOPOLOGY_TYPE topologyType) noexcept;

    // render target

    GraphicsPipelineStateBuilder &setRenderTargetFormats(
        std::initializer_list<DXGI_FORMAT> formats) noexcept;

    GraphicsPipelineStateBuilder &setRenderTargetFormats(
        const DXGI_FORMAT *formats, int count) noexcept;

    GraphicsPipelineStateBuilder &setDepthStencilBufferFormat(
        DXGI_FORMAT format) noexcept;

    // multisample

    GraphicsPipelineStateBuilder &setMultisample(
        int count, int quality) noexcept;

    GraphicsPipelineStateBuilder &setMultisample(
        const DXGI_SAMPLE_DESC &desc) noexcept;

    // shader stage

    GraphicsPipelineStateBuilder &setVertexShader(
        const D3D12_SHADER_BYTECODE &byteCode) noexcept;

    GraphicsPipelineStateBuilder &setVertexShader(
        ComPtr<ID3D10Blob> blob) noexcept;

    GraphicsPipelineStateBuilder &setPixelShader(
        const D3D12_SHADER_BYTECODE &byteCode) noexcept;

    GraphicsPipelineStateBuilder &setPixelShader(
        ComPtr<ID3D10Blob> blob) noexcept;

    // rasterizer state

    GraphicsPipelineStateBuilder &setRasterizerState(
        D3D12_RASTERIZER_DESC &desc) noexcept;

    // blend state

    GraphicsPipelineStateBuilder &setBlendState(
        D3D12_BLEND_DESC &desc) noexcept;
};

inline InputLayoutBuilder &InputLayoutBuilder::operator()(
    const D3D12_INPUT_ELEMENT_DESC &desc)
{
    elemDescs_.push_back(desc);
    return *this;
}

inline class InputLayoutBuilder &InputLayoutBuilder::set(
    const D3D12_INPUT_ELEMENT_DESC *elems, int count)
{
    elemDescs_ = { elems, elems + count };
    return *this;
}

inline InputLayoutBuilder &InputLayoutBuilder::clear()
{
    elemDescs_.clear();
    return *this;
}

inline D3D12_INPUT_LAYOUT_DESC InputLayoutBuilder::getDesc() const noexcept
{
    D3D12_INPUT_LAYOUT_DESC desc;
    desc.NumElements        = UINT(elemDescs_.size());
    desc.pInputElementDescs = elemDescs_.data();
    return desc;
}

inline GraphicsPipelineStateBuilder::GraphicsPipelineStateBuilder(
    ID3D12Device *device)
    : device_(device), desc_{}
{
    desc_.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    desc_.SampleDesc.Count      = 1;
    desc_.SampleDesc.Quality    = 0;
    desc_.SampleMask            = 0xffffffff;
    desc_.RasterizerState       = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    desc_.BlendState            = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
}

inline ComPtr<ID3D12PipelineState>
    GraphicsPipelineStateBuilder::createPipelineState() const
{
    desc_.InputLayout    = inputLayout_.getDesc();
    desc_.pRootSignature = rootSignature_.Get();

    ComPtr<ID3D12PipelineState> pipeline;
    AGZ_D3D12_CHECK_HR(
        device_->CreateGraphicsPipelineState(
            &desc_, IID_PPV_ARGS(pipeline.GetAddressOf())));

    return pipeline;
}

inline GraphicsPipelineStateBuilder &GraphicsPipelineStateBuilder::setRootSignature(
    ComPtr<ID3D12RootSignature> rs) noexcept
{
    rootSignature_.Swap(rs);
    return *this;
}

inline InputLayoutBuilder &
    GraphicsPipelineStateBuilder::getInputLayoutBuilder() noexcept
{
    return inputLayout_;
}

inline class GraphicsPipelineStateBuilder &
    GraphicsPipelineStateBuilder::setInputElements(
        const D3D12_INPUT_ELEMENT_DESC *descs, int count)
{
    inputLayout_.set(descs, count);
    return *this;
}

template<int N>
GraphicsPipelineStateBuilder &
    GraphicsPipelineStateBuilder::setInputElements(
        const D3D12_INPUT_ELEMENT_DESC (&descs)[N])
{
    this->setInputElements(descs, N);
    return *this;
}

inline GraphicsPipelineStateBuilder &
    GraphicsPipelineStateBuilder::setPrimitiveTopology(
        D3D12_PRIMITIVE_TOPOLOGY_TYPE topologyType) noexcept
{
    desc_.PrimitiveTopologyType = topologyType;
    return *this;
}

inline GraphicsPipelineStateBuilder &
    GraphicsPipelineStateBuilder::setRenderTargetFormats(
        std::initializer_list<DXGI_FORMAT> formats) noexcept
{
    desc_.NumRenderTargets = UINT(formats.size());
    int i = 0;
    for(auto f : formats)
        desc_.RTVFormats[i++] = f;
    return *this;
}

inline GraphicsPipelineStateBuilder &
    GraphicsPipelineStateBuilder::setRenderTargetFormats(
        const DXGI_FORMAT *formats, int count) noexcept
{
    desc_.NumRenderTargets = UINT(count);
    for(int i = 0; i < count; ++i)
        desc_.RTVFormats[i] = formats[i];
    return *this;
}

inline GraphicsPipelineStateBuilder &
    GraphicsPipelineStateBuilder::setDepthStencilBufferFormat(
        DXGI_FORMAT format) noexcept
{
    desc_.DSVFormat = format;
    return *this;
}

inline GraphicsPipelineStateBuilder &
    GraphicsPipelineStateBuilder::setMultisample(
        int count, int quality) noexcept
{
    desc_.SampleDesc = { UINT(count), UINT(quality) };
    return *this;
}

inline class GraphicsPipelineStateBuilder &
    GraphicsPipelineStateBuilder::setMultisample(
        const DXGI_SAMPLE_DESC &desc) noexcept
{
    desc_.SampleDesc = desc;
    return *this;
}

inline GraphicsPipelineStateBuilder &
    GraphicsPipelineStateBuilder::setVertexShader(
        ComPtr<ID3D10Blob> blob) noexcept
{
    vertexShader_.Swap(blob);
    desc_.VS = blobToByteCode(vertexShader_);
    return *this;
}

inline GraphicsPipelineStateBuilder &
    GraphicsPipelineStateBuilder::setVertexShader(
        const D3D12_SHADER_BYTECODE &byteCode) noexcept
{
    vertexShader_.Reset();
    desc_.VS = byteCode;
    return *this;
}

inline GraphicsPipelineStateBuilder &
    GraphicsPipelineStateBuilder::setPixelShader(
        ComPtr<ID3D10Blob> blob) noexcept
{
    pixelShader_.Swap(blob);
    desc_.PS = blobToByteCode(pixelShader_);
    return *this;
}

inline GraphicsPipelineStateBuilder &
    GraphicsPipelineStateBuilder::setPixelShader(
        const D3D12_SHADER_BYTECODE &byteCode) noexcept
{
    pixelShader_.Reset();
    desc_.PS = byteCode;
    return *this;
}

inline GraphicsPipelineStateBuilder &
    GraphicsPipelineStateBuilder::setRasterizerState(
        D3D12_RASTERIZER_DESC &desc) noexcept
{
    desc_.RasterizerState = desc;
    return *this;
}

inline GraphicsPipelineStateBuilder &
    GraphicsPipelineStateBuilder::setBlendState(
        D3D12_BLEND_DESC &desc) noexcept
{
    desc_.BlendState = desc;
    return *this;
}

AGZ_D3D12_LAB_END
