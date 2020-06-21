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
    fg::FrameGraphExecuter executer(device, 1, window.getImageCount());

    fg::ResourceAllocator rscAlloc(device);

    std::vector<DescriptorHeap> rtvHeap(3);

    while(!window.getCloseFlag())
    {
        window.doEvents();
        window.waitForFocus();

        if(window.getKeyboard()->isPressed(KEY_ESCAPE))
            window.setCloseFlag(true);

        frameFence.startFrame(window.getCurrentImageIndex());
        executer.startFrame(window.getCurrentImageIndex());

        fg::FrameGraphCompiler compiler;

        const auto rtIdx = compiler.addExternalResource(
            window.getCurrentImage(),
            D3D12_RESOURCE_STATE_PRESENT,
            D3D12_RESOURCE_STATE_PRESENT);

        compiler.addPass(
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

        auto graph = compiler.compile(device, rscAlloc);

        rtvHeap[window.getCurrentImageIndex()].initialize(
            device, graph.rtvDescCount, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, false);
        auto rtvRange = *rtvHeap[window.getCurrentImageIndex()]
                            .allocRange(graph.rtvDescCount);

        executer.execute(
            graph, {}, rtvRange, {}, window.getCommandQueue());

        window.present();
        frameFence.endFrame(window.getCommandQueue());
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
