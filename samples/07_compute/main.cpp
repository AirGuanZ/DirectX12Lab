#include <iostream>

#include <agz/d3d12/lab.h>
#include <agz/d3d12/imgui/imgui.h>
#include <agz/utility/image.h>

using namespace agz::d3d12;

const char *HORI_BLUR_SHADER = R"___(
cbuffer BlurSetting : register(b0)
{
    int Radius;
    float Weight0;
    float Weight1;
    float Weight2;
    float Weight3;
    float Weight4;
    float Weight5;
    float Weight6;
    float Weight7;
    float Weight8;
    float Weight9;
    float Weight10;
};

Texture2D<float4>   InputImage  : register(t0);
RWTexture2D<float4> OutputImage : register(u0);

#define MAX_RADIUS 5
#define GROUP_SIZE 256

groupshared float4 ImageCache[GROUP_SIZE + 2 * MAX_RADIUS];

struct CSInput
{
    int3 threadInGroup : SV_GroupThreadID;
    int3 thread        : SV_DispatchThreadID;
};

[numthreads(GROUP_SIZE, 1, 1)]
void main(CSInput input)
{
    // fill imageLineCache

    if(input.threadInGroup.x < Radius)
    {
        int  texelX  = max(0, input.thread.x - Radius);
        int2 texelXY = int2(texelX, input.thread.y);
        ImageCache[input.threadInGroup.x] = InputImage[texelXY];
    }

    if(input.threadInGroup.x >= GROUP_SIZE - Radius)
    {
        int  texelX  = min(InputImage.Length.x - 1, input.thread.x + Radius);
        int2 texelXY = int2(texelX, input.thread.y);
        ImageCache[input.threadInGroup.x + 2 * Radius] = InputImage[texelXY];
    }

    ImageCache[input.threadInGroup.x + Radius] =
        InputImage[min(input.thread.xy, InputImage.Length.xy - 1)];

    // sync thread group

    GroupMemoryBarrierWithGroupSync();

    // blur
    
    float weights[11] = {
        Weight0, Weight1, Weight2, Weight3,
        Weight4, Weight5, Weight6, Weight7,
        Weight8, Weight9, Weight10
    };

    float4 weightedSum = float4(0, 0, 0, 0);

    for(int offset = -Radius; offset <= Radius; ++offset)
    {
        int weightIdx = offset + Radius;
        int cacheIdx  = input.threadInGroup.x + weightIdx;
        weightedSum += weights[weightIdx] * ImageCache[cacheIdx];
    }

    OutputImage[input.thread.xy] = weightedSum;
}
)___";

const char *VERT_BLUR_SHADER = R"___(
cbuffer BlurSetting : register(b0)
{
    int Radius;
    float Weight0;
    float Weight1;
    float Weight2;
    float Weight3;
    float Weight4;
    float Weight5;
    float Weight6;
    float Weight7;
    float Weight8;
    float Weight9;
    float Weight10;
};

Texture2D<float4>   InputImage  : register(t0);
RWTexture2D<float4> OutputImage : register(u0);

#define MAX_RADIUS 5
#define GROUP_SIZE 256

groupshared float4 ImageCache[GROUP_SIZE + 2 * MAX_RADIUS];

struct CSInput
{
    int3 threadInGroup : SV_GroupThreadID;
    int3 thread        : SV_DispatchThreadID;
};

