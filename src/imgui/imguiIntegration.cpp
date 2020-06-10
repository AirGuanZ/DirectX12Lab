#include <agz/d3d12/imgui/imgui.h>
#include <agz/d3d12/imgui/imgui_impl_dx12.h>
#include <agz/d3d12/imgui/imgui_impl_win32.h>

#include <agz/d3d12/imgui/imguiIntegration.h>

AGZ_D3D12_BEGIN

class ImGuiIntegration::InputDispatcher
    : public event::receiver_t<MouseButtonDownEvent>,
      public event::receiver_t<MouseButtonUpEvent>,
      public event::receiver_t<WheelScrollEvent>,
      public event::receiver_t<RawKeyDownEvent>,
      public event::receiver_t<RawKeyUpEvent>,
      public event::receiver_t<CharInputEvent>
{
    static int toImGuiMouseButton(MouseButton mb) noexcept
    {
        if(mb == MouseButton::Left)  return 0;
        if(mb == MouseButton::Right) return 1;
        return 2;
    }

public:

    void handle(const MouseButtonDownEvent &event) override
    {
        if(ImGui::GetCurrentContext())
            ImGui::GetIO().MouseDown[toImGuiMouseButton(event.button)] = true;
    }

    void handle(const MouseButtonUpEvent &event) override
    {
        if(ImGui::GetCurrentContext())
            ImGui::GetIO().MouseDown[toImGuiMouseButton(event.button)] = false;
    }

    void handle(const WheelScrollEvent &event) override
    {
        if(ImGui::GetCurrentContext())
            ImGui::GetIO().MouseWheel += event.offset;
    }

    void handle(const RawKeyDownEvent &event) override
    {
        if(ImGui::GetCurrentContext() && event.vk < 256)
            ImGui::GetIO().KeysDown[event.vk] = true;
    }

    void handle(const RawKeyUpEvent &event) override
    {

        if(ImGui::GetCurrentContext() && event.vk < 256)
            ImGui::GetIO().KeysDown[event.vk] = false;
    }

    void handle(const CharInputEvent &event) override
    {
        if(ImGui::GetCurrentContext() && event.ch < 0x10000)
        {
            ImGui::GetIO().AddInputCharacter(event.ch);
        }
    }

    void attachTo(Mouse *mouse, Keyboard *keyboard)
    {
        mouse->attach(static_cast<receiver_t<MouseButtonDownEvent>*>(this));
        mouse->attach(static_cast<receiver_t<MouseButtonUpEvent>  *>(this));
        mouse->attach(static_cast<receiver_t<WheelScrollEvent>    *>(this));

        keyboard->attach(static_cast<receiver_t<RawKeyDownEvent> *>(this));
        keyboard->attach(static_cast<receiver_t<RawKeyUpEvent>   *>(this));
        keyboard->attach(static_cast<receiver_t<CharInputEvent>  *>(this));
    }
};

ImGuiIntegration::ImGuiIntegration(
    Window                     &window,
    ID3D12DescriptorHeap       *descHeap,
    D3D12_CPU_DESCRIPTOR_HANDLE cpuSRV,
    D3D12_GPU_DESCRIPTOR_HANDLE gpuSRV)
{
    ImGui::CreateContext();
    ImGui_ImplWin32_Init(window.getWindowHandle());
    ImGui_ImplDX12_Init(
        window.getDevice(),
        window.getImageCount(),
        window.getImageFormat(),
        descHeap,
        cpuSRV,
        gpuSRV);
    ImGui::StyleColorsLight();

    inputDispatcher_ = std::make_unique<InputDispatcher>();
    inputDispatcher_->attachTo(window.getMouse(), window.getKeyboard());
}

ImGuiIntegration::~ImGuiIntegration()
{
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext(ImGui::GetCurrentContext());
}

void ImGuiIntegration::newFrame()
{
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void ImGuiIntegration::render(ID3D12GraphicsCommandList *cmdList)
{
    ImGui::Render();
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmdList);
}

AGZ_D3D12_END
