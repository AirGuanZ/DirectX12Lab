#include <iostream>

#include <d3dx12.h>

#include <agz/d3d12/D3D12Lab.h>
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
    float lf  = max(dot(normalize(input.nor), normalize(float3(-1, 1, 1))), 0);
    float lum = 0.6 * saturate(lf * lf + 0.05);
    float4 tex = pow(Tex.Sample(Sam, input.uv), 2.2);
    if(tex.a < 0.5)
        discard;
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
    int imageIndex,
    ConstantBuffer<CBTransform> &cb,
    std::chrono::high_resolution_clock::time_point start,
    const float aspectRatio)
{
    const auto duration = std::chrono::high_resolution_clock::now() - start;
    const float t = std::chrono::duration_cast<
        std::chrono::microseconds>(duration).count() / 1e6f;

    const Mat4 world = Trans4::rotate_y(t);
    const Mat4 view  = Trans4::look_at({ -4, 0, 0 }, { 0, 0, 0 }, { 0, 1, 0 });
    const Mat4 proj  = Trans4::perspective(
        agz::math::deg2rad(60.0f), aspectRatio, 0.1f, 100.0f);

    cb.updateContentData(imageIndex, { world * view * proj, world });
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

    D3D12_DESCRIPTOR_RANGE texRange[1];
    texRange[0].BaseShaderRegister                = 0;
    texRange[0].NumDescriptors                    = 1;
    texRange[0].OffsetInDescriptorsFromTableStart = 0;
    texRange[0].RangeType                         = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    texRange[0].RegisterSpace                     = 0;

    CD3DX12_ROOT_PARAMETER rootParams[2] = {};
    rootParams[0].InitAsConstantBufferView(0);
    rootParams[1].InitAsDescriptorTable(1, texRange);

    CD3DX12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
    staticSamplers[0].Init(0);

    CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Init(
        UINT(agz::array_size(rootParams)), rootParams,
        UINT(agz::array_size(staticSamplers)), staticSamplers,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ComPtr<ID3D10Blob> rootSignatureBlob;
    AGZ_D3D12_CHECK_HR(D3D12SerializeRootSignature(
        &rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1,
        rootSignatureBlob.GetAddressOf(), nullptr));

    ComPtr<ID3D12RootSignature> rootSignature;
    AGZ_D3D12_CHECK_HR(device->CreateRootSignature(
        0,
        rootSignatureBlob->GetBufferPointer(),
        rootSignatureBlob->GetBufferSize(),
        IID_PPV_ARGS(rootSignature.GetAddressOf())));

    // pipeline state

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

    auto pipeline = window.createPipelineBuilder()
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
        const auto depthStencilBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
            depthStencilBuffer.getResource(),
            D3D12_RESOURCE_STATE_COMMON,
            D3D12_RESOURCE_STATE_DEPTH_WRITE);
        uploadCmdList->ResourceBarrier(1, &depthStencilBarrier);
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
    auto tex = loadTexture2DFromMemory(
        device, uploadCmdList.getCmdList(), texData);

    uploadCmdList->Close();
    window.executeOneCmdList(uploadCmdList.getCmdList());
    window.waitCommandQueueIdle();

    tex.uploadResource.Reset();

    // SRV heap

    DescriptorHeap srvHeap(
        device, 1, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
    srvDesc.Format                        = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension                 = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping       = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels           = 1;
    srvDesc.Texture2D.MostDetailedMip     = 0;
    srvDesc.Texture2D.PlaneSlice          = 0;
    srvDesc.Texture2D.ResourceMinLODClamp = 0;

    device->CreateShaderResourceView(
        tex.textureResource.Get(), &srvDesc, srvHeap.getCPUHandle(0));

    // mainloop

    const auto startTime = std::chrono::high_resolution_clock::now();

    while(!window.getCloseFlag())
    {
        window.doEvents();
        window.waitForFocus();

        cmdList.startFrame();
        cmdList.resetCommandList();

        // prepare render target & depth stencil buffer

        const auto barrier1 = CD3DX12_RESOURCE_BARRIER::Transition(
            window.getCurrentImage(cmdList.getFrameIndex()),
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

        // root signature & constant buffer

        updateCBTransform(
            window.getCurrentImageIndex(),
            cbTransform,
            startTime,
            window.getImageWOverH());

        cmdList->SetGraphicsRootSignature(rootSignature.Get());
        cmdList->SetGraphicsRootConstantBufferView(
            0, cbTransform.getGpuVirtualAddress(window.getCurrentImageIndex()));

        ID3D12DescriptorHeap *rawSRVHeap[] = { srvHeap.getHeap() };
        cmdList->SetDescriptorHeaps(1, rawSRVHeap);
        cmdList->SetGraphicsRootDescriptorTable(
            1, srvHeap.getGPUHandle(0));

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
            window.getCurrentImage(cmdList.getFrameIndex()),
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
    catch(const D3D12LabException &e)
    {
        MessageBoxA(nullptr, e.what(), "Error", MB_OK);
    }
    catch(const std::exception &e)
    {
        std::cerr << e.what() << std::endl;
        return -1;
    }
}
