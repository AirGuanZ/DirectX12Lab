#pragma once

#include <d3d12.h>

#include <agz/d3d12/common.h>

AGZ_D3D12_LAB_BEGIN

class ShaderCompiler
{
public:

    enum class OptLevel
    {
        Debug,
        O0, O1, O2, O3,
    };

    explicit ShaderCompiler();

    ShaderCompiler &setOptLevel(OptLevel level) noexcept;

    ShaderCompiler &setWarnings(bool asError) noexcept;

    ComPtr<ID3D10Blob> compileShader(
        std::string_view        sourceCode,
        const char             *target,
        const D3D_SHADER_MACRO *macros     = nullptr,
        const char             *entry      = "main",
        const char             *sourceName = nullptr,
        ID3DInclude            *includes   = nullptr);

    const std::string &getWarnings() const noexcept;

private:

    void updateFlag();

    OptLevel optLevel_;
    bool treatWarningAsError_;

    UINT flag_;
    std::string warnings_;
};

D3D12_SHADER_BYTECODE blobToByteCode(ComPtr<ID3D10Blob> blob);

AGZ_D3D12_LAB_END
