#include <iostream>

#include "./deferred.h"

void run()
{
    enableD3D12DebugLayerInDebugMode();

    WindowDesc desc;
    desc.title = L"04.deferred";

    Window window(desc);

    auto mouse    = window.getMouse();
    auto keyboard = window.getKeyboard();
    auto device   = window.getDevice();

    PerFrameCommandList frameCmdList(window);

    // SRV/CBV/UAV desc heap

    DescriptorHeap rscHeap;
    rscHeap.initialize(
        device, 100, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);

    // renderer

    DeferredRenderer renderer(window, rscHeap);
    renderer.setLight({ -4, 5, 0 }, { 10, 24, 24 }, { 0.05f, 0.05f, 0.05f });

    // mesh

    ResourceUploader uploader(window, 1);

    std::vector<Mesh> meshes(2);
    meshes[0].loadFromFile(
        window, uploader, rscHeap.getRootSubheap(),
        "./asset/03_cube.obj", "./asset/03_texture.png");
    meshes[1].loadFromFile(
        window, uploader, rscHeap.getRootSubheap(),
        "./asset/03_cube.obj", "./asset/03_texture.png");

    uploader.waitForIdle();

    meshes[0].setWorldTransform(Mat4::identity());
    meshes[1].setWorldTransform(
        Trans4::rotate_y(0.6f) *
        Trans4::translate({ 0, 0, -3 }));

    // camera & cursor lock

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

    while(!window.getCloseFlag())
    {
        window.doEvents();
        window.waitForFocus();

        frameCmdList.startFrame();
        frameCmdList.resetCommandList();

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

        // render target transition

        const auto barrier1 = CD3DX12_RESOURCE_BARRIER::Transition(
            window.getCurrentImage(),
            D3D12_RESOURCE_STATE_PRESENT,
            D3D12_RESOURCE_STATE_RENDER_TARGET);
        frameCmdList->ResourceBarrier(1, &barrier1);

        // desc heap

        auto rawRSCHeap = rscHeap.getRawHeap();
        frameCmdList->SetDescriptorHeaps(1, &rawRSCHeap);

        // g-buffer

        renderer.startGBuffer(frameCmdList.getCmdList());

        for(auto &m : meshes)
            m.draw(
                frameCmdList.getCmdList(), window.getCurrentImageIndex(), camera);

        renderer.endGBuffer(frameCmdList.getCmdList());

        // lighting

        const auto rtvHandle = window.getCurrentImageDescHandle();
        frameCmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

        const float CLEAR_COLOR[] = { 0, 0, 0, 0 };
        frameCmdList->ClearRenderTargetView(rtvHandle, CLEAR_COLOR, 0, nullptr);

        frameCmdList->RSSetViewports(1, &window.getDefaultViewport());
        frameCmdList->RSSetScissorRects(1, &window.getDefaultScissorRect());

        renderer.render(frameCmdList.getCmdList());

        // render target transition

        const auto barrier2 = CD3DX12_RESOURCE_BARRIER::Transition(
            window.getCurrentImage(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PRESENT);
        frameCmdList->ResourceBarrier(1, &barrier2);

        AGZ_D3D12_CHECK_HR(frameCmdList->Close());

        // render & present

        window.executeOneCmdList(frameCmdList.getCmdList());
        window.present();

        frameCmdList.endFrame();
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
