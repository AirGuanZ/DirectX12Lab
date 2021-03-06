#include <agz/utility/image.h>
#include <agz/utility/mesh.h>

#include "./deferred.h"

namespace
{
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
}

void Mesh::loadFromFile(
    const Window      &window,
    ResourceUploader  &uploader,
    DescriptorSubHeap &descHeap,
    const std::string &objFilename,
    const std::string &albedoFilename)
{
    albedoDescTable_ = descHeap.allocRange(1);

    std::vector<ComPtr<ID3D12Resource>> ret;

    // albedo texture

    const auto imgData = agz::texture::texture2d_t<agz::math::color4b>(
        agz::img::load_rgba_from_file(albedoFilename));
    if(!imgData.is_available())
    {
        throw std::runtime_error(
            "failed to load image data from " + albedoFilename);
    }

    AGZ_D3D12_CHECK_HR(
        window.getDevice()->CreateCommittedResource(
            agz::get_temp_ptr(CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT)),
            D3D12_HEAP_FLAG_NONE,
            agz::get_temp_ptr(CD3DX12_RESOURCE_DESC::Tex2D(
                DXGI_FORMAT_R8G8B8A8_UNORM,
                imgData.width(), imgData.height(), 1, 1)),
            D3D12_RESOURCE_STATE_COMMON, nullptr,
            IID_PPV_ARGS(albedo_.GetAddressOf())));

    uploader.uploadTex2DData(
        albedo_, ResourceUploader::Tex2DSubInitData{ imgData.raw_data() },
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
    srvDesc.Format                        = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.Shader4ComponentMapping       = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension                 = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels           = 1;
    srvDesc.Texture2D.MostDetailedMip     = 0;
    srvDesc.Texture2D.PlaneSlice          = 0;
    srvDesc.Texture2D.ResourceMinLODClamp = 0;

    window.getDevice()->CreateShaderResourceView(
        albedo_.Get(), &srvDesc, albedoDescTable_[0]);

    // constant buffer
    
    vsTransform_.initializeDynamic(window.getDevice(), window.getImageCount());

    // vertex data

    const auto mesh =
        triangle_to_vertex(agz::mesh::load_from_file(objFilename));

    std::vector<Vertex> vertexData;
    std::transform(
        mesh.begin(), mesh.end(), std::back_inserter(vertexData),
        [](const agz::mesh::vertex_t &v)
    {
        return Vertex{ v.position, v.normal, v.tex_coord };
    });

    vertexBuffer_.initializeDefault(
        window.getDevice(), vertexData.size(), {});

    uploader.uploadBufferData(
        vertexBuffer_, vertexData.data(),
        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
}

void Mesh::setWorldTransform(const Mat4 &world) noexcept
{
    world_ = world;
}

void Mesh::draw(
    ID3D12GraphicsCommandList *cmdList,
    int                        imageIndex,
    const WalkingCamera       &camera) const
{
    const Mat4 wvp = world_ * camera.getViewProj();
    vsTransform_.updateContentData(imageIndex, { wvp, world_ });

    cmdList->SetGraphicsRootConstantBufferView(
        0, vsTransform_.getGpuVirtualAddress(imageIndex));

    cmdList->SetGraphicsRootDescriptorTable(
        1, albedoDescTable_[0].getGPUHandle());

    const auto vertexBufferView = vertexBuffer_.getView();
    cmdList->IASetVertexBuffers(0, 1, &vertexBufferView);

    cmdList->DrawInstanced(UINT(vertexBuffer_.getVertexCount()), 1, 0, 0);
}

DeferredRenderer::DeferredRenderer(
    Window &window, DescriptorHeap &shaderRscHeap)
    : window_(window), shaderRscHeap_(shaderRscHeap)
{
    // g-buffer render targets

    gBufferRTVHeap_ = RawDescriptorHeap(
        window.getDevice(), 3,
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV, false);

    gBufferSRVDescTable_ = shaderRscHeap_.allocRange(3);

    createGBuffers();

    // g-buffer pipeline

    gBufferRootSignature_ = fg::RootSignature
    {
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT,
        fg::ConstantBufferView{ D3D12_SHADER_VISIBILITY_VERTEX, fg::s0b0 },
        fg::DescriptorTable
        {
            D3D12_SHADER_VISIBILITY_PIXEL,
            fg::SRVRange{ fg::s0t0 }
        },
        fg::StaticSampler
        {
            D3D12_SHADER_VISIBILITY_PIXEL,
            fg::s0s0,
            D3D12_FILTER_MIN_MAG_MIP_LINEAR
        }
    }.createRootSignature(window_.getDevice());

    ShaderCompiler compiler;
    compiler
        .setOptLevel(ShaderCompiler::OptLevel::Debug)
        .setWarnings(true);

    D3D12_INPUT_ELEMENT_DESC inputElements[] =
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

    gBufferPipeline_ = GraphicsPipelineStateBuilder(window_.getDevice())
        .setRootSignature(gBufferRootSignature_.Get())
        .setVertexShader(compiler.compileShader(GBUFFER_VERTEX_SHADER, "vs_5_0"))
        .setPixelShader(compiler.compileShader(GBUFFER_PIXEL_SHADER, "ps_5_0"))
        .setRenderTargetFormats({
            DXGI_FORMAT_R32G32B32A32_FLOAT,
            DXGI_FORMAT_R32G32B32A32_FLOAT,
            DXGI_FORMAT_R8G8B8A8_UNORM })
        .setInputElements(inputElements)
        .setDepthStencilBufferFormat(DXGI_FORMAT_D24_UNORM_S8_UINT)
        .setDepthStencilTestState(CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT))
        .createPipelineState();

    // lighting pipeline

    lightingRootSignature_ = fg::RootSignature
    {
        D3D12_ROOT_SIGNATURE_FLAG_NONE,
        fg::ConstantBufferView{ D3D12_SHADER_VISIBILITY_PIXEL, fg::s0b0 },
        fg::DescriptorTable
        {
            D3D12_SHADER_VISIBILITY_PIXEL,
            fg::SRVRange{ fg::s0t0, fg::RangeSize{ 3 } }
        },
        fg::StaticSampler
        {
            D3D12_SHADER_VISIBILITY_PIXEL,
            fg::s0s0,
            D3D12_FILTER_MIN_MAG_MIP_POINT
        }
    }.createRootSignature(window.getDevice());

    lightingPipeline_ = GraphicsPipelineStateBuilder(window_.getDevice())
        .setRootSignature(lightingRootSignature_.Get())
        .setVertexShader(compiler.compileShader(LIGHTING_VERTEX_SHADER, "vs_5_0"))
        .setPixelShader(compiler.compileShader(LIGHTING_PIXEL_SHADER, "ps_5_0"))
        .setRenderTargetFormats({ window.getImageFormat() })
        .createPipelineState();

    // lighting constant buffer

    lightingPointLight_.initializeDynamic(
        window.getDevice(), window.getImageCount());

    // resize event

    postResizeHandler_.set_function([&] { onResize(); });
    window.attach(&postResizeHandler_);
}

