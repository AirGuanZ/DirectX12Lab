#include <iostream>

#include <d3dx12.h>

#include <agz/d3d12/debugLayer.h>
#include <agz/d3d12/perFrameCmdList.h>
#include <agz/d3d12/shader.h>
#include <agz/d3d12/vertexBuffer.h>
#include <agz/d3d12/window.h>

using namespace agz::d3d12;

const char *VERTEX_SHADER_SOURCE = R"___(
float4 main(float3 pos : POSITION) : SV_POSITION
{
    return float4(pos, 1.0);
}
)___";

const char *PIXEL_SHADER_SOURCE = R"___(
float4 main() : SV_TARGET
{
    return float4(1.0, 1.0, 0.0, 1.0);
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

    // shader bytecode

    ShaderCompiler compiler;
    compiler
        .setOptLevel(ShaderCompiler::OptLevel::Debug)
        .setWarnings(true);

    ComPtr<ID3D10Blob> vertexShader = compiler.compileShader(
        VERTEX_SHADER_SOURCE, "vs_5_0");
    ComPtr<ID3D10Blob> pixelShader = compiler.compileShader(
        PIXEL_SHADER_SOURCE, "ps_5_0");

    // pipeline state

    D3D12_INPUT_ELEMENT_DESC inputDescs[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc = {};
    pipelineDesc.pRootSignature                 = rootSignature.Get();
    pipelineDesc.VS                             = blobToByteCode(vertexShader);
    pipelineDesc.PS                             = blobToByteCode(pixelShader);
    pipelineDesc.PrimitiveTopologyType          = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pipelineDesc.NumRenderTargets               = 1;
    pipelineDesc.RTVFormats[0]                  = desc.backbufferFormat;
    pipelineDesc.InputLayout.NumElements        = 1;
    pipelineDesc.InputLayout.pInputElementDescs = inputDescs;
    pipelineDesc.SampleDesc.Count               = desc.multisampleCount;
    pipelineDesc.SampleDesc.Quality             = desc.multisampleQuality;
    pipelineDesc.SampleMask                     = 0xffffffff;
    pipelineDesc.RasterizerState                = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    pipelineDesc.BlendState                     = CD3DX12_BLEND_DESC(D3D12_DEFAULT);

    ComPtr<ID3D12PipelineState> pipeline;
    AGZ_D3D12_CHECK_HR(
        window.getDevice()->CreateGraphicsPipelineState(
            &pipelineDesc, IID_PPV_ARGS(pipeline.GetAddressOf())));

    // vertex buffer

    struct Vertex
    {
        Vec3 pos;
    };

    const Vertex vertexData[] =
    {
        { { -0.5f, -0.5f, +0.5f } },
        { { +0.0f, +0.5f, +0.5f } },
        { { +0.5f, -0.5f, +0.5f } }
    };

    cmdList.resetCommandList();

    VertexBuffer<Vertex> vertexBuffer;
    auto uploadBuf = vertexBuffer.initializeStatic(
        window.getDevice(),
        cmdList.getCmdList(),
        3,
        vertexData);

    cmdList->Close();

    ID3D12CommandList *copyCmdLists[] = { cmdList.getCmdList() };
    window.getCommandQueue()->ExecuteCommandLists(1, copyCmdLists);

    window.waitCommandQueueIdle();

    // mainloop

    while(!window.getCloseFlag())
    {
        window.doEvents();
        window.waitForFocus();

        cmdList.startFrame();
        cmdList.resetCommandList();

        // record command list

        const auto barrier1 = CD3DX12_RESOURCE_BARRIER::Transition(
            window.getImage(cmdList.getFrameIndex()),
            D3D12_RESOURCE_STATE_PRESENT,
            D3D12_RESOURCE_STATE_RENDER_TARGET);
        cmdList->ResourceBarrier(1, &barrier1);

        auto rtvHandle = window.getImageDescHandle(cmdList.getFrameIndex());
        cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

        const float CLEAR_COLOR[] = { 0, 1, 1, 0 };
        cmdList->ClearRenderTargetView(rtvHandle, CLEAR_COLOR, 0, nullptr);

        D3D12_VIEWPORT viewport;
        viewport.TopLeftX = 0;
        viewport.TopLeftY = 0;
        viewport.Width    = static_cast<float>(window.getImageWidth());
        viewport.Height   = static_cast<float>(window.getImageHeight());
        viewport.MinDepth = 0;
        viewport.MaxDepth = 1;

        D3D12_RECT scissor;
        scissor.left   = 0;
        scissor.top    = 0;
        scissor.right  = window.getImageWidth();
        scissor.bottom = window.getImageHeight();

        const auto vertexBufferView = vertexBuffer.getView();

        cmdList->SetGraphicsRootSignature(rootSignature.Get());
        cmdList->SetPipelineState(pipeline.Get());

        cmdList->RSSetViewports(1, &viewport);
        cmdList->RSSetScissorRects(1, &scissor);

        cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        cmdList->IASetVertexBuffers(0, 1, &vertexBufferView);

        cmdList->DrawInstanced(3, 1, 0, 0);

        const auto barrier2 = CD3DX12_RESOURCE_BARRIER::Transition(
            window.getImage(cmdList.getFrameIndex()),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PRESENT);
        cmdList->ResourceBarrier(1, &barrier2);

        AGZ_D3D12_CHECK_HR(cmdList->Close());

        // render

        ID3D12CommandList *cmdListArr[] = { cmdList.getCmdList() };
        window.getCommandQueue()->ExecuteCommandLists(1, cmdListArr);

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
