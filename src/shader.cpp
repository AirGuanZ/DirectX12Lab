#include <d3dcompiler.h>

#include <agz/d3d12/pipeline/shader.h>

AGZ_D3D12_LAB_BEGIN

ShaderCompiler::ShaderCompiler()
{
    optLevel_            = OptLevel::Debug;
    treatWarningAsError_ = true;
    flag_                = 0;
    updateFlag();
}

ShaderCompiler &ShaderCompiler::setOptLevel(OptLevel level) noexcept
{
    optLevel_ = level;
    updateFlag();
    return *this;
}

ShaderCompiler &ShaderCompiler::setWarnings(bool asError) noexcept
{
    treatWarningAsError_ = asError;
    updateFlag();
    return *this;
}

ComPtr<ID3D10Blob> ShaderCompiler::compileShader(
    std::string_view        sourceCode,
    const char             *target,
    const D3D_SHADER_MACRO *macros,
    const char             *entry,
    const char             *sourceName,
    ID3DInclude            *includes)
{
    ComPtr<ID3D10Blob> byteCode;
    ComPtr<ID3D10Blob> errMsg;

    const HRESULT hr = D3DCompile(
        sourceCode.data(),
        sourceCode.size(),
        sourceName,
        macros,
        includes,
        entry,
        target,
        flag_,
        0,
        byteCode.GetAddressOf(),
        errMsg.GetAddressOf());

    warnings_.clear();

    if(FAILED(hr))
    {
        if(!errMsg)
            throw D3D12LabException("failed to compile d3d shader");

        std::string errStr(
            reinterpret_cast<const char *>(errMsg->GetBufferPointer()));
        throw D3D12LabException(errStr);
    }

    if(errMsg)
        warnings_ = reinterpret_cast<const char *>(errMsg->GetBufferPointer());

    return byteCode;
}

const std::string &ShaderCompiler::getWarnings() const noexcept
{
    return warnings_;
}

void ShaderCompiler::updateFlag()
{
    switch(optLevel_)
    {
    case OptLevel::Debug:
        flag_ = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
        break;
    case OptLevel::O0:
        flag_ = D3DCOMPILE_OPTIMIZATION_LEVEL0;
        break;
    case OptLevel::O1:
        flag_ = D3DCOMPILE_OPTIMIZATION_LEVEL1;
        break;
    case OptLevel::O2:
        flag_ = D3DCOMPILE_OPTIMIZATION_LEVEL2;
        break;
    case OptLevel::O3:
        flag_ = D3DCOMPILE_OPTIMIZATION_LEVEL3;
        break;
    }

    if(treatWarningAsError_)
        flag_ |= D3DCOMPILE_WARNINGS_ARE_ERRORS;
}

D3D12_SHADER_BYTECODE blobToByteCode(ComPtr<ID3D10Blob> blob)
{
    D3D12_SHADER_BYTECODE ret;
    ret.pShaderBytecode = blob->GetBufferPointer();
    ret.BytecodeLength  = blob->GetBufferSize();
    return ret;
}

AGZ_D3D12_LAB_END
