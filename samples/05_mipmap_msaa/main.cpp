#include <iostream>

#include <agz/d3d12/lab.h>
#include <agz/utility/image.h>

using namespace agz::d3d12;

const char *VERTEX_SHADER = R"___(
cbuffer Transform : register(b0)
{
    float4x4 WVP;
};

struct VSInput
{
    float3 pos : POSITION;
    float2 uv  : TEXCOORD;
};

struct VSOutput
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD;
};

VSOutput main(VSInput input)
{
    VSOutput output = (VSOutput)0;
    output.pos = mul(float4(input.pos, 1), WVP);
    output.uv  = input.uv;
    return output;
}
)___";

const char *PIXEL_SHADER = R"___(
Texture2D<float4> Tex : register(t0);
SamplerState      Sam : register(s0);

struct PSInput
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD;
};

float4 main(PSInput input) : SV_TARGET
{
    return Tex.Sample(Sam, input.uv);
}
)___";

void run()
{
    enableD3D12DebugLayerInDebugMode();

    WindowDesc windowDesc;
    windowDesc.title = L"05.mipmap & msaa";

    Window window(windowDesc);

    auto device   = window.getDevice();

    // cmd list

    PerFrameCommandList frameCmdList(window);
    GraphicsCommandList uploadCmdList(device);
    uploadCmdList.resetCommandList();

    // rsc desc heap

    DescriptorHeap rscHeap;
    rscHeap.initialize(
        device, 100, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);

    // vertex buffer

    struct Vertex
    {
        Vec3 pos;
        Vec2 uv;
    };

    const Vertex vertexData[] =
    {
        { { -1, -1, 0 }, { 0, 1 } },
        { { -1, +1, 0 }, { 0, 0 } },
        { { +1, +1, 0 }, { 1, 0 } },
        { { -1, -1, 0 }, { 0, 1 } },
        { { +1, +1, 0 }, { 1, 0 } },
        { { +1, -1, 0 }, { 1, 1 } }
    };

    VertexBuffer<Vertex> vertexBuffer;
    auto vertexUploader = vertexBuffer.initializeStatic(
        device, uploadCmdList, 6, vertexData);

    // constant buffer

    struct VSTransform
    {
        Mat4 WVP;
    };

    ConstantBuffer<VSTransform> vsTransform;
    vsTransform.initializeDynamic(device, window.getImageCount());

    // texture

    auto texData = agz::texture::texture2d_t<agz::math::color4b>(
        agz::img::load_rgba_from_file("./asset/05_mipmap.png"));
    if(!texData.is_available())
        throw std::runtime_error("failed to load texture from image file");
    const auto mipmapChain = constructMipmapChain(std::move(texData), -1);

    std::vector<Texture2D::SubresourceInitData> texInitData;
    for(auto &m : mipmapChain)
        texInitData.push_back({ m.raw_data() });

    Texture2D tex;
    auto texUploader = tex.initializeShaderResource(
        device, DXGI_FORMAT_R8G8B8A8_UNORM,
        mipmapChain[0].width(), mipmapChain[0].height(),
        1, int(mipmapChain.size()), false, false,
        { uploadCmdList, texInitData.data() });

    auto texSRV = *rscHeap.allocSingle();
    tex.createShaderResourceView(texSRV);

    // msaa render target

    Texture2D msaaRT;
    RawDescriptorHeap msaaRTVHeap(
        device, 1, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, false);

    auto initMSAART = [&]
    {
        msaaRT.initializeRenderTarget(
            device,
            DXGI_FORMAT_R8G8B8A8_UNORM,
            window.getImageWidth(),
            window.getImageHeight(),
            4, 0, { 0, 0, 0, 0 });

        msaaRT.createRenderTargetView(msaaRTVHeap.getCPUHandle(0));
    };

    initMSAART();

    // pipeline

    auto rootSignature = fg::RootSignature
    {
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT,
        fg::ConstantBufferView{ D3D12_SHADER_VISIBILITY_VERTEX, "s0b0" },
        fg::DescriptorTable
        {
            D3D12_SHADER_VISIBILITY_PIXEL,
            fg::ShaderResourceViewRange{ "s0t0" }
        },
        fg::StaticSampler
        {
            D3D12_SHADER_VISIBILITY_PIXEL,
            "s0s0",
            D3D12_FILTER_MIN_MAG_MIP_LINEAR
        }
    }.createRootSignature(device);

    D3D12_INPUT_ELEMENT_DESC inputDescs[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, pos),
          D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(Vertex, uv),
          D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    const auto msaaSampleDesc = msaaRT.getMultisample();

    ShaderCompiler compiler;
    compiler
        .setOptLevel(ShaderCompiler::OptLevel::Debug)
        .setWarnings(true);

    auto pipeline = GraphicsPipelineStateBuilder(device)
        .setRootSignature(rootSignature)
        .setVertexShader(compiler.compileShader(VERTEX_SHADER, "vs_5_0"))
        .setPixelShader(compiler.compileShader(PIXEL_SHADER, "ps_5_0"))
        .setRenderTargetFormats({ windowDesc.backbufferFormat })
        .setMultisample(msaaSampleDesc.first, msaaSampleDesc.second)
        .setInputElements(inputDescs)
        .createPipelineState();

    // data upload

    uploadCmdList->Close();
    window.executeOneCmdList(uploadCmdList);
    window.waitCommandQueueIdle();

    vertexUploader.Reset();
    texUploader.Reset();

    // camera

    WalkingCamera camera;
    window.attach(std::make_shared<WindowPostResizeHandler>([&]
    {
        camera.setWOverH(window.getImageWOverH());
    }));
    camera.setWOverH(window.getImageWOverH());
    camera.setPosition({ 0, 0, -25 });
    camera.setLookAt({ 0, 0, 0 });
    camera.setSpeed(10);

    window.getMouse()->showCursor(false);
    window.getMouse()->setCursorLock(true, 300, 200);
    window.doEvents();

    // mainloop

    while(!window.getCloseFlag())
    {
        window.doEvents();
        window.waitForFocus();

        frameCmdList.startFrame();
        frameCmdList.resetCommandList();

        if(window.getKeyboard()->isPressed(KEY_ESCAPE))
            window.setCloseFlag(true);

        // camera

        WalkingCamera::UpdateParams cameraUpdateParams;

        cameraUpdateParams.rotateLeft =
            -0.003f * window.getMouse()->getRelativePositionX();
        cameraUpdateParams.rotateDown =
            +0.003f * window.getMouse()->getRelativePositionY();

        cameraUpdateParams.forward  = window.getKeyboard()->isPressed(KEY_W);
        cameraUpdateParams.backward = window.getKeyboard()->isPressed(KEY_S);
        cameraUpdateParams.left     = window.getKeyboard()->isPressed(KEY_A);
        cameraUpdateParams.right    = window.getKeyboard()->isPressed(KEY_D);

        cameraUpdateParams.seperateUpDown = true;
        cameraUpdateParams.up   = window.getKeyboard()->isPressed(KEY_SPACE);
        cameraUpdateParams.down = window.getKeyboard()->isPressed(KEY_LSHIFT);

        camera.update(cameraUpdateParams, 0.016f);

        // prepare render target

        msaaRT.transit(
            frameCmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);

        const auto msaaRTHandle = msaaRTVHeap.getCPUHandle(0);
        frameCmdList->OMSetRenderTargets(
            1, &msaaRTHandle, FALSE, nullptr);

        const float CLEAR_COLOR[] = { 0, 0, 0, 0 };
        frameCmdList->ClearRenderTargetView(
            msaaRTHandle, CLEAR_COLOR, 0, nullptr);

        // pipeline & constant buffer

        vsTransform.updateContentData(
            window.getCurrentImageIndex(), { camera.getViewProj() });

        ID3D12DescriptorHeap *rawRSCHeap[] = { rscHeap.getRawHeap() };
        frameCmdList->SetDescriptorHeaps(1, rawRSCHeap);

        frameCmdList->SetGraphicsRootSignature(rootSignature.Get());
        frameCmdList->SetGraphicsRootConstantBufferView(
            0, vsTransform.getGpuVirtualAddress(window.getCurrentImageIndex()));
        frameCmdList->SetGraphicsRootDescriptorTable(1, texSRV);
        frameCmdList->SetPipelineState(pipeline.Get());

        // viewport & scissor

        frameCmdList->RSSetViewports(1, &window.getDefaultViewport());
        frameCmdList->RSSetScissorRects(1, &window.getDefaultScissorRect());

        // input topology & vertex buffer

        frameCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        const auto vertexBufferView = vertexBuffer.getView();
        frameCmdList->IASetVertexBuffers(0, 1, &vertexBufferView);

        // drawcall

        frameCmdList->DrawInstanced(
            UINT(vertexBuffer.getVertexCount()), 1, 0, 0);

        // resolve msaaRT into swap chain image

        const auto barrier1 = CD3DX12_RESOURCE_BARRIER::Transition(
            window.getCurrentImage(),
            D3D12_RESOURCE_STATE_PRESENT,
            D3D12_RESOURCE_STATE_RESOLVE_DEST);
        frameCmdList->ResourceBarrier(1, &barrier1);

        msaaRT.transit(
            frameCmdList, D3D12_RESOURCE_STATE_RESOLVE_SOURCE);

        frameCmdList->ResolveSubresource(
            window.getCurrentImage(), 0, msaaRT.getResource(), 0,
            windowDesc.backbufferFormat);

        const auto barrier2 = CD3DX12_RESOURCE_BARRIER::Transition(
            window.getCurrentImage(),
            D3D12_RESOURCE_STATE_RESOLVE_DEST,
            D3D12_RESOURCE_STATE_PRESENT);
        frameCmdList->ResourceBarrier(1, &barrier2);

        // render

        AGZ_D3D12_CHECK_HR(frameCmdList->Close());
        window.executeOneCmdList(frameCmdList);

        // present

        window.present();
        frameCmdList.endFrame();
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
