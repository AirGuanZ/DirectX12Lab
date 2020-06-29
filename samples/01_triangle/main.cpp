#include <iostream>

#include <d3dx12.h>

#include <agz/d3d12/lab.h>

using namespace agz::d3d12;

const char *VERTEX_SHADER_SOURCE = R"___(
struct VSInput
{
    float3 pos   : POSITION;
    float3 color : COLOR;
};

struct VSOutput
{
    float4 pos   : SV_POSITION;
    float3 color : COLOR;
};

VSOutput main(VSInput input)
{
    VSOutput output = (VSOutput)0;
    output.pos   = float4(input.pos, 1);
    output.color = input.color;
    return output;
}
)___";

const char *PIXEL_SHADER_SOURCE = R"___(
struct PSInput
{
    float4 pos   : SV_POSITION;
    float3 color : COLOR;
};

float4 main(PSInput input) : SV_TARGET
{
    return float4(input.color, 1);
}
)___";

void run()
{
    enableD3D12DebugLayerInDebugMode();

    WindowDesc desc;
    desc.title = L"01.triangle";

    Window window(desc);

    window.getKeyboard()->attach(
        std::make_shared<KeyDownHandler>(
            [&](const auto &e)
    {
        if(e.key == KEY_ESCAPE)
            window.setCloseFlag(true);
    }));

    // cmd list

    PerFrameCommandList cmdList(window);

    // root signature

    CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Init(
        0, nullptr, 0, nullptr,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ComPtr<ID3D10Blob> rootSignatureBlob;
    AGZ_D3D12_CHECK_HR(D3D12SerializeRootSignature(
        &rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1,
        rootSignatureBlob.GetAddressOf(), nullptr));

    ComPtr<ID3D12RootSignature> rootSignature;
    AGZ_D3D12_CHECK_HR(window.getDevice()->CreateRootSignature(
        0,
        rootSignatureBlob->GetBufferPointer(),
        rootSignatureBlob->GetBufferSize(),
        IID_PPV_ARGS(rootSignature.GetAddressOf())));

    // pipeline state

    struct Vertex
    {
        Vec3 pos;
        Vec3 color;
    };

    D3D12_INPUT_ELEMENT_DESC inputDescs[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
          D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, color),
          D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
    };

    ShaderCompiler compiler;
    compiler
        .setOptLevel(ShaderCompiler::OptLevel::Debug)
        .setWarnings(true);

    auto pipeline = GraphicsPipelineStateBuilder(window.getDevice())
        .setRootSignature(rootSignature)
        .setVertexShader(compiler.compileShader(VERTEX_SHADER_SOURCE, "vs_5_0"))
        .setPixelShader(compiler.compileShader(PIXEL_SHADER_SOURCE, "ps_5_0"))
        .setRenderTargetFormats({ desc.backbufferFormat })
        .setInputElements(inputDescs)
        .createPipelineState();

    // vertex buffer

    const Vertex vertexData[] =
    {
        { { -0.5f, -0.5f, +0.5f }, { 1, 0, 0 } },
        { { +0.0f, +0.5f, +0.5f }, { 0, 1, 0 } },
        { { +0.5f, -0.5f, +0.5f }, { 0, 0, 1 } }
    };

    ResourceUploader uploader(window, 1);

    VertexBuffer<Vertex> vertexBuffer;
    vertexBuffer.initializeDefault(
        window.getDevice(), 3, D3D12_RESOURCE_STATE_COMMON);

    uploader.uploadBufferData(
        vertexBuffer, vertexData,
        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

    uploader.waitForIdle();

    // mainloop

    while(!window.getCloseFlag())
    {
        window.doEvents();
        window.waitForFocus();

        cmdList.startFrame();
        cmdList.resetCommandList();

        // record command list

        const auto barrier1 = CD3DX12_RESOURCE_BARRIER::Transition(
            window.getCurrentImage(),
            D3D12_RESOURCE_STATE_PRESENT,
            D3D12_RESOURCE_STATE_RENDER_TARGET);
        cmdList->ResourceBarrier(1, &barrier1);

        auto rtvHandle = window.getCurrentImageDescHandle();
        cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

        const float CLEAR_COLOR[] = { 0, 0, 0, 0 };
        cmdList->ClearRenderTargetView(rtvHandle, CLEAR_COLOR, 0, nullptr);

        const auto vertexBufferView = vertexBuffer.getView();

        cmdList->SetGraphicsRootSignature(rootSignature.Get());
        cmdList->SetPipelineState(pipeline.Get());

        cmdList->RSSetViewports(1, &window.getDefaultViewport());
        cmdList->RSSetScissorRects(1, &window.getDefaultScissorRect());

        cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        cmdList->IASetVertexBuffers(0, 1, &vertexBufferView);

        cmdList->DrawInstanced(3, 1, 0, 0);

        const auto barrier2 = CD3DX12_RESOURCE_BARRIER::Transition(
            window.getCurrentImage(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PRESENT);
        cmdList->ResourceBarrier(1, &barrier2);

        AGZ_D3D12_CHECK_HR(cmdList->Close());

        // render

        window.executeOneCmdList(cmdList.getCmdList());

        // present

        window.present();
        cmdList.endFrame();
    }

    window.waitCommandQueueIdle();
}

int main()
{
    try
    {
        run();
    }
    catch(const std::exception &e)
    {
        std::cerr << e.what() << std::endl;
        return -1;
    }
}
