#pragma once

#include <d3dcompiler.h>
#include <d3dx12.h>

#include <agz/d3d12/pipeline/shader.h>

AGZ_D3D12_FG_BEGIN

template<typename Shader>
PipelineShader<Shader>::PipelineShader() noexcept
    : byteCodeDesc{}
{
    
}

template<typename Shader>
PipelineShader<Shader>::PipelineShader(
    const D3D12_SHADER_BYTECODE &byteCodeDesc)
    : byteCodeDesc(byteCodeDesc)
{
    
}

template<typename Shader>
PipelineShader<Shader>::PipelineShader(
    ComPtr<ID3D10Blob> byteCodeBlob)
    : byteCode(std::move(byteCodeBlob)), byteCodeDesc{}
{
    byteCodeDesc.pShaderBytecode = byteCode->GetBufferPointer();
    byteCodeDesc.BytecodeLength  = byteCode->GetBufferSize();
}

template<typename Shader>
PipelineShader<Shader>::PipelineShader(
    std::string_view        sourceCode,
    const char             *target,
    OptLevel                optLevel,
    const D3D_SHADER_MACRO *macros,
    const char             *entry,
    const char             *sourceName,
    ID3DInclude            *includes)
    : byteCodeDesc{}
{
    ShaderCompiler compiler;
    compiler.setOptLevel(optLevel).setWarnings(false);

    byteCode = compiler.compileShader(
        sourceCode, target, macros, entry, sourceName,
        includes ? includes : D3D_COMPILE_STANDARD_FILE_INCLUDE);

    byteCodeDesc.pShaderBytecode = byteCode->GetBufferPointer();
    byteCodeDesc.BytecodeLength  = byteCode->GetBufferSize();
}

inline InputLayout::InputLayout()
{
    
}

inline InputLayout::InputLayout(
    const D3D12_INPUT_ELEMENT_DESC *elems, size_t count) noexcept
    : inputElemsRef(elems, count)
{
    
}

template<int N>
InputLayout::InputLayout(D3D12_INPUT_ELEMENT_DESC ( &elems)[N]) noexcept
    : InputLayout(elems, N)
{
    
}

inline InputLayout::InputLayout(
    std::initializer_list<D3D12_INPUT_ELEMENT_DESC> elems)
    : inputElems(elems)
{
    
}

namespace detail
{

    inline void _initGPS(
        GraphicsPipelineState &gps,
        const VertexShader &vertexShader) noexcept
    {
        gps.vs = vertexShader;
    }

    inline void _initGPS(
        GraphicsPipelineState &gps,
        const PixelShader &pixelShader) noexcept
    {
        gps.ps = pixelShader;
    }

    inline void _initGPS(
        GraphicsPipelineState &gps,
        const DomainShader &domainShader) noexcept
    {
        gps.ds = domainShader;
    }

    inline void _initGPS(
        GraphicsPipelineState &gps,
        const HullShader &hullShader) noexcept
    {
        gps.hs = hullShader;
    }

    inline void _initGPS(
        GraphicsPipelineState &gps,
        const GeometryShader &geometryShader) noexcept
    {
        gps.gs = geometryShader;
    }

    inline void _initGPS(
        GraphicsPipelineState &gps,
        DepthStencilState &dss) noexcept
    {
        gps.depthStencilState = dss;
    }

    inline void _initGPS(
        GraphicsPipelineState &gps,
        InputLayout inputLayout) noexcept
    {
        gps.inputLayout = std::move(inputLayout);
    }

    inline void _initGPS(
        GraphicsPipelineState &gps,
        D3D12_PRIMITIVE_TOPOLOGY_TYPE primTopo) noexcept
    {
        gps.primitiveTopology = primTopo;
    }

    inline void _initGPS(
        GraphicsPipelineState &gps,
        std::initializer_list<DXGI_FORMAT> rtvFormats)
    {
        gps.rtvFormats = rtvFormats;
    }

    inline void _initGPS(
        GraphicsPipelineState &gps,
        const Multisample &multisample) noexcept
    {
        gps.multisample = multisample;
    }

    inline void _initGPS(
        GraphicsPipelineState &gps,
        ComPtr<ID3D12RootSignature> rootSignature) noexcept
    {
        gps.rootSignature.Swap(rootSignature);
    }

} // namespace detail

template<typename ... Args>
GraphicsPipelineState::GraphicsPipelineState(Args &&... args)
    : primitiveTopology(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE)
{
    InvokeAll([&] { detail::_initGPS(*this, std::forward<Args>(args)); }...);
}

inline ComPtr<ID3D12PipelineState> GraphicsPipelineState::createPipelineState(ID3D12Device *device)
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};

    desc.pRootSignature    = rootSignature.Get();
    desc.VS                = vs.byteCodeDesc;
    desc.PS                = ps.byteCodeDesc;
    desc.DS                = ds.byteCodeDesc;
    desc.HS                = hs.byteCodeDesc;
    desc.GS                = gs.byteCodeDesc;
    desc.BlendState        = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    desc.SampleMask        = 0xffffffff;
    desc.RasterizerState   = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);

    if(depthStencilState)
        desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);

    if(!inputLayout.inputElems.empty())
    {
        desc.InputLayout.NumElements =
            static_cast<UINT>(inputLayout.inputElems.size());
        desc.InputLayout.pInputElementDescs =
            inputLayout.inputElems.data();
    }
    else
    {
        desc.InputLayout.NumElements =
            static_cast<UINT>(inputLayout.inputElemsRef.size());
        desc.InputLayout.pInputElementDescs =
            inputLayout.inputElemsRef.data();
    }

    desc.PrimitiveTopologyType = primitiveTopology;

    desc.NumRenderTargets = static_cast<UINT>(rtvFormats.size());
    std::memcpy(
        desc.RTVFormats, rtvFormats.data(),
        sizeof(DXGI_FORMAT) * rtvFormats.size());

    if(depthStencilState)
        desc.DSVFormat = depthStencilState->format;
    else
        desc.DSVFormat = DXGI_FORMAT_UNKNOWN;

    desc.SampleDesc = { multisample.count, multisample.quality };

    ComPtr<ID3D12PipelineState> pipelineState;
    AGZ_D3D12_CHECK_HR(
        device->CreateGraphicsPipelineState(
            &desc, IID_PPV_ARGS(pipelineState.GetAddressOf())));

    return pipelineState;
}

namespace detail
{

    inline void _initCPS(
        ComputePipeline &p,
        const ComputeShader &computeShader)
    {
        p.cs = computeShader;
    }

    inline void _initCPS(
        ComputePipeline &p,
        ComPtr<ID3D12RootSignature> rs)
    {
        p.rootSignature = rs;
    }

} // namespace detail

template<typename ... Args>
ComputePipeline::ComputePipeline(Args &&... args)
{
    InvokeAll([&] { detail::_initCPS(*this, std::forward<Args>(args)); }...);
}

inline ComPtr<ID3D12PipelineState> ComputePipeline::createPipelineState(
    ID3D12Device *device) const
{
    D3D12_COMPUTE_PIPELINE_STATE_DESC desc = {};
    desc.pRootSignature = rootSignature.Get();
    desc.CS             = cs.byteCodeDesc;

    ComPtr<ID3D12PipelineState> ret;
    AGZ_D3D12_CHECK_HR(
        device->CreateComputePipelineState(
            &desc, IID_PPV_ARGS(ret.GetAddressOf())));

    return ret;
}

AGZ_D3D12_FG_END