[numthreads(1, GROUP_SIZE, 1)]
void main(CSInput input)
{
    // fill imageLineCache

    if(input.threadInGroup.y < Radius)
    {
        int  texelY  = max(0, input.thread.y - Radius);
        int2 texelXY = int2(input.thread.x, texelY);
        ImageCache[input.threadInGroup.y] = InputImage[texelXY];
    }

    if(input.threadInGroup.y >= GROUP_SIZE - Radius)
    {
        int  texelY  = min(InputImage.Length.y - 1, input.thread.y + Radius);
        int2 texelXY = int2(input.thread.x, texelY);
        ImageCache[input.threadInGroup.y + 2 * Radius] = InputImage[texelXY];
    }

    ImageCache[input.threadInGroup.y + Radius] =
        InputImage[min(input.thread.xy, InputImage.Length.xy - 1)];

    // sync thread group

    GroupMemoryBarrierWithGroupSync();

    // blur
    
    float weights[11] = {
        Weight0, Weight1, Weight2, Weight3,
        Weight4, Weight5, Weight6, Weight7,
        Weight8, Weight9, Weight10
    };

    float4 weightedSum = float4(0, 0, 0, 0);

    for(int offset = -Radius; offset <= Radius; ++offset)
    {
        int weightIdx = offset + Radius;
        int cacheIdx  = input.threadInGroup.y + weightIdx;
        weightedSum += weights[weightIdx] * ImageCache[cacheIdx];
    }

    OutputImage[input.thread.xy] = weightedSum;
}
)___";

std::vector<float> generateBlurWeights(int radius, float sigma)
{
    const float fac = -0.5f / (sigma * sigma);
    const auto gaussian = [fac](int x) { return std::exp(fac * x * x); };

    std::vector<float> ret(2 * radius + 1);
    for(int r = -radius; r <= radius; ++r)
        ret[r + radius] = gaussian(r);

    const float sum = std::accumulate(
        ret.begin(), ret.end(), 0.0f, std::plus<float>());
    const float invSum = 1 / sum;

    for(auto &v : ret)
        v *= invSum;

    return ret;
}

