#include <iostream>

#include "./mesh.h"

const char *GBUFFER_VERTEX_SHADER = R"___(
cbuffer Transform : register(b0)
{
    float4x4 WVP;
    float4x4 World;
};

struct VSInput
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float2 uv       : TEXCOORD;
};

struct VSOutput
{
    float4 position      : SV_POSITION;
    float3 worldPosition : POSITION;
    float3 worldNormal   : NORMAL;
    float2 uv            : TEXCOORD;
};

VSOutput main(VSInput input)
{
    VSOutput output      = (VSOutput)0;
    output.position      = mul(float4(input.position, 1), WVP);

    float4 hWorldPos = mul(float4(input.position, 1), World);
    float4 hWorldNor = mul(float4(input.normal, 0), World);

    output.worldPosition = hWorldPos.xyz / hWorldPos.w;
    output.worldNormal   = normalize(hWorldNor.xyz);
    output.uv            = input.uv;
    return output;
}
)___";

const char *GBUFFER_PIXEL_SHADER = R"___(
Texture2D<float4> Albedo        : register(t0);
SamplerState      LinearSampler : register(s0);

struct PSInput
{
    float4 position      : SV_POSITION;
    float3 worldPosition : POSITION;
    float3 worldNormal   : NORMAL;
    float2 uv            : TEXCOORD;
};

struct PSOutput
{
    float4 position : SV_TARGET0;
    float4 normal   : SV_TARGET1;
    float4 color    : SV_TARGET2;
};

PSOutput main(PSInput input)
{
    PSOutput output = (PSOutput)0;
    output.position = float4(input.worldPosition, 1);
    output.normal   = float4(normalize(input.worldNormal), 0);
    output.color    = Albedo.Sample(LinearSampler, input.uv);
    return output;
}
)___";

const char *LIGHTING_VERTEX_SHADER = R"___(
struct VSOutput
{
    float4 position : SV_POSITION;
    float2 uv       : TEXCOORD;
};

VSOutput main(uint vertexID : SV_VertexID)
{
    VSOutput output = (VSOutput)0;
    output.uv       = float2((vertexID << 1) & 2, vertexID & 2);
    output.position = float4(output.uv * float2(2, -2) + float2(-1, 1), 0, 1);
    return output;
}
)___";

const char *LIGHTING_PIXEL_SHADER = R"___(
cbuffer PointLight : register(b0)
{
    float3 LightPosition;
    float3 LightColor;
    float3 LightAmbient;
};

Texture2D<float4> Position : register(t0);
Texture2D<float4> Normal   : register(t1);
Texture2D<float4> Color    : register(t2);

SamplerState PointSampler : register(s0);

struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv       : TEXCOORD;
};

float4 main(PSInput input) : SV_TARGET
{
    float3 worldPos = Position.Sample(PointSampler, input.uv).xyz;
    float3 worldNor = Normal  .Sample(PointSampler, input.uv).xyz;
    float3 color    = Color   .Sample(PointSampler, input.uv).rgb;

    if(dot(abs(worldNor), 1) < 0.1)
        discard;

    float3 d = LightPosition - worldPos;
    float lightFactor = max(0, dot(normalize(d), worldNor)) / dot(d, d);
    float3 lightColor = lightFactor * LightColor + LightAmbient;

    float3 finalColor = lightColor * pow(color, 2.2);

    return float4(pow(finalColor, 1 / 2.2), 1);
}
)___";

