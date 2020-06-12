#include <iostream>

#include <d3dx12.h>

#include <agz/d3d12/lab.h>
#include <agz/utility/mesh.h>
#include <agz/utility/image.h>

using namespace agz::d3d12;

const char *VERTEX_SHADER_SOURCE = R"___(
cbuffer Transform : register(b0)
{
    float4x4 WVP;
    float4x4 World;
};

struct VSInput
{
    float3 pos : POSITION;
    float3 nor : NORMAL;
    float2 uv  : TEXCOORD;
};

struct VSOutput
{
    float4 pos : SV_POSITION;
    float3 nor : NORMAL;
    float2 uv  : TEXCOORD;
};

VSOutput main(VSInput input)
{
    VSOutput output = (VSOutput)0;
    output.pos = mul(float4(input.pos, 1), WVP);
    output.nor = normalize(mul(float4(input.nor, 0), World).xyz);
    output.uv  = input.uv;
    return output;
}
)___";

const char *PIXEL_SHADER_SOURCE = R"___(
struct PSInput
{
    float4 pos : SV_POSITION;
    float3 nor : NORMAL;
    float2 uv  : TEXCOORD;
};

Texture2D    Tex : register(t0);
SamplerState Sam : register(s0);

float4 main(PSInput input) : SV_TARGET
{
    float lf  = max(dot(normalize(input.nor), normalize(float3(-2, 1.5, 1))), 0);
    float lum = 0.6 * saturate(lf * lf + 0.05);
    float4 tex = pow(Tex.Sample(Sam, input.uv), 2.2);
    float3 color = pow(lum * tex.rgb, 1 / 2.2);
    return float4(color, 1);
}
)___";

struct Vertex
{
    Vec3 pos;
    Vec3 nor;
    Vec2 uv;
};

std::vector<Vertex> loadMesh(const std::string &filename)
{
    const auto mesh = triangle_to_vertex(agz::mesh::load_from_file(filename));

    const auto bbox = compute_bounding_box(mesh.data(), mesh.size());
    const Vec3 offset = -0.5f * (bbox.low + bbox.high);
    const float scale = 2 / (bbox.high - bbox.low).max_elem();

    std::vector<Vertex> ret;
    std::transform(mesh.begin(), mesh.end(), std::back_inserter(ret),
        [scale, offset](const agz::mesh::vertex_t &v)
    {
        return Vertex{ scale * (v.position + offset), v.normal, v.tex_coord };
    });

    return ret;
}

struct CBTransform
{
    Mat4 WVP;
    Mat4 world;
};

void updateCBTransform(
    const WalkingCamera &camera,
    int imageIndex, ConstantBuffer<CBTransform> &cb)
{
    cb.updateContentData(imageIndex, { camera.getViewProj(), Mat4::identity() });
}

