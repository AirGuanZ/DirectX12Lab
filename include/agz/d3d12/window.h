#pragma once

#include <chrono>
#include <string>

#include <d3d12.h>

#include <agz/d3d12/keyboard.h>
#include <agz/d3d12/mouse.h>
#include <agz/d3d12/windowEvent.h>

AGZ_D3D12_LAB_BEGIN

struct WindowDesc
{
    // window

    int clientWidth  = 640;
    int clientHeight = 480;

    std::wstring title = L"D3D12 Lab";

    bool resizable  = true;
    bool fullscreen = false;

    // d3d12

    int multisampleCount   = 1;
    int multisampleQuality = 0;

    int imageCount = 2;

    DWORD getStyle() const noexcept;
};

struct WindowImplData;

class Window
{
public:

    explicit Window(const WindowDesc &desc);

    ~Window();

    // win32 events

    void doEvents();

    bool isInFocus() const noexcept;

    void waitForFocus();

    // close flag

    bool getCloseFlag() const noexcept;

    void setCloseFlag(bool closeFlag) noexcept;

    // keyboard/mouse

    Keyboard *getKeyboard() const noexcept;

    Mouse *getMouse() const noexcept;

    // swap chain image

    int getImageCount() const noexcept;

    int getCurrentImageIndex() const;

    ComPtr<ID3D12Resource> getImage(int index) const noexcept;

    CD3DX12_CPU_DESCRIPTOR_HANDLE getImageDescHandle(int index) const noexcept;

    UINT getImageDescSize() const noexcept;

    void present() const noexcept;

    // d3d12 device/queue

    ID3D12Device *getDevice();

    ID3D12CommandQueue *getCommandQueue();

    void waitCommandQueueIdle();

    // event handler manager

    AGZ_D3D12_DECL_EVENT_MGR_HANDLER(eventMgr_, WindowCloseEvent)
    AGZ_D3D12_DECL_EVENT_MGR_HANDLER(eventMgr_, WindowGetFocusEvent)
    AGZ_D3D12_DECL_EVENT_MGR_HANDLER(eventMgr_, WindowLostFocusEvent)
    AGZ_D3D12_DECL_EVENT_MGR_HANDLER(eventMgr_, WindowPreResizeEvent)
    AGZ_D3D12_DECL_EVENT_MGR_HANDLER(eventMgr_, WindowPostResizeEvent)

    // for internal use

    void _msgClose();

    void _msgKeyDown(KeyCode keycode);

    void _msgKeyUp(KeyCode keycode);

    void _msgCharInput(uint32_t ch);

    void _msgRawKeyDown(uint32_t vk);

    void _msgRawKeyUp(uint32_t vk);

    void _msgGetFocus();

    void _msgLostFocus();

    void _msgResize();

private:

    void initWin32Window(const WindowDesc &desc);

    void initD3D12(const WindowDesc &desc);

    void initKeyboardAndMouse();

    std::unique_ptr<WindowImplData> impl_;

    WindowEventManager eventMgr_;
};

AGZ_D3D12_LAB_END
