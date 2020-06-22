#include <iostream>

#include <agz/d3d12/lab.h>
#include <agz/utility/mesh.h>

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
};

struct VSOutput
{
    float4 pos : SV_POSITION;
    float3 nor : NORMAL;
};

VSOutput main(VSInput input)
{
    VSOutput output = (VSOutput)0;
    output.pos = mul(float4(input.pos, 1), WVP);
    output.nor = normalize(mul(float4(input.nor, 0), World).xyz);
    return output;
}
)___";

const char *PIXEL_SHADER_SOURCE = R"___(
struct PSInput
{
    float4 pos : SV_POSITION;
    float3 nor : NORMAL;
};

float4 main(PSInput input) : SV_TARGET
{
    float lf  = max(dot(normalize(input.nor), normalize(float3(-1, 1, 1))), 0);
    float lum = 0.6 * saturate(lf * lf + 0.05);
    float color = pow(lum, 1 / 2.2);
    return float4(color, color, color, 1);
}
)___";

struct Vertex
{
    Vec3 pos;
    Vec3 nor;
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
        return Vertex{ scale * (v.position + offset), v.normal };
    });

    return ret;
}

struct CBTransform
{
    Mat4 WVP;
    Mat4 world;
};

void updateCBTransform(
    int imageIndex,
    ConstantBuffer<CBTransform> &cb,
    std::chrono::high_resolution_clock::time_point start,
    const float aspectRatio)
{
    const auto duration = std::chrono::high_resolution_clock::now() - start;
    const float t = std::chrono::duration_cast<
        std::chrono::microseconds>(duration).count() / 1e6f;

    const Mat4 world = Trans4::rotate_y(t);
    const Mat4 view = Trans4::look_at({ -4, 0, 0 }, { 0, 0, 0 }, { 0, 1, 0 });
    const Mat4 proj = Trans4::perspective(
        agz::math::deg2rad(60.0f), aspectRatio, 0.1f, 100.0f);

    cb.updateContentData(imageIndex, { world * view * proj, world });
}

