#include <iostream>

#include <d3dx12.h>

#include <agz/d3d12/debugLayer.h>
#include <agz/d3d12/perFrameCmdList.h>
#include <agz/d3d12/window.h>

using namespace agz::d3d12;

void run()
{
    enableD3D12DebugLayerInDebugMode();

    WindowDesc desc;
    desc.title = L"00.init";

    Window window(desc);

    window.getKeyboard()->attach(
        std::make_shared<KeyDownHandler>(
            [&](const auto &e)
    {
        if(e.key == KEY_ESCAPE)
            window.setCloseFlag(true);
    }));

    PerFrameCommandList cmdList(window);

    while(!window.getCloseFlag())
    {
        window.doEvents();
        window.waitForFocus();

        cmdList.startFrame();
        cmdList.resetCommandList();

        // record command list

        const auto barrier1 = CD3DX12_RESOURCE_BARRIER::Transition(
            window.getImage(cmdList.getFrameIndex()),
            D3D12_RESOURCE_STATE_PRESENT,
            D3D12_RESOURCE_STATE_RENDER_TARGET);
        cmdList->ResourceBarrier(1, &barrier1);
        
        auto rtvHandle = window.getImageDescHandle(cmdList.getFrameIndex());
        cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

        const float CLEAR_COLOR[] = { 0, 1, 1, 0 };
        cmdList->ClearRenderTargetView(rtvHandle, CLEAR_COLOR, 0, nullptr);

        const auto barrier2 = CD3DX12_RESOURCE_BARRIER::Transition(
            window.getImage(cmdList.getFrameIndex()),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PRESENT);
        cmdList->ResourceBarrier(1, &barrier2);

        cmdList->Close();

        // render

        ID3D12CommandList *cmdListArr[] = { cmdList.getCmdList() };
        window.getCommandQueue()->ExecuteCommandLists(1, cmdListArr);

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
    catch(const std::exception &e)
    {
        std::cerr << e.what() << std::endl;
        return -1;
    }
}
