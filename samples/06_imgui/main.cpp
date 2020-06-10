#include <iostream>

#include <agz/d3d12/imgui/imgui.h>
#include <agz/d3d12/imgui/imfilebrowser.h>
#include <agz/d3d12/lab.h>

using namespace agz::d3d12;

void run()
{
    enableD3D12DebugLayerInDebugMode();

    WindowDesc windowDesc;
    windowDesc.title = L"06.imgui";

    Window window(windowDesc);
    auto device = window.getDevice();

    PerFrameCommandList cmdList(window);

    DescriptorHeap rscHeap;
    rscHeap.initialize(
        device, 100, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);

    auto imguiSRV = *rscHeap.allocSingle();
    ImGuiIntegration imgui(
        window, rscHeap.getRawHeap(), imguiSRV, imguiSRV);

    ImGui::FileBrowser fileBrowser;
    fileBrowser.SetTitle("hello, imgui!");
    fileBrowser.SetWindowSize(600, 400);

    while(!window.getCloseFlag())
    {
        window.doEvents();
        window.waitForFocus();

        imgui.newFrame();

        cmdList.startFrame();
        cmdList.resetCommandList();

        if(window.getKeyboard()->isPressed(KEY_ESCAPE))
            window.setCloseFlag(true);

        const auto barrier1 = CD3DX12_RESOURCE_BARRIER::Transition(
            window.getCurrentImage(),
            D3D12_RESOURCE_STATE_PRESENT,
            D3D12_RESOURCE_STATE_RENDER_TARGET);
        cmdList->ResourceBarrier(1, &barrier1);

        auto rtvHandle = window.getCurrentImageDescHandle();
        cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

        const float CLEAR_COLOR[] = { 0, 1, 1, 0 };
        cmdList->ClearRenderTargetView(rtvHandle, CLEAR_COLOR, 0, nullptr);

        ID3D12DescriptorHeap *rawDescHeap[] = { rscHeap.getRawHeap() };
        cmdList->SetDescriptorHeaps(1, rawDescHeap);

        if(ImGui::Begin("imgui", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            if(ImGui::Button("exit"))
                window.setCloseFlag(true);

            ImGui::SameLine();

            if(ImGui::Button("open file browser"))
                fileBrowser.Open();
        }
        ImGui::End();

        fileBrowser.Display();

        if(fileBrowser.HasSelected())
        {
            const auto path = fileBrowser.GetSelected();
            fileBrowser.ClearSelected();

            std::cout << absolute(path) << std::endl;
        }

        imgui.render(cmdList);

        const auto barrier2 = CD3DX12_RESOURCE_BARRIER::Transition(
            window.getCurrentImage(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PRESENT);
        cmdList->ResourceBarrier(1, &barrier2);

        AGZ_D3D12_CHECK_HR(cmdList->Close());

        window.executeOneCmdList(cmdList.getCmdList());
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
