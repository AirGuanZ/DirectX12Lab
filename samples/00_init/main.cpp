#include <iostream>

#include <d3dx12.h>

#include <agz/d3d12/debugLayer.h>
#include <agz/d3d12/window.h>

using namespace agz::d3d12;

void run()
{
    enableD3D12DebugLayer();

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

    // create per render target resources

    std::vector<ComPtr<ID3D12CommandAllocator>> cmdAllocs;
    std::vector<ComPtr<ID3D12Fence>>            fences;
    std::vector<UINT64>                         fenceValues;

    for(int i = 0; i < window.getImageCount(); ++i)
    {
        // command list allocator

        ComPtr<ID3D12CommandAllocator> alloc;
        AGZ_D3D12_CHECK_HR(
            window.getDevice()->CreateCommandAllocator(
                D3D12_COMMAND_LIST_TYPE_DIRECT,
                IID_PPV_ARGS(alloc.GetAddressOf())));
        cmdAllocs.push_back(alloc);

        // fence

        ComPtr<ID3D12Fence> fence;
        AGZ_D3D12_CHECK_HR(
            window.getDevice()->CreateFence(
                0, D3D12_FENCE_FLAG_NONE,
                IID_PPV_ARGS(fence.GetAddressOf())));

        fences.push_back(fence);
        fenceValues.push_back(0);
    }

    // fence event

    HANDLE fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if(!fenceEvent)
        throw D3D12LabException("failed to create fence event");

    // create command list

    ComPtr<ID3D12GraphicsCommandList> cmdList;
    AGZ_D3D12_CHECK_HR(
        window.getDevice()->CreateCommandList(
            0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdAllocs[0].Get(), nullptr,
            IID_PPV_ARGS(cmdList.GetAddressOf())));
    cmdList->Close();

    while(!window.getCloseFlag())
    {
        window.doEvents();
        window.waitForFocus();

        // wait for image available

        int imgIdx = window.getCurrentImageIndex();

        if(fences[imgIdx]->GetCompletedValue() < fenceValues[imgIdx])
        {
            fences[imgIdx]->SetEventOnCompletion(fenceValues[imgIdx], fenceEvent);
            
            WaitForSingleObject(fenceEvent, INFINITE);
        }

        fenceValues[imgIdx]++;

        // reset cmd list

        cmdAllocs[imgIdx]->Reset();
        cmdList->Reset(cmdAllocs[imgIdx].Get(), nullptr);

        // render target transition

        const auto barrier1 = CD3DX12_RESOURCE_BARRIER::Transition(
            window.getImage(imgIdx).Get(),
            D3D12_RESOURCE_STATE_PRESENT,
            D3D12_RESOURCE_STATE_RENDER_TARGET);
        cmdList->ResourceBarrier(1, &barrier1);

        // set render target & clear

        auto rtvHandle = window.getImageDescHandle(imgIdx);
        cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

        const float CLEAR_COLOR[] = { 0, 1, 1, 0 };
        cmdList->ClearRenderTargetView(rtvHandle, CLEAR_COLOR, 0, nullptr);

        // render target transition

        const auto barrier2 = CD3DX12_RESOURCE_BARRIER::Transition(
            window.getImage(imgIdx).Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PRESENT);
        cmdList->ResourceBarrier(1, &barrier2);

        cmdList->Close();

        // render

        ID3D12CommandList *cmdListArr[] = { cmdList.Get() };
        window.getCommandQueue()->ExecuteCommandLists(1, cmdListArr);

        window.getCommandQueue()->Signal(
            fences[imgIdx].Get(), fenceValues[imgIdx]);

        window.present();
    }
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