void run()
{
    enableD3D12DebugLayerInDebugMode();

    WindowDesc desc;
    desc.title = L"03.texture";

    Window window(desc);

    window.getKeyboard()->attach(
        std::make_shared<KeyDownHandler>(
            [&](const auto &e)
    {
        if(e.key == KEY_ESCAPE)
            window.setCloseFlag(true);
    }));

    auto device = window.getDevice();

    // cmd list

    PerFrameCommandList cmdList(window);
    GraphicsCommandList uploadCmdList(window.getDevice());

    // root signature

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
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, nor),
          D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(Vertex, uv),
          D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    ShaderCompiler compiler;
    compiler
        .setOptLevel(ShaderCompiler::OptLevel::Debug)
        .setWarnings(true);

    auto pipeline = GraphicsPipelineStateBuilder(device)
        .setRootSignature(rootSignature)
        .setVertexShader(compiler.compileShader(VERTEX_SHADER_SOURCE, "vs_5_0"))
        .setPixelShader(compiler.compileShader(PIXEL_SHADER_SOURCE, "ps_5_0"))
        .setRenderTargetFormats({ desc.backbufferFormat })
        .setInputElements(inputDescs)
        .setDepthStencilBufferFormat(DXGI_FORMAT_D24_UNORM_S8_UINT)
        .setDepthStencilTestState(CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT))
        .createPipelineState();

    // vertex buffer

    auto vertexData = loadMesh("./asset/03_cube.obj");

    uploadCmdList.resetCommandList();
    VertexBuffer<Vertex> vertexBuffer;
    auto uploadBuf = vertexBuffer.initializeStatic(
        device,
        uploadCmdList.getCmdList(),
        vertexData.size(),
        vertexData.data());
    vertexData.clear();
    uploadCmdList->Close();

    window.executeOneCmdList(uploadCmdList.getCmdList());

    window.waitCommandQueueIdle();
    uploadBuf.Reset();

    // constant buffer

    ConstantBuffer<CBTransform> cbTransform;
    cbTransform.initializeDynamic(
        device, window.getImageCount());

    // depth stencil buffer

    DepthStencilBuffer depthStencilBuffer;

    auto prepareDepthStencilBuffer = [&]
    {
        depthStencilBuffer.initialize(
            device, window.getImageWidth(), window.getImageHeight(),
            DXGI_FORMAT_D24_UNORM_S8_UINT);

        uploadCmdList.resetCommandList();
        uploadCmdList->Close();

        window.executeOneCmdList(uploadCmdList.getCmdList());

        window.waitCommandQueueIdle();
    };

    prepareDepthStencilBuffer();

    window.attach(std::make_shared<WindowPostResizeHandler>([&]
    {
        prepareDepthStencilBuffer();
    }));

    // texture

    auto texData = agz::texture::texture2d_t<agz::math::color4b>(
        agz::img::load_rgba_from_file("./asset/03_texture.png"));
    if(!texData.is_available())
        throw std::runtime_error("failed to load texture data from file");

    uploadCmdList.resetCommandList();
    
    Texture2D tex;
    auto uploadTex = tex.initializeShaderResource(
        device, DXGI_FORMAT_R8G8B8A8_UNORM,
        texData.width(), texData.height(),
        uploadCmdList, { texData.raw_data() });

    uploadCmdList->Close();
    window.executeOneCmdList(uploadCmdList.getCmdList());
    window.waitCommandQueueIdle();

    // SRV heap

    DescriptorHeap descHeap;
    descHeap.initialize(device, 1, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);

    auto srv = *descHeap.allocSingle();
    tex.createShaderResourceView(srv);

    // camera

    WalkingCamera camera;
    window.attach(std::make_shared<WindowPostResizeHandler>([&]
    {
        camera.setWOverH(window.getImageWOverH());
    }));
    camera.setWOverH(window.getImageWOverH());
    camera.setPosition({ -4, 0, 0 });
    camera.setLookAt({ 0, 0, 0 });
    camera.setSpeed(2.5f);

    window.getMouse()->showCursor(false);
    window.getMouse()->setCursorLock(true, 300, 200);
    window.doEvents();

    // mainloop

    while(!window.getCloseFlag())
    {
        window.doEvents();
        window.waitForFocus();

        cmdList.startFrame();
        cmdList.resetCommandList();

        // prepare render target & depth stencil buffer

        const auto barrier1 = CD3DX12_RESOURCE_BARRIER::Transition(
            window.getCurrentImage(),
            D3D12_RESOURCE_STATE_PRESENT,
            D3D12_RESOURCE_STATE_RENDER_TARGET);
        cmdList->ResourceBarrier(1, &barrier1);

        const auto rtvHandle = window.getCurrentImageDescHandle();
        const auto dsvHandle = depthStencilBuffer.getDSVHandle();
        cmdList->OMSetRenderTargets(
            1, &rtvHandle, FALSE, &dsvHandle);

        const float CLEAR_COLOR[] = { 0, 0, 0, 0 };
        cmdList->ClearRenderTargetView(rtvHandle, CLEAR_COLOR, 0, nullptr);
        cmdList->ClearDepthStencilView(
            dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1, 0, 0, nullptr);

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

        // root signature & constant buffer

        updateCBTransform(
            camera,
            window.getCurrentImageIndex(),
            cbTransform);

        cmdList->SetGraphicsRootSignature(rootSignature.Get());
        cmdList->SetGraphicsRootConstantBufferView(
            0, cbTransform.getGpuVirtualAddress(window.getCurrentImageIndex()));

        ID3D12DescriptorHeap *rawSRVHeap[] = { descHeap.getRawHeap() };
        cmdList->SetDescriptorHeaps(1, rawSRVHeap);
        cmdList->SetGraphicsRootDescriptorTable(
            1, srv.getGPUHandle());

        cmdList->SetPipelineState(pipeline.Get());

        // viewport & scissor

        cmdList->RSSetViewports(1, &window.getDefaultViewport());
        cmdList->RSSetScissorRects(1, &window.getDefaultScissorRect());

        // input topology & vertex buffer

        cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        const auto vertexBufferView = vertexBuffer.getView();
        cmdList->IASetVertexBuffers(0, 1, &vertexBufferView);

        // drawcall

        cmdList->DrawInstanced(UINT(vertexBuffer.getVertexCount()), 1, 0, 0);

        // render target state transition

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
