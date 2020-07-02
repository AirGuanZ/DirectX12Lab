#include <iostream>

#include <agz/d3d12/imgui/imgui.h>
#include <agz/d3d12/imgui/imfilebrowser.h>

#include "./particles.h"

void run()
{
    enableD3D12DebugLayerInDebugMode();

    WindowDesc windowDesc;
    windowDesc.title      = L"09.particles";
    windowDesc.fullscreen = false;

    Window window(windowDesc, true);
    auto device = window.getDevice();

    FrameResourceFence frameFence(
        device, window.getCommandQueue(), window.getImageCount());

    ResourceUploader uploader(window, 3);

    DescriptorHeap rtvHeap;
    DescriptorHeap dsvHeap;
    DescriptorHeap gpuHeap;

    rtvHeap.initialize(device, 20, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, false);
    dsvHeap.initialize(device, 20, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, false);
    gpuHeap.initialize(device, 100, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);

    // imgui

    auto imguiSRV = gpuHeap.allocSingle();
    ImGuiIntegration imgui(window, gpuHeap.getRawHeap(), imguiSRV, imguiSRV);

    ImGui::StyleColorsDark();

    // particle sys

    constexpr int PARTICLE_CNT = 50000;
    constexpr int MAX_ATTRACTOR_CNT = 40000;

    int attractedCount = PARTICLE_CNT / 2;

    ParticleSystem particleSys(
        device, uploader, window.getImageCount(), PARTICLE_CNT);
    particleSys.setAttractedCount(attractedCount);
    particleSys.setParticleSize(0.003f);

    // meshes

    struct MeshRecord
    {
        ComPtr<ID3D12Resource> attractors;
        int attractorCnt = 1;
    };

    std::vector<MeshRecord> meshes;
    int curMeshIdx = 0;
    bool autoSwitchMesh = true;

    auto loadMesh = [&](const char *filename)
    {
        AttractorMesh mesh;
        mesh.loadFromFile(filename);

        meshes.emplace_back();
        meshes.back().attractors = mesh.generateAttractorData(
            device, uploader, MAX_ATTRACTOR_CNT);
        meshes.back().attractorCnt = 20000;
    };

    const char *meshNameList[] =
    {
        "./asset/09_models/bunny.obj",
        "./asset/09_models/cube.obj",
        "./asset/09_models/face.obj",
        "./asset/09_models/heart.obj",
        "./asset/09_models/plant.obj",
        "./asset/09_models/tea.obj"
    };

    for(auto m : meshNameList)
        loadMesh(m);

    particleSys.setMesh(
        meshes[curMeshIdx].attractors, meshes[curMeshIdx].attractorCnt);

    // framegraph

    fg::FrameGraph graph(
        device, window.getAdaptor(),
        rtvHeap.allocSubHeap(20),
        dsvHeap.allocSubHeap(20),
        gpuHeap.allocSubHeap(80),
        window.getCommandQueue(),
        1, window.getImageCount());

    window.attach(std::make_shared<WindowPreResizeHandler>(
        [&] { graph.reset(); }));

    float camRotRadX = 0;
    float camRotRadY = 0;
    int frameCnter = 0;

    while(!window.getCloseFlag())
    {
        window.doEvents();
        window.waitForFocus();

        if(window.getKeyboard()->isDown(KEY_ESCAPE))
            window.setCloseFlag(true);

        imgui.newFrame();

        if(autoSwitchMesh && ++frameCnter > 300)
        {
            frameCnter = 0;
            curMeshIdx = (curMeshIdx + 1) % static_cast<int>(meshes.size());

            particleSys.setMesh(
                meshes[curMeshIdx].attractors,
                meshes[curMeshIdx].attractorCnt);
        }

        if(ImGui::Begin("09.particles", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            if(ImGui::SliderInt(
                "Attractor Count", &meshes[curMeshIdx].attractorCnt,
                1, MAX_ATTRACTOR_CNT))
            {
                particleSys.setMesh(
                    meshes[curMeshIdx].attractors,
                    meshes[curMeshIdx].attractorCnt);
            }

            if(ImGui::SliderInt(
                "Attracted Count", &attractedCount, 1, PARTICLE_CNT))
            {
                particleSys.setAttractedCount(attractedCount);
            }

            if(ImGui::Combo(
                "Model", &curMeshIdx,
                meshNameList, static_cast<int>(agz::array_size(meshNameList))))
            {
                frameCnter = 0;
                particleSys.setMesh(
                    meshes[curMeshIdx].attractors,
                    meshes[curMeshIdx].attractorCnt);
            }

            ImGui::Checkbox("Auto Switch Mesh", &autoSwitchMesh);
        }
        ImGui::End();

        frameFence.startFrame(window.getCurrentImageIndex());

        fg::ResourceIndex renderTargetIdx;

        graph.newGraph();

        renderTargetIdx = graph.addExternalResource(
            window.getCurrentImage(),
            D3D12_RESOURCE_STATE_PRESENT,
            D3D12_RESOURCE_STATE_PRESENT);

        particleSys.initPasses(
            window.getImageWidth(),
            window.getImageHeight(),
            graph, renderTargetIdx);

        graph.addGraphicsPass(
            [&](ID3D12GraphicsCommandList *cmdList,
                fg::FrameGraphPassContext &)
        {
            imgui.render(cmdList);
        },
            fg::RenderTargetBinding{ fg::Tex2DRTV{ renderTargetIdx } });

        graph.compile();
        graph.startFrame(window.getCurrentImageIndex());

        particleSys.setAttractorWorld(Mat4::identity());

        if(!ImGui::IsAnyWindowFocused() &&
            window.getMouse()->isPressed(MouseButton::Left))
        {
            camRotRadX -= 0.002f * window.getMouse()->getRelativePositionX();
            camRotRadY += 0.002f * window.getMouse()->getRelativePositionY();

            camRotRadY = agz::math::clamp(
                camRotRadY,
                -agz::math::PI_f / 2 + 0.001f,
                agz::math::PI_f / 2 - 0.001f);
        }

        const Vec3 eye = 4.0f * Vec3(
            std::cos(camRotRadY) * std::cos(camRotRadX),
            std::sin(camRotRadY),
            std::cos(camRotRadY) * std::sin(camRotRadX)
        );

        const Mat4 view = Trans4::look_at(
            eye,
            { 0, 0, 0 },
            { 0, 1, 0 });

        const Mat4 proj = Trans4::perspective(
            agz::math::deg2rad(60.0f),
            window.getImageWOverH(),
            0.01f, 100.0f);

        particleSys.update(
            window.getCurrentImageIndex(),
            0.0016f, view * proj, eye);

        graph.setExternalRsc(renderTargetIdx, window.getCurrentImage());
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
        std::cerr << e.what() << std::endl;
        return -1;
    }
}
