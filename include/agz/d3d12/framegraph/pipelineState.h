#pragma once

#include <optional>

#include <d3d12.h>

#include <agz/d3d12/framegraph/common.h>
#include <agz/d3d12/framegraph/resourceDesc.h>
#include <agz/d3d12/pipeline/shader.h>
#include <agz/utility/misc.h>

AGZ_D3D12_FG_BEGIN

struct PipelineShader
{
    using OptLevel = ShaderCompiler::OptLevel;

    explicit PipelineShader(const D3D12_SHADER_BYTECODE &byteCodeDesc);

    explicit PipelineShader(ComPtr<ID3D10Blob> byteCode);

    PipelineShader(
        std::string_view        sourceCode,
#ifdef AGZ_DEBUG
        OptLevel                optLevel = OptLevel::O3,
#else
        OptLevel                optLevel = OptLevel::Debug,
#endif
        const char             *target,
        const D3D_SHADER_MACRO *macros     = nullptr,
        const char             *entry      = "main",
        const char             *sourceName = nullptr,
        ID3DInclude            *includes   = nullptr);

    D3D12_SHADER_BYTECODE byteCodeDesc;
    ComPtr<ID3D10Blob>    byteCode;
};

struct VertexShader   : PipelineShader { using PipelineShader::PipelineShader; };
struct PixelShader    : PipelineShader { using PipelineShader::PipelineShader; };
struct DomainShader   : PipelineShader { using PipelineShader::PipelineShader; };
struct HullShader     : PipelineShader { using PipelineShader::PipelineShader; };
struct GeometryShader : PipelineShader { using PipelineShader::PipelineShader; };

struct DepthStencilState
{

};

struct InputLayout
{
    InputLayout();

    InputLayout(const D3D12_INPUT_ELEMENT_DESC *elems, size_t count) noexcept;

    template<int N>
    InputLayout(D3D12_INPUT_ELEMENT_DESC(&elems)[N]) noexcept;

    InputLayout(std::initializer_list<D3D12_INPUT_ELEMENT_DESC> elems);

    std::vector<D3D12_INPUT_ELEMENT_DESC>      inputElems;
    misc::span<const D3D12_INPUT_ELEMENT_DESC> inputElemsRef;
};

struct GraphicsPipelineState
{
    // Args: XXXShader
    //       DepthStencilState                            : disabled
    //       InputLayout                                  : no input
    //       D3D12_PRIMITIVE_TOPOLOGY_TYPE                : triangle
    //       std::initializer_list<DXGI_FORMAT> rtvFormat : inferred by compiler
    //       DXGI_FORMAT dsvFormat                        : inferred by compiler
    //       Multisample                                  : inferred by compiler
    template<typename...Args>
    GraphicsPipelineState(const Args&&...args);

    GraphicsPipelineState(const GraphicsPipelineState &) = default;

    VertexShader   vs;
    PixelShader    ps;
    DomainShader   ds;
    HullShader     hs;
    GeometryShader gs;

    std::optional<DepthStencilState> depthStencilState;

    InputLayout inputLayout;
    D3D12_PRIMITIVE_TOPOLOGY_TYPE primitiveTopology;

    std::vector<DXGI_FORMAT> rtvFormats;
    std::optional<DXGI_FORMAT> dsvFormat;
    
    Multisample multisample;
};

AGZ_D3D12_FG_END
