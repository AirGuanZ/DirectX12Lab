#include <iostream>

#include <agz/d3d12/lab.h>

using namespace agz::d3d12;

void run()
{
    enableD3D12DebugLayerInDebugMode();

    WindowDesc windowDesc;
    windowDesc.title = L"08.framegraph";

    Window window(windowDesc);
    auto device = window.getDevice();

    FrameResourceFence frameFence(device, window.getImageCount());

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

    window.attach(std::make_unique<WindowPreResizeHandler>(
        [&] { graph.restart(); }));

    while(!window.getCloseFlag())
    {
        window.doEvents();
        window.waitForFocus();

        if(window.getKeyboard()->isPressed(KEY_ESCAPE))
            window.setCloseFlag(true);

        frameFence.startFrame(window.getCurrentImageIndex());
        graph.startFrame(window.getCurrentImageIndex());

        graph.newGraph();

        const auto rtIdx = graph.addExternalResource(
            window.getCurrentImage(),
            D3D12_RESOURCE_STATE_PRESENT,
            D3D12_RESOURCE_STATE_PRESENT);

        graph.addPass(
            [&](ID3D12GraphicsCommandList *cmdList,
                fg::FrameGraphPassContext &ctx)
        {
            const auto rtRsc = ctx.getResource(rtIdx);

            cmdList->OMSetRenderTargets(
                1, &rtRsc.descriptor.getCPUHandle(), false, nullptr);

            const float CLEAR_COLOR[4] = { 0, 1, 1, 0 };
            cmdList->ClearRenderTargetView(
                rtRsc.descriptor, CLEAR_COLOR, 0, nullptr);
        },
            fg::Tex2DRTV(rtIdx));

        graph.compile();
        graph.execute();

        window.present();
        frameFence.endFrame(window.getCommandQueue());
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
        std::cout << e.what() << std::endl;
        return -1;
    }
}
