#pragma once

#include <optional>

#include <d3d12.h>

#include <agz/d3d12/framegraph/common.h>
#include <agz/d3d12/framegraph/resourceDesc.h>
#include <agz/d3d12/pipeline/shader.h>
#include <agz/utility/file.h>
#include <agz/utility/misc.h>

AGZ_D3D12_FG_BEGIN

template<typename Shader>
struct PipelineShader
{
    using OptLevel = ShaderCompiler::OptLevel;

#ifdef AGZ_DEBUG
    static constexpr OptLevel DEFAULT_OPT = OptLevel::Debug;
#else
    static constexpr OptLevel DEFAULT_OPT = OptLevel::O3;
#endif

    PipelineShader() noexcept;

    explicit PipelineShader(const D3D12_SHADER_BYTECODE &byteCodeDesc);

    explicit PipelineShader(ComPtr<ID3D10Blob> byteCode);

    PipelineShader(
        std::string_view        sourceCode,
        const char             *target,
        OptLevel                optLevel   = DEFAULT_OPT,
        const D3D_SHADER_MACRO *macros     = nullptr,
        const char             *entry      = "main",
        const char             *sourceName = nullptr,
        ID3DInclude            *includes   = nullptr);

    static Shader loadFromFile(
        const char             *filename,
        const char             *target,
        OptLevel                optLevel   = DEFAULT_OPT,
        const D3D_SHADER_MACRO *macros     = nullptr,
        const char             *entry      = "main",
        ID3DInclude            *includes   = nullptr)
    {
        const auto src = file::read_txt_file(filename);
        return Shader(src, target, optLevel, macros, entry, filename, includes);
    }

    ComPtr<ID3D10Blob>    byteCode;
    D3D12_SHADER_BYTECODE byteCodeDesc;
};

struct VertexShader   : PipelineShader<VertexShader  > { using PipelineShader::PipelineShader; };
struct PixelShader    : PipelineShader<PixelShader   > { using PipelineShader::PipelineShader; };
struct DomainShader   : PipelineShader<DomainShader  > { using PipelineShader::PipelineShader; };
struct HullShader     : PipelineShader<HullShader    > { using PipelineShader::PipelineShader; };
struct GeometryShader : PipelineShader<GeometryShader> { using PipelineShader::PipelineShader; };
struct ComputeShader  : PipelineShader<ComputeShader > { using PipelineShader::PipelineShader; };

struct DepthStencilState
{
    DXGI_FORMAT format;
};

struct InputLayout
{
    InputLayout();

    InputLayout(const D3D12_INPUT_ELEMENT_DESC *elems, size_t count) noexcept;

    template<int N>
    explicit InputLayout(D3D12_INPUT_ELEMENT_DESC(&elems)[N]) noexcept;

    explicit InputLayout(std::initializer_list<D3D12_INPUT_ELEMENT_DESC> elems);

    std::vector<D3D12_INPUT_ELEMENT_DESC>      inputElems;
    misc::span<const D3D12_INPUT_ELEMENT_DESC> inputElemsRef;
};

using PipelineRTVFormats = std::initializer_list<DXGI_FORMAT>;

// Args: XXXShader
//       DepthStencilState                            : disabled
//       InputLayout                                  : no input
//       D3D12_PRIMITIVE_TOPOLOGY_TYPE                : triangle
//       std::initializer_list<DXGI_FORMAT> rtvFormat : necessary
//       Multisample                                  : { 1, 0 }
struct GraphicsPipelineState
{
    template<typename...Args>
    explicit GraphicsPipelineState(Args &&...args);

    GraphicsPipelineState(const GraphicsPipelineState &) = default;

    ComPtr<ID3D12PipelineState> createPipelineState(ID3D12Device *device);

    VertexShader   vs;
    PixelShader    ps;
    DomainShader   ds;
    HullShader     hs;
    GeometryShader gs;

    std::optional<DepthStencilState> depthStencilState;

    InputLayout inputLayout;
    D3D12_PRIMITIVE_TOPOLOGY_TYPE primitiveTopology;

    std::vector<DXGI_FORMAT> rtvFormats;
    
    Multisample multisample;

    ComPtr<ID3D12RootSignature> rootSignature;
};

struct ComputePipeline
{
    template<typename...Args>
    explicit ComputePipeline(Args &&...args);

    ComputePipeline(const ComputePipeline &) = default;

    ComputePipeline &operator=(const ComputePipeline &) = default;

    ComPtr<ID3D12PipelineState> createPipelineState(
        ID3D12Device *device) const;

    ComputeShader cs;

    ComPtr<ID3D12RootSignature> rootSignature;
};

AGZ_D3D12_FG_END

#include "./impl/pipelineState.inl"
