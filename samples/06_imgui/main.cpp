#include <iostream>

#include <agz/d3d12/imgui/imgui.h>
#include <agz/d3d12/imgui/imfilebrowser.h>
#include <agz/d3d12/lab.h>
#include <agz/utility/image.h>

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

    auto imguiSRV = rscHeap.allocSingle();
    ImGuiIntegration imgui(
        window, rscHeap.getRawHeap(), imguiSRV, imguiSRV);

    ResourceUploader uploader(window, 1);

    const auto texData = agz::texture::texture2d_t<agz::math::color4b>(
        agz::img::load_rgba_from_file("./asset/05_mipmap.png"));
    if(!texData.is_available())
        throw std::runtime_error("failed to load image");

    Texture2D tex;
    tex.initialize(
        device,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        texData.width(),
        texData.height(),
        1, 1, 1, 0, {}, {});

    uploader.uploadTex2DData(
        tex, ResourceUploader::Tex2DSubInitData{ texData.raw_data() },
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    uploader.waitForIdle();

    auto texSRV = rscHeap.allocSingle();
    tex.createSRV(texSRV);

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

        if(ImGui::Begin("imgui", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            if(ImGui::Button("exit"))
                window.setCloseFlag(true);

            ImGui::SameLine();

            if(ImGui::Button("open file browser"))
                fileBrowser.Open();

            ImGui::Image(
                ImTextureID(texSRV.getGPUHandle().ptr),
                { float(texData.width()), float(texData.height()) });
        }
        ImGui::End();

        fileBrowser.Display();

        if(fileBrowser.HasSelected())
        {
            const auto path = fileBrowser.GetSelected();
            fileBrowser.ClearSelected();

            std::cout << absolute(path) << std::endl;
        }

        cmdList->ResourceBarrier(
            1, agz::get_temp_ptr(CD3DX12_RESOURCE_BARRIER::Transition(
                window.getCurrentImage(),
                D3D12_RESOURCE_STATE_PRESENT,
                D3D12_RESOURCE_STATE_RENDER_TARGET)));

        auto rtvHandle = window.getCurrentImageDescHandle();
        cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

        const float CLEAR_COLOR[] = { 0, 1, 1, 0 };
        cmdList->ClearRenderTargetView(rtvHandle, CLEAR_COLOR, 0, nullptr);

        ID3D12DescriptorHeap *rawDescHeap[] = { rscHeap.getRawHeap() };
        cmdList->SetDescriptorHeaps(1, rawDescHeap);

        imgui.render(cmdList);

        cmdList->ResourceBarrier(
            1, agz::get_temp_ptr(CD3DX12_RESOURCE_BARRIER::Transition(
                window.getCurrentImage(),
                D3D12_RESOURCE_STATE_RENDER_TARGET,
                D3D12_RESOURCE_STATE_PRESENT)));

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