void run()
{
    enableD3D12DebugLayerInDebugMode();

    WindowDesc windowDesc;
    windowDesc.title = L"07.compute";

    Window window(windowDesc);
    auto device = window.getDevice();

    // compute pipeline

    auto rootSignature = fg::RootSignature
    {
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT,
        fg::ImmediateConstants
        {
            D3D12_SHADER_VISIBILITY_ALL,
            fg::s0b0, 12
        },
        fg::DescriptorTable
        {
            D3D12_SHADER_VISIBILITY_ALL,
            fg::SRVRange{ fg::s0t0 },
            fg::UAVRange{ fg::s0u0 },
        }
    }.createRootSignature(device);

    ShaderCompiler compiler;
    compiler
        .setOptLevel(ShaderCompiler::OptLevel::Debug)
        .setWarnings(true);

    auto horiPipeline = ComputePipelineStateBuilder(device)
        .setRootSignature(rootSignature)
        .setComputeShader(compiler.compileShader(HORI_BLUR_SHADER, "cs_5_0"))
        .createPipelineState();

    auto vertPipeline = ComputePipelineStateBuilder(device)
        .setRootSignature(rootSignature)
        .setComputeShader(compiler.compileShader(VERT_BLUR_SHADER, "cs_5_0"))
        .createPipelineState();

    // constant values

    int radius = 5;
    float sigma = 2;
    std::vector<float> weights = generateBlurWeights(radius, sigma);

    // descriptor heap

    DescriptorHeap rscHeap;
    rscHeap.initialize(
        device, 100, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);

    // cmd list

    PerFrameCommandList graphicsCmdList(window);

    GraphicsCommandList uploadCmdList(device);
    uploadCmdList.resetCommandList();

    // input image

    const auto inputImageData = agz::texture::texture2d_t<agz::math::color4b>(
        agz::img::load_rgba_from_file("./asset/05_mipmap.png"));
    if(!inputImageData.is_available())
        throw std::runtime_error("failed to load input image");

    Texture2D inputImage;
    auto uploadTex = inputImage.initializeShaderResource(
        device, DXGI_FORMAT_R8G8B8A8_UNORM,
        inputImageData.width(), inputImageData.height(),
        uploadCmdList, { inputImageData.raw_data() });

    // rw texture
    // read from image, write to A
    // read from A, write to B
    // draw B onto screen

    Texture2D texA, texB;

    texA.initializeShaderResource(
        device, DXGI_FORMAT_R8G8B8A8_UNORM,
        inputImageData.width(), inputImageData.height(),
        1, 1, true, true, {});

    texB.initializeShaderResource(
        device, DXGI_FORMAT_R8G8B8A8_UNORM,
        inputImageData.width(), inputImageData.height(),
        1, 1, true, true, {});

    // desc table

    auto inputToATable = rscHeap.allocRange(2);
    auto AToBTable     = rscHeap.allocRange(2);
    auto BSRV          = rscHeap.allocSingle();

    inputImage.createShaderResourceView(inputToATable[0]);
    texA.createUnorderedAccessView(inputToATable[1]);

    texA.createShaderResourceView(AToBTable[0]);
    texB.createUnorderedAccessView(AToBTable[1]);

    texB.createShaderResourceView(BSRV);

    // imgui

    auto imguiSRV = rscHeap.allocSingle();
    ImGuiIntegration imgui(
        window, rscHeap.getRawHeap(),
        imguiSRV.getCPUHandle(), imguiSRV.getGPUHandle());

    // upload

    uploadCmdList->Close();
    window.executeOneCmdList(uploadCmdList);
    window.waitCommandQueueIdle();
    uploadTex.Reset();

    while(!window.getCloseFlag())
    {
        window.doEvents();
        window.waitForFocus();

        imgui.newFrame();

        graphicsCmdList.startFrame();
        graphicsCmdList.resetCommandList();

        if(window.getKeyboard()->isPressed(KEY_ESCAPE))
            window.setCloseFlag(true);

        // compute part

        texA.transit(graphicsCmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        
        ID3D12DescriptorHeap *rawRscHeap[] = { rscHeap.getRawHeap() };
        graphicsCmdList->SetDescriptorHeaps(1, rawRscHeap);
        
        graphicsCmdList->SetComputeRootSignature(rootSignature.Get());
        
        graphicsCmdList->SetComputeRoot32BitConstant(0, UINT(radius), 0);
        graphicsCmdList->SetComputeRoot32BitConstants(
            0, UINT(weights.size()), weights.data(), 1);
        
        graphicsCmdList->SetComputeRootDescriptorTable(1, inputToATable[0]);
        
        graphicsCmdList->SetPipelineState(horiPipeline.Get());
        
        graphicsCmdList->Dispatch(
            inputImageData.width(), inputImageData.height(), 1);

        texA.transit(graphicsCmdList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        texB.transit(graphicsCmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        
        graphicsCmdList->SetComputeRootDescriptorTable(1, AToBTable[0]);
        
        graphicsCmdList->SetPipelineState(vertPipeline.Get());
        
        graphicsCmdList->Dispatch(
            inputImageData.width(), inputImageData.height(), 1);

        // imgui

        if(ImGui::Begin("output", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            bool changed = ImGui::SliderInt("Radius", &radius, 0, 5);
            changed |= ImGui::SliderFloat("Sigma", &sigma, 0.01f, 10);
            if(changed)
                weights = generateBlurWeights(radius, sigma);

            ImGui::Image(
                ImTextureID(BSRV.getGPUHandle().ptr),
                { float(inputImageData.width()), float(inputImageData.height()) });
        }
        ImGui::End();

        // graphics part

        texB.transit(graphicsCmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

        graphicsCmdList->ResourceBarrier(
            1, agz::get_temp_ptr(CD3DX12_RESOURCE_BARRIER::Transition(
                window.getCurrentImage(),
                D3D12_RESOURCE_STATE_PRESENT,
                D3D12_RESOURCE_STATE_RENDER_TARGET)));

        auto rtvHandle = window.getCurrentImageDescHandle();
        graphicsCmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

        const float CLEAR_COLOR[] = { 0, 1, 1, 0 };
        graphicsCmdList->ClearRenderTargetView(rtvHandle, CLEAR_COLOR, 0, nullptr);

        imgui.render(graphicsCmdList);

        graphicsCmdList->ResourceBarrier(
            1, agz::get_temp_ptr(CD3DX12_RESOURCE_BARRIER::Transition(
                window.getCurrentImage(),
                D3D12_RESOURCE_STATE_RENDER_TARGET,
                D3D12_RESOURCE_STATE_PRESENT)));

        graphicsCmdList->Close();
        window.executeOneCmdList(graphicsCmdList);

        window.present();
        graphicsCmdList.endFrame();
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