void run()
{
    enableD3D12DebugLayerInDebugMode();

    WindowDesc windowDesc;
    windowDesc.title = L"08.framegraph";

    Window window(windowDesc);
    auto device = window.getDevice();

    // frame fence

    FrameResourceFence frameFence(
        device, window.getCommandQueue(), window.getImageCount());

    // root signature

    auto rootSignature = fg::RootSignature{
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT,
        fg::ConstantBufferView{ D3D12_SHADER_VISIBILITY_VERTEX, { 0 } }
    }.createRootSignature(device);

    // pipeline state

    D3D12_INPUT_ELEMENT_DESC inputDescs[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, pos),
          D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, nor),
          D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
    };

    ShaderCompiler compiler;
    compiler
        .setOptLevel(ShaderCompiler::OptLevel::Debug)
        .setWarnings(true);

    auto pipeline = GraphicsPipelineStateBuilder(device)
        .setRootSignature(rootSignature)
        .setVertexShader(compiler.compileShader(VERTEX_SHADER_SOURCE, "vs_5_0"))
        .setPixelShader(compiler.compileShader(PIXEL_SHADER_SOURCE, "ps_5_0"))
        .setRenderTargetFormats({ windowDesc.backbufferFormat })
        .setInputElements(inputDescs)
        .setDepthStencilBufferFormat(DXGI_FORMAT_D24_UNORM_S8_UINT)
        .setDepthStencilTestState(CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT))
        .createPipelineState();

    // vertex buffer

    auto vertexData = loadMesh("./asset/02_mesh.obj");

    GraphicsCommandList uploadCmdList(device);

    uploadCmdList.resetCommandList();
    VertexBuffer<Vertex> vertexBuffer;
    auto uploadBuf = vertexBuffer.initializeStatic(
        device,
        uploadCmdList,
        vertexData.size(),
        vertexData.data());
    vertexData.clear();
    uploadCmdList->Close();

    window.executeOneCmdList(uploadCmdList);

    window.waitCommandQueueIdle();
    uploadBuf.Reset();

    // constant buffer

    ConstantBuffer<CBTransform> cbTransform;
    cbTransform.initializeDynamic(
        device, window.getImageCount());

    // framegraph

    DescriptorHeap rtvHeap;
    rtvHeap.initialize(
        device, 100, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, false);

    DescriptorHeap dsvHeap;
    dsvHeap.initialize(
        device, 100, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, false);

    DescriptorHeap gpuHeap;
    gpuHeap.initialize(
        device, 100, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);

    auto rtvGraphHeap = *rtvHeap.allocSubHeap(100);
    auto dsvGraphHeap = *dsvHeap.allocSubHeap(100);
    auto gpuGraphHeap = *gpuHeap.allocSubHeap(100);

    fg::FrameGraph graph(
        device, &rtvGraphHeap, &dsvGraphHeap, &gpuGraphHeap,
        window.getCommandQueue(), 3, window.getImageCount());

    fg::ResourceIndex rtIdx;
    fg::ResourceIndex dsIdx;

    // mainloop

    auto initGraph = [&]
    {
        graph.newGraph();

        rtIdx = graph.addExternalResource(
            window.getCurrentImage(),
            D3D12_RESOURCE_STATE_PRESENT,
            D3D12_RESOURCE_STATE_PRESENT);

        dsIdx = graph.addTransientResource(
            fg::Tex2DDesc{
                DXGI_FORMAT_D24_UNORM_S8_UINT,
                static_cast<UINT>(window.getImageWidth()),
                static_cast<UINT>(window.getImageHeight()),
                D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
            },
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            fg::ClearDepthStencil{ 1, 0 });

        graph.addPass(
            [&](ID3D12GraphicsCommandList *cmdList,
                fg::FrameGraphPassContext &ctx)
            {
                cmdList->SetGraphicsRootSignature(rootSignature.Get());
                cmdList->SetGraphicsRootConstantBufferView(
                    0, cbTransform.getGpuVirtualAddress(
                            window.getCurrentImageIndex()));
                cmdList->SetPipelineState(pipeline.Get());

                // vertex data

                cmdList->IASetPrimitiveTopology(
                    D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

                const auto vertexBufferView = vertexBuffer.getView();
                cmdList->IASetVertexBuffers(0, 1, &vertexBufferView);

                // drawcall

                cmdList->DrawInstanced(
                    UINT(vertexBuffer.getVertexCount()), 1, 0, 0);
            },
            fg::RenderTargetBinding
            {
                fg::Tex2DRTV(rtIdx),
                fg::ClearColor{ 0, 1, 1, 0 }
            },
            fg::DepthStencilBinding
            {
                fg::Tex2DDSV(dsIdx),
                fg::ClearDepthStencil{ 1, 0 }
            });

        graph.compile();
    };

    initGraph();

    window.attach(std::make_shared<WindowPreResizeHandler>(
        [&] { graph.restart(); }));
    window.attach(std::make_shared<WindowPostResizeHandler>(
        [&] { initGraph(); }));

    const auto startTime = std::chrono::high_resolution_clock::now();

    while(!window.getCloseFlag())
    {
        window.doEvents();
        window.waitForFocus();

        if(window.getKeyboard()->isPressed(KEY_ESCAPE))
            window.setCloseFlag(true);

        // call 'startFrame' after window event handling
        // since the backbuffer index may change in resize event
        frameFence.startFrame(window.getCurrentImageIndex());
        graph.startFrame(window.getCurrentImageIndex());

        // update mesh transform
        updateCBTransform(
            window.getCurrentImageIndex(),
            cbTransform,
            startTime,
            window.getImageWOverH());

        // render target is different in each frame
        graph.setExternalRsc(rtIdx, window.getCurrentImage());

        // execute the framegraph
        graph.execute();

        window.present();

        graph.endFrame();
        frameFence.endFrame();
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
        std::cout << e.what() << std::endl;
        return -1;
    }
}
