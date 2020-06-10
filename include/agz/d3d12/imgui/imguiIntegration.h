#pragma once

#include <d3d12.h>

#include <agz/d3d12/cmd/perFrameCmdList.h>
#include <agz/d3d12/descriptor/descriptorHeap.h>
#include <agz/d3d12/window/keyboard.h>
#include <agz/d3d12/window/mouse.h>

AGZ_D3D12_BEGIN

class ImGuiIntegration : public misc::uncopyable_t
{
public:

    ImGuiIntegration(
        Window                     &window,
        ID3D12DescriptorHeap       *descHeap,
        D3D12_CPU_DESCRIPTOR_HANDLE cpuSRV,
        D3D12_GPU_DESCRIPTOR_HANDLE gpuSRV);

    ~ImGuiIntegration();

    void newFrame();

    void render(ID3D12GraphicsCommandList *cmdList);

private:

    class InputDispatcher;

    std::unique_ptr<InputDispatcher> inputDispatcher_;
};

AGZ_D3D12_END
