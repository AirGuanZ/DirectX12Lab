#include <iostream>

#include "./particles.h"

void run()
{
    enableD3D12DebugLayerInDebugMode();

    WindowDesc windowDesc;
    windowDesc.title = L"09.particles";

    Window window(windowDesc);
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

    fg::FrameGraph graph(
        device, window.getAdaptor(),
        rtvHeap.allocSubHeap(20),
        dsvHeap.allocSubHeap(20),
        gpuHeap.allocSubHeap(80),
        window.getCommandQueue(),
        2, window.getImageCount());

    ParticleSystem particleSys(
        device, uploader, window.getImageCount(), 40000);
    particleSys.setAttractedCount(15000);
    particleSys.setParticleSize(0.003f);

    fg::ResourceIndex renderTargetIdx;

    auto initFramegraph = [&]
    {
        graph.newGraph();

        renderTargetIdx = graph.addExternalResource(
            window.getCurrentImage(),
            D3D12_RESOURCE_STATE_PRESENT,
            D3D12_RESOURCE_STATE_PRESENT);

        particleSys.initPasses(
            window.getImageWidth(),
            window.getImageHeight(),
            graph, renderTargetIdx);

        graph.compile();
    };

    initFramegraph();

    window.attach(std::make_shared<WindowPreResizeHandler>(
        [&] { graph.reset(); }));
    window.attach(std::make_shared<WindowPostResizeHandler>(
        [&] { initFramegraph(); }));

    float rotateRad = 0;

    while(!window.getCloseFlag())
    {
        window.doEvents();
        window.waitForFocus();

        if(window.getKeyboard()->isDown(KEY_ESCAPE))
            window.setCloseFlag(true);

        frameFence.startFrame(window.getCurrentImageIndex());
        graph.startFrame(window.getCurrentImageIndex());

        const Mat4 attractorWorld = Trans4::rotate_y(rotateRad += 0.001f);
        particleSys.setAttractorWorld(attractorWorld);

        const Vec3 EYE = { 2, 0.3f, 2 };

        const Mat4 view = Trans4::look_at(
            EYE,
            { 0, 0, 0 },
            { 0, 1, 0 });

        const Mat4 proj = Trans4::perspective(
            agz::math::deg2rad(60.0f),
            window.getImageWOverH(),
            0.01f, 100.0f);

        particleSys.update(
            window.getCurrentImageIndex(),
            0.0016f, view * proj, EYE);

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