void run()
{
    enableD3D12DebugLayerInDebugMode();

    WindowDesc desc;
    desc.title = L"09.framegraph.deferred";

    Window window(desc);

    auto mouse    = window.getMouse();
    auto keyboard = window.getKeyboard();
    auto device   = window.getDevice();

    // frame resource fence

    FrameResourceFence frameFence(window);

    // desc heaps

    DescriptorHeap gpuHeap;
    gpuHeap.initialize(
        device, 150, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);

    DescriptorHeap rtvHeap;
    rtvHeap.initialize(
        device, 100, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, false);

    DescriptorHeap dsvHeap;
    dsvHeap.initialize(
        device, 100, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, false);

    // meshes

    GraphicsCommandList uploadCmdList(device);

    uploadCmdList.resetCommandList();

    std::vector<Mesh> meshes(2);
    auto meshUploadHeap = meshes[0].loadFromFile(
        window, gpuHeap.getRootSubheap(), uploadCmdList,
        "./asset/03_cube.obj", "./asset/03_texture.png");
    auto meshUploadHeap2 = meshes[1].loadFromFile(
        window, gpuHeap.getRootSubheap(), uploadCmdList,
        "./asset/03_cube.obj", "./asset/03_texture.png");

    uploadCmdList->Close();

    window.executeOneCmdList(uploadCmdList);
    window.waitCommandQueueIdle();

    meshUploadHeap.clear();
    meshUploadHeap2.clear();

    meshes[0].setWorldTransform(Mat4::identity());
    meshes[1].setWorldTransform(
        Trans4::rotate_y(0.6f) *
        Trans4::translate({ 0, 0, -3 }));

    // light data
    struct PointLight
    {
        Vec3 pos;
        float pad0 = 0;
        Vec3 color;
        float pad1 = 0;
        Vec3 ambient;
        float pad2 = 0;
    };

    ConstantBuffer<PointLight> lightingPointLight;
    lightingPointLight.initializeDynamic(
        window.getDevice());
    lightingPointLight.updateContentData(0,
        { { -4, 5, 0 }, 0, { 10, 24, 24 }, 0, { 0.05f, 0.05f, 0.05f }, 0 });

    // root signature

    auto gBufferRootSignature = fg::RootSignature
    {
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT,
        fg::ConstantBufferView{ D3D12_SHADER_VISIBILITY_VERTEX, fg::s0b0 },
        fg::DescriptorTable
        {
            D3D12_SHADER_VISIBILITY_PIXEL,
            fg::s0t0,
        },
        fg::StaticSampler
        {
            D3D12_SHADER_VISIBILITY_PIXEL,
            fg::s0s0,
            D3D12_FILTER_MIN_MAG_MIP_LINEAR
        }
    }.createRootSignature(window.getDevice());

    auto lightingRootSignature = fg::RootSignature
    {
        D3D12_ROOT_SIGNATURE_FLAG_NONE,
        fg::ConstantBufferView{ D3D12_SHADER_VISIBILITY_PIXEL, fg::s0b0 },
        fg::DescriptorTable
        {
            D3D12_SHADER_VISIBILITY_PIXEL,
            fg::s0t0,
            fg::s0t1,
            fg::s0t2
        },
        fg::StaticSampler
        {
            D3D12_SHADER_VISIBILITY_PIXEL,
            fg::s0s0,
            D3D12_FILTER_MIN_MAG_MIP_POINT
        }
    }.createRootSignature(window.getDevice());

    // pipeline state

    D3D12_INPUT_ELEMENT_DESC gBufferInputElems[] =
    {
        {
            "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,
            0, offsetof(Mesh::Vertex, pos),
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
        },
        {
            "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT,
            0, offsetof(Mesh::Vertex, nor),
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
        },
        {
            "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,
            0, offsetof(Mesh::Vertex, uv),
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
        }
    };

    auto gBufferPipeline = fg::GraphicsPipelineState{
        gBufferRootSignature,
        fg::VertexShader{ GBUFFER_VERTEX_SHADER, "vs_5_0" },
        fg::PixelShader{ GBUFFER_PIXEL_SHADER, "ps_5_0" },
        fg::InputLayout(gBufferInputElems),
        fg::PipelineRTVFormats{
            DXGI_FORMAT_R32G32B32A32_FLOAT,
            DXGI_FORMAT_R32G32B32A32_FLOAT,
            DXGI_FORMAT_R8G8B8A8_UNORM },
        fg::DepthStencilState{ DXGI_FORMAT_D24_UNORM_S8_UINT }
    }.createPipelineState(device);

    auto lightingPipeline = fg::GraphicsPipelineState{
        lightingRootSignature,
        fg::VertexShader{ LIGHTING_VERTEX_SHADER, "vs_5_0" },
        fg::PixelShader{ LIGHTING_PIXEL_SHADER, "ps_5_0" },
        fg::PipelineRTVFormats{ window.getImageFormat() }
    }.createPipelineState(device);

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

    mouse->showCursor(false);
    mouse->setCursorLock(true, 300, 200);
    window.doEvents();

    // framegraph

    fg::FrameGraph graph(
        device,
        rtvHeap.allocSubHeap(100),
        dsvHeap.allocSubHeap(100),
        gpuHeap.allocSubHeap(100),
        window.getCommandQueue(),
        2, window.getImageCount());

    fg::ResourceIndex dsIdx, rtIdx, gPosIdx, gNorIdx, gColorIdx;

    auto initFrameGraph = [&]
    {
        const UINT W = static_cast<UINT>(window.getImageWidth());
        const UINT H = static_cast<UINT>(window.getImageHeight());

        using namespace fg;

        graph.newGraph();

        dsIdx = graph.addTransientResource(
            Tex2DDesc{ DXGI_FORMAT_D24_UNORM_S8_UINT, W, H });

        rtIdx = graph.addExternalResource(
            window.getCurrentImage(),
            D3D12_RESOURCE_STATE_PRESENT,
            D3D12_RESOURCE_STATE_PRESENT);

        gPosIdx = graph.addTransientResource(
            Tex2DDesc{ DXGI_FORMAT_R32G32B32A32_FLOAT, W, H });

        gNorIdx = graph.addTransientResource(
            Tex2DDesc{ DXGI_FORMAT_R32G32B32A32_FLOAT, W, H });

        gColorIdx = graph.addTransientResource(
            Tex2DDesc{ DXGI_FORMAT_R8G8B8A8_UNORM, W, H });

        graph.addGraphicsPass(
            [&](ID3D12GraphicsCommandList *cmdList,
                FrameGraphPassContext &ctx)
            {
                cmdList->IASetPrimitiveTopology(
                    D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

                for(auto &m : meshes)
                {
                    m.draw(
                        cmdList, window.getCurrentImageIndex(), camera);
                }
            },
            RenderTargetBinding{ Tex2DRTV{ gPosIdx   }, ClearColor{ } },
            RenderTargetBinding{ Tex2DRTV{ gNorIdx   }, ClearColor{ } },
            RenderTargetBinding{ Tex2DRTV{ gColorIdx }, ClearColor{ } },
            DepthStencilBinding{ Tex2DDSV{ dsIdx     }, ClearDepthStencil{ } },
            gBufferRootSignature,
            gBufferPipeline);

        graph.addGraphicsPass(
            [&](ID3D12GraphicsCommandList *cmdList,
                FrameGraphPassContext &ctx)
        {
            cmdList->SetGraphicsRootConstantBufferView(
                0, lightingPointLight.getGpuVirtualAddress(0));
            cmdList->SetGraphicsRootDescriptorTable(
                1, ctx.getResource(gPosIdx).descriptor);

            cmdList->IASetPrimitiveTopology(
                D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

            cmdList->DrawInstanced(3, 1, 0, 0);
        },
            Tex2DSRV(gPosIdx),
            Tex2DSRV(gNorIdx),
            Tex2DSRV(gColorIdx),
            RenderTargetBinding{
                Tex2DRTV{ rtIdx },
                ClearColor{ }
            },
            lightingPipeline,
            lightingRootSignature);

        graph.compile();
    };

    initFrameGraph();

    window.attach(std::make_shared<WindowPreResizeHandler>(
        [&] { graph.restart(); }));
    window.attach(std::make_shared<WindowPostResizeHandler>(
        [&] { initFrameGraph(); }));

    while(!window.getCloseFlag())
    {
        window.doEvents();
        window.waitForFocus();

        frameFence.startFrame(window.getCurrentImageIndex());
        graph.startFrame(window.getCurrentImageIndex());

        // exit?

        if(keyboard->isDown(KEY_ESCAPE))
            window.setCloseFlag(true);
        
        // camera
        
        WalkingCamera::UpdateParams cameraUpdateParams;

        cameraUpdateParams.rotateLeft =
            -0.003f * mouse->getRelativePositionX();
        cameraUpdateParams.rotateDown =
            +0.003f * mouse->getRelativePositionY();

        cameraUpdateParams.forward  = keyboard->isPressed(KEY_W);
        cameraUpdateParams.backward = keyboard->isPressed(KEY_S);
        cameraUpdateParams.left     = keyboard->isPressed(KEY_A);
        cameraUpdateParams.right    = keyboard->isPressed(KEY_D);

        cameraUpdateParams.seperateUpDown = true;
        cameraUpdateParams.up   = keyboard->isPressed(KEY_SPACE);
        cameraUpdateParams.down = keyboard->isPressed(KEY_LSHIFT);

        camera.update(cameraUpdateParams, 0.016f);

        // framegraph

        graph.setExternalRsc(rtIdx, window.getCurrentImage());
        graph.execute();

        // present

        window.present();

        frameFence.endFrame();
        graph.endFrame();
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
    }
}