DeferredRenderer::~DeferredRenderer()
{
    window_.detach(&postResizeHandler_);
}

void DeferredRenderer::setLight(
    const Vec3 &pos, const Vec3 &intensity, const Vec3 &ambient) noexcept
{
    lightData_.pos     = pos;
    lightData_.color   = intensity;
    lightData_.ambient = ambient;
}

void DeferredRenderer::startGBuffer(ID3D12GraphicsCommandList *cmdList)
{
    cmdList->ResourceBarrier(
        1, agz::get_temp_ptr(CD3DX12_RESOURCE_BARRIER::Transition(
            gBufferPosition_.Get(),
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_RENDER_TARGET)));

    cmdList->ResourceBarrier(
        1, agz::get_temp_ptr(CD3DX12_RESOURCE_BARRIER::Transition(
            gBufferNormal_.Get(),
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_RENDER_TARGET)));

    cmdList->ResourceBarrier(
        1, agz::get_temp_ptr(CD3DX12_RESOURCE_BARRIER::Transition(
            gBufferColor_.Get(),
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_RENDER_TARGET)));

    auto rtvHandle = gBufferRTVHeap_.getCPUHandle(0);
    auto dsvHandle = depthStencilBuffer_.getDSVHandle();
    cmdList->OMSetRenderTargets(3, &rtvHandle, true, &dsvHandle);
    cmdList->SetPipelineState(gBufferPipeline_.Get());
    cmdList->RSSetViewports(1, &window_.getDefaultViewport());
    cmdList->RSSetScissorRects(1, &window_.getDefaultScissorRect());

    cmdList->SetGraphicsRootSignature(gBufferRootSignature_.Get());

    float CLEAR_VALUES[] = { 0, 0, 0, 0 };
    cmdList->ClearRenderTargetView(
        gBufferRTVHeap_.getCPUHandle(0), CLEAR_VALUES, 0, nullptr);
    cmdList->ClearRenderTargetView(
        gBufferRTVHeap_.getCPUHandle(1), CLEAR_VALUES, 0, nullptr);
    cmdList->ClearRenderTargetView(
        gBufferRTVHeap_.getCPUHandle(2), CLEAR_VALUES, 0, nullptr);
    cmdList->ClearDepthStencilView(
        dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1, 0, 0, nullptr);

    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void DeferredRenderer::endGBuffer(ID3D12GraphicsCommandList *cmdList)
{
    cmdList->ResourceBarrier(
        1, agz::get_temp_ptr(CD3DX12_RESOURCE_BARRIER::Transition(
            gBufferPosition_.Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)));

    cmdList->ResourceBarrier(
        1, agz::get_temp_ptr(CD3DX12_RESOURCE_BARRIER::Transition(
            gBufferNormal_.Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)));

    cmdList->ResourceBarrier(
        1, agz::get_temp_ptr(CD3DX12_RESOURCE_BARRIER::Transition(
            gBufferColor_.Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)));
}

void DeferredRenderer::render(ID3D12GraphicsCommandList *cmdList)
{
    const int imageIndex = window_.getCurrentImageIndex();

    cmdList->SetPipelineState(lightingPipeline_.Get());
    cmdList->RSSetViewports(1, &window_.getDefaultViewport());
    cmdList->RSSetScissorRects(1, &window_.getDefaultScissorRect());

    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    lightingPointLight_.updateContentData(imageIndex, lightData_);

    cmdList->SetGraphicsRootSignature(lightingRootSignature_.Get());

    cmdList->SetGraphicsRootConstantBufferView(
        0, lightingPointLight_.getGpuVirtualAddress(imageIndex));

    cmdList->SetGraphicsRootDescriptorTable(
        1, gBufferSRVDescTable_[0].getGPUHandle());

    cmdList->DrawInstanced(3, 1, 0, 0);
}

void DeferredRenderer::createGBuffers()
{
    auto device = window_.getDevice();
    const int width  = window_.getImageWidth();
    const int height = window_.getImageHeight();

    AGZ_D3D12_CHECK_HR(
        device->CreateCommittedResource(
            agz::get_temp_ptr(CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT)),
            D3D12_HEAP_FLAG_NONE,
            agz::get_temp_ptr(CD3DX12_RESOURCE_DESC::Tex2D(
                DXGI_FORMAT_R32G32B32A32_FLOAT,
                width, height, 1, 1, 1, 0,
                D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)),
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            agz::get_temp_ptr(
                CreateClearColorValue(
                    DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, 0, 0)),
            IID_PPV_ARGS(gBufferPosition_.GetAddressOf())));

    AGZ_D3D12_CHECK_HR(
        device->CreateCommittedResource(
            agz::get_temp_ptr(CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT)),
            D3D12_HEAP_FLAG_NONE,
            agz::get_temp_ptr(CD3DX12_RESOURCE_DESC::Tex2D(
                DXGI_FORMAT_R32G32B32A32_FLOAT,
                width, height, 1, 1, 1, 0,
                D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)),
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            agz::get_temp_ptr(
                CreateClearColorValue(
                    DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, 0, 0)),
            IID_PPV_ARGS(gBufferNormal_.GetAddressOf())));

    AGZ_D3D12_CHECK_HR(
        device->CreateCommittedResource(
            agz::get_temp_ptr(CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT)),
            D3D12_HEAP_FLAG_NONE,
            agz::get_temp_ptr(CD3DX12_RESOURCE_DESC::Tex2D(
                DXGI_FORMAT_R8G8B8A8_UNORM,
                width, height, 1, 1, 1, 0,
                D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)),
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            agz::get_temp_ptr(
                CreateClearColorValue(
                    DXGI_FORMAT_R8G8B8A8_UNORM, 0, 0, 0, 0)),
            IID_PPV_ARGS(gBufferColor_.GetAddressOf())));

    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc;
    rtvDesc.Format               = DXGI_FORMAT_UNKNOWN;
    rtvDesc.ViewDimension        = D3D12_RTV_DIMENSION_TEXTURE2D;
    rtvDesc.Texture2D.MipSlice   = 0;
    rtvDesc.Texture2D.PlaneSlice = 0;

    device->CreateRenderTargetView(
        gBufferPosition_.Get(), &rtvDesc, gBufferRTVHeap_.getCPUHandle(0));
    device->CreateRenderTargetView(
        gBufferNormal_.Get(), &rtvDesc, gBufferRTVHeap_.getCPUHandle(1));
    device->CreateRenderTargetView(
        gBufferColor_.Get(), &rtvDesc, gBufferRTVHeap_.getCPUHandle(2));

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
    srvDesc.Format                        = DXGI_FORMAT_R32G32B32A32_FLOAT;
    srvDesc.Shader4ComponentMapping       = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension                 = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels           = 1;
    srvDesc.Texture2D.MostDetailedMip     = 0;
    srvDesc.Texture2D.PlaneSlice          = 0;
    srvDesc.Texture2D.ResourceMinLODClamp = 0;

    device->CreateShaderResourceView(
        gBufferPosition_.Get(), &srvDesc, gBufferSRVDescTable_[0]);
    device->CreateShaderResourceView(
        gBufferNormal_.Get(), &srvDesc, gBufferSRVDescTable_[1]);

    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    device->CreateShaderResourceView(
        gBufferColor_.Get(), &srvDesc, gBufferSRVDescTable_[2]);

    depthStencilBuffer_.initialize(
        device, width, height, DXGI_FORMAT_D24_UNORM_S8_UINT);
}

void DeferredRenderer::onResize()
{
    createGBuffers();
}
