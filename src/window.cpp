#include <thread>

#include <d3dx12.h>
#include <dxgi1_4.h>
#include <dxgidebug.h>

#include <agz/d3d12/sync/cmdQueueWaiter.h>
#include <agz/d3d12/window/window.h>

AGZ_D3D12_BEGIN

namespace impl
{

    LRESULT CALLBACK windowMessageProc(HWND, UINT, WPARAM, LPARAM);

    std::unordered_map<HWND, Window*> &handleToWindow()
    {
        static std::unordered_map<HWND, Window *> ret;
        return ret;
    }

    Vec2i clientSizeToWindowSize(DWORD style, const Vec2i &clientSize)
    {
        RECT winRect = { 0, 0, clientSize.x, clientSize.y };
        if(!AdjustWindowRect(&winRect, style, FALSE))
            throw D3D12LabException("failed to compute window size");
        return { winRect.right - winRect.left, winRect.bottom - winRect.top };
    }

} // namespace  impl

struct Window::WindowImplData
{
    bool exiting = false;

    // win32 window

    DWORD style = 0;

    std::wstring className;

    HWND hWindow        = nullptr;
    HINSTANCE hInstance = nullptr;

    int clientWidth  = 0;
    int clientHeight = 0;

    bool shouldClose = false;
    bool inFocus     = true;

    // d3d12

    bool vsync = true;

    DXGI_FORMAT backbufferFormat = DXGI_FORMAT_UNKNOWN;

    ComPtr<ID3D12Device>       device;
    ComPtr<ID3D12CommandQueue> cmdQueue;
    
    ComPtr<IDXGISwapChain3> swapChain;

    int swapChainImageCount = 0;
    int currentImageIndex_ = 0;
    std::vector<ComPtr<ID3D12Resource>> swapChainBuffers;

    RawDescriptorHeap RTVHeap;

    std::unique_ptr<CommandQueueWaiter> cmdQueueWaiter;

    // viewport & scissor

    D3D12_VIEWPORT fullwindowViewport = {};
    D3D12_RECT     fullwindowRect     = {};
    
    // events

    std::unique_ptr<Keyboard> keyboard;
    std::unique_ptr<Mouse>    mouse;
};

DWORD WindowDesc::getStyle() const noexcept
{
    DWORD style = WS_POPUPWINDOW | WS_CAPTION | WS_MINIMIZEBOX;
    if(resizable)
        style |= WS_SIZEBOX | WS_MAXIMIZEBOX;
    return style;
}

void Window::initWin32Window(const WindowDesc &desc)
{
    impl_ = std::make_unique<WindowImplData>();

    // hInstance & class name

    impl_->hInstance = GetModuleHandle(nullptr);
    impl_->className = L"D3D12LabWindowClass" + std::to_wstring(
                                reinterpret_cast<size_t>(this));

    // register window class

    WNDCLASSEXW wc;
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = impl::windowMessageProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = impl_->hInstance;
    wc.hIcon         = nullptr;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BACKGROUND + 1);
    wc.lpszMenuName  = nullptr;
    wc.lpszClassName = impl_->className.c_str();
    wc.hIconSm       = nullptr;

    if(!RegisterClassExW(&wc))
        throw D3D12LabException("failed to register window class");

    // screen info

    const int screenW = GetSystemMetrics(SM_CXSCREEN);
    const int screenH = GetSystemMetrics(SM_CYSCREEN);

    RECT workAreaRect;
    SystemParametersInfo(SPI_GETWORKAREA, 0, &workAreaRect, 0);

    const int workAreaW = workAreaRect.right - workAreaRect.left;
    const int workAreaH = workAreaRect.bottom - workAreaRect.top;

    // window style & size

    const int clientW = desc.fullscreen ? screenW : desc.clientWidth;
    const int clientH = desc.fullscreen ? screenH : desc.clientHeight;

    impl_->style = desc.getStyle();

    const auto winSize = impl::clientSizeToWindowSize(
        impl_->style, { clientW, clientH });

    // window pos

    int left, top;
    if(desc.fullscreen)
    {
        left = (screenW - winSize.x) / 2;
        top  = (screenH - winSize.y) / 2;
    }
    else
    {
        left = workAreaRect.left + (workAreaW - winSize.x) / 2;
        top  = workAreaRect.top  + (workAreaH - winSize.y) / 2;
    }

    // create window

    impl_->hWindow = CreateWindowW(
        impl_->className.c_str(), desc.title.c_str(),
        impl_->style, left, top, winSize.x, winSize.y,
        nullptr, nullptr, impl_->hInstance, nullptr);
    if(!impl_->hWindow)
        throw D3D12LabException("failed to create win32 window");

    // show & focus

    ShowWindow(impl_->hWindow, SW_SHOW);
    UpdateWindow(impl_->hWindow);
    SetForegroundWindow(impl_->hWindow);
    SetFocus(impl_->hWindow);

    // client size

    RECT clientRect;
    GetClientRect(impl_->hWindow, &clientRect);
    impl_->clientWidth  = clientRect.right - clientRect.left;
    impl_->clientHeight = clientRect.bottom - clientRect.top;

    // event dispatcher

    impl::handleToWindow().insert({ impl_->hWindow, this });
}

void Window::initD3D12(const WindowDesc &desc)
{
    impl_->vsync = desc.vsync;

    // factory

    ComPtr<IDXGIFactory4> dxgiFactory;
    AGZ_D3D12_CHECK_HR_MSG(
        "failed to create dxgi factory",
        CreateDXGIFactory1(IID_PPV_ARGS(dxgiFactory.GetAddressOf())));

    // device

    AGZ_D3D12_CHECK_HR_MSG(
        "failed to create d3d12 device",
        D3D12CreateDevice(
            nullptr, D3D_FEATURE_LEVEL_12_0,
            IID_PPV_ARGS(impl_->device.GetAddressOf())));
    
    // command queue

    D3D12_COMMAND_QUEUE_DESC cmdQueueDesc = {};
    AGZ_D3D12_CHECK_HR_MSG(
        "failed to create d3d12 command queue",
        impl_->device->CreateCommandQueue(
            &cmdQueueDesc, IID_PPV_ARGS(impl_->cmdQueue.GetAddressOf())));

    // swap chain

    impl_->backbufferFormat = desc.backbufferFormat;

    DXGI_SWAP_CHAIN_DESC swapChainDesc;

    swapChainDesc.BufferDesc.Width                   = impl_->clientWidth;
    swapChainDesc.BufferDesc.Height                  = impl_->clientHeight;
    swapChainDesc.BufferDesc.Format                  = desc.backbufferFormat;
    swapChainDesc.BufferDesc.RefreshRate.Numerator   = 60;
    swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
    swapChainDesc.BufferDesc.Scaling                 = DXGI_MODE_SCALING_UNSPECIFIED;
    swapChainDesc.BufferDesc.ScanlineOrdering        = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    
    swapChainDesc.SampleDesc.Count   = 1;
    swapChainDesc.SampleDesc.Quality = 0;

    swapChainDesc.BufferUsage  = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount  = desc.imageCount;
    swapChainDesc.OutputWindow = impl_->hWindow;
    swapChainDesc.Windowed     = TRUE;
    swapChainDesc.SwapEffect   = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.Flags        = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    IDXGISwapChain *tSwapChain;
    AGZ_D3D12_CHECK_HR_MSG(
        "failed to create dxgi swap chain",
        dxgiFactory->CreateSwapChain(
            impl_->cmdQueue.Get(), &swapChainDesc, &tSwapChain));
    impl_->swapChain.Attach(static_cast<IDXGISwapChain3 *>(tSwapChain));

    impl_->swapChainImageCount = desc.imageCount;

    AGZ_SCOPE_GUARD({
        if(desc.fullscreen)
            impl_->swapChain->SetFullscreenState(TRUE, nullptr);
    });

    impl_->currentImageIndex_ = int(impl_->swapChain
                    ->GetCurrentBackBufferIndex());

    // render target descriptor heap

    impl_->RTVHeap = RawDescriptorHeap(
        impl_->device.Get(),
        desc.imageCount,
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
        false);

    impl_->swapChainBuffers.resize(desc.imageCount);

    for(int i = 0; i < desc.imageCount; ++i)
    {
        AGZ_D3D12_CHECK_HR_MSG(
            "failed to get swap chain buffer",
            impl_->swapChain->GetBuffer(
                i, IID_PPV_ARGS(impl_->swapChainBuffers[i].GetAddressOf())));

        impl_->device->CreateRenderTargetView(
            impl_->swapChainBuffers[i].Get(), nullptr,
            impl_->RTVHeap.getCPUHandle(i));
    }

    // command queue fence

    impl_->cmdQueueWaiter = std::make_unique<CommandQueueWaiter>(
        impl_->device.Get());

    // viewport & scissor

    impl_->fullwindowViewport = {
        0, 0,
        float(impl_->clientWidth),
        float(impl_->clientHeight),
        0, 1
    };

    impl_->fullwindowRect = {
        0, 0, impl_->clientWidth, impl_->clientHeight
    };
}

void Window::initKeyboardAndMouse()
{
    impl_->keyboard = std::make_unique<Keyboard>();
    impl_->mouse = std::make_unique<Mouse>(impl_->hWindow);
}

Window::Window(const WindowDesc &desc)
{
    initWin32Window(desc);
    initD3D12(desc);
    initKeyboardAndMouse();
}

Window::~Window()
{
    if(!impl_)
        return;

    impl_->exiting = true;

    // IMPROVE: exception thrown in destructor...
    try { waitCommandQueueIdle(); } catch(...) { }

    impl_->cmdQueueWaiter.reset();

    impl_->swapChainBuffers.clear();
    impl_->RTVHeap = {};

    if(impl_->swapChain)
        impl_->swapChain->SetFullscreenState(FALSE, nullptr);

    impl_->swapChain.Reset();
    impl_->cmdQueue.Reset();
    impl_->device.Reset();

#ifdef AGZ_DEBUG
    {
        ComPtr<IDXGIDebug> dxgiDebug;
        if(SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug))))
        {
            dxgiDebug->ReportLiveObjects(
                DXGI_DEBUG_ALL,
                DXGI_DEBUG_RLO_FLAGS(
                    DXGI_DEBUG_RLO_SUMMARY | DXGI_DEBUG_RLO_IGNORE_INTERNAL));
        }
    }
#endif

    if(impl_->hWindow)
    {
        impl::handleToWindow().erase(impl_->hWindow);
        DestroyWindow(impl_->hWindow);
    }

    UnregisterClassW(impl_->className.c_str(), impl_->hInstance);
}

HWND Window::getWindowHandle() const noexcept
{
    return impl_->hWindow;
}

void Window::doEvents()
{
    impl_->keyboard->_startUpdating();

    MSG msg;
    while(PeekMessage(&msg, impl_->hWindow, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    impl_->mouse->_update();

    impl_->keyboard->_endUpdating();
}

bool Window::isInFocus() const noexcept
{
    return impl_->inFocus;
}

void Window::waitForFocus()
{
    if(isInFocus())
        return;

    auto mouse = getMouse();

    const bool showCursor = mouse->isVisible();
    const bool lockCursor = mouse->isLocked();
    const int  lockX      = mouse->getLockX();
    const int  lockY      = mouse->getLockY();

    mouse->showCursor(true);
    mouse->setCursorLock(false, lockX, lockY);

    do
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        doEvents();

    } while(!isInFocus());

    mouse->showCursor(showCursor);
    mouse->setCursorLock(lockCursor, lockX, lockY);
    mouse->_update();
}

bool Window::getCloseFlag() const noexcept
{
    return impl_->shouldClose;
}

void Window::setCloseFlag(bool closeFlag) noexcept
{
    impl_->shouldClose = closeFlag;
}

Keyboard *Window::getKeyboard() const noexcept
{
    return impl_->keyboard.get();
}

Mouse *Window::getMouse() const noexcept
{
    return impl_->mouse.get();
}

int Window::getImageCount() const noexcept
{
    return impl_->swapChainImageCount;
}

int Window::getCurrentImageIndex() const
{
    return static_cast<int>(impl_->currentImageIndex_);
}

ID3D12Resource *Window::getCurrentImage() const noexcept
{
    return impl_->swapChainBuffers[impl_->currentImageIndex_].Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE Window::getCurrentImageDescHandle() const noexcept
{
    return impl_->RTVHeap.getCPUHandle(impl_->currentImageIndex_);
}

UINT Window::getImageDescSize() const noexcept
{
    return impl_->RTVHeap.getDescIncSize();
}

void Window::present() const
{
    AGZ_D3D12_CHECK_HR_MSG(
        "failed to present swap chain image",
        impl_->swapChain->Present(impl_->vsync ? 1 : 0, 0));
    impl_->currentImageIndex_ = (impl_->currentImageIndex_ + 1)
                               % impl_->swapChainImageCount;
}

int Window::getImageWidth() const noexcept
{
    return impl_->clientWidth;
}

int Window::getImageHeight() const noexcept
{
    return impl_->clientHeight;
}

float Window::getImageWOverH() const noexcept
{
    return float(getImageWidth()) / getImageHeight();
}

DXGI_FORMAT Window::getImageFormat() const noexcept
{
    return impl_->backbufferFormat;
}

const D3D12_VIEWPORT &Window::getDefaultViewport() const noexcept
{
    return impl_->fullwindowViewport;
}

const D3D12_RECT &Window::getDefaultScissorRect() const noexcept
{
    return impl_->fullwindowRect;
}

ID3D12Device *Window::getDevice() const noexcept
{
    return impl_->device.Get();
}

ID3D12CommandQueue *Window::getCommandQueue() const noexcept
{
    return impl_->cmdQueue.Get();
}

void Window::executeOneCmdList(ID3D12CommandList *cmdList) const noexcept
{
    impl_->cmdQueue->ExecuteCommandLists(1, &cmdList);
}

void Window::waitCommandQueueIdle() const
{
    impl_->cmdQueueWaiter->waitIdle(impl_->cmdQueue.Get());
}

void Window::_msgClose()
{
    impl_->shouldClose = true;
    eventMgr_.send(WindowCloseEvent{});
}

void Window::_msgKeyDown(KeyCode keycode)
{
    if(keycode != KEY_UNKNOWN)
        impl_->keyboard->_msgDown(keycode);
}

void Window::_msgKeyUp(KeyCode keycode)
{
    if(keycode != KEY_UNKNOWN)
        impl_->keyboard->_msgUp(keycode);
}

void Window::_msgCharInput(uint32_t ch)
{
    impl_->keyboard->_msgChar(ch);
}

void Window::_msgRawKeyDown(uint32_t vk)
{
    impl_->keyboard->_msgRawDown(vk);
}

void Window::_msgRawKeyUp(uint32_t vk)
{
    impl_->keyboard->_msgRawUp(vk);
}

void Window::_msgGetFocus()
{
    impl_->inFocus = true;
    eventMgr_.send(WindowGetFocusEvent{});
}

void Window::_msgLostFocus()
{
    impl_->inFocus = false;
    eventMgr_.send(WindowLostFocusEvent{});
}

void Window::_msgResize()
{
    if(impl_->exiting)
        return;

    waitCommandQueueIdle();

    eventMgr_.send(WindowPreResizeEvent{});

    // new client size

    RECT clientRect;
    GetClientRect(impl_->hWindow, &clientRect);
    impl_->clientWidth  = clientRect.right - clientRect.left;
    impl_->clientHeight = clientRect.bottom - clientRect.top;

    impl_->fullwindowViewport = {
        0, 0,
        float(impl_->clientWidth),
        float(impl_->clientHeight),
        0, 1
    };

    impl_->fullwindowRect = {
        0, 0, impl_->clientWidth, impl_->clientHeight
    };

    // resize swap chain buffers

    for(auto &b : impl_->swapChainBuffers)
        b.Reset();

    AGZ_D3D12_CHECK_HR_MSG(
        "failed to resize swap chain buffers",
        impl_->swapChain->ResizeBuffers(
            impl_->swapChainImageCount,
            impl_->clientWidth, impl_->clientHeight,
            impl_->backbufferFormat,
            DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH));

    impl_->currentImageIndex_ = int(impl_->swapChain
                    ->GetCurrentBackBufferIndex());

    // rtv desc heap

    for(int i = 0; i < impl_->swapChainImageCount; ++i)
    {
        AGZ_D3D12_CHECK_HR_MSG(
            "failed to get swap chain buffer",
            impl_->swapChain->GetBuffer(
                i, IID_PPV_ARGS(impl_->swapChainBuffers[i].GetAddressOf())));

        impl_->device->CreateRenderTargetView(
            impl_->swapChainBuffers[i].Get(), nullptr,
            impl_->RTVHeap.getCPUHandle(i));
    }

    eventMgr_.send(WindowPostResizeEvent{});
}

LRESULT impl::windowMessageProc(
    HWND hWindow, UINT msg, WPARAM wParam, LPARAM lParam)
{
    const auto winIt = handleToWindow().find(hWindow);
    if(winIt == handleToWindow().end())
        return DefWindowProcW(hWindow, msg, wParam, lParam);
    const auto win = winIt->second;

    win->getMouse()->_msg(msg, wParam);

    switch(msg)
    {
    case WM_CLOSE:
        win->_msgClose();
        return 0;
    case WM_KEYDOWN:
        win->_msgKeyDown(
            event::keycode::win_vk_to_keycode(static_cast<int>(wParam)));
        [[fallthrough]];
    case WM_SYSKEYDOWN:
        win->_msgRawKeyDown(static_cast<uint32_t>(wParam));
        return 0;
    case WM_KEYUP:
        win->_msgKeyUp(
            event::keycode::win_vk_to_keycode(static_cast<int>(wParam)));
        [[fallthrough]];
    case WM_SYSKEYUP:
        win->_msgRawKeyUp(static_cast<uint32_t>(wParam));
        return 0;
    case WM_CHAR:
        if(wParam > 0 && wParam < 0x10000)
            win->_msgCharInput(static_cast<uint32_t>(wParam));
        break;
    case WM_SETFOCUS:
        win->_msgGetFocus();
        break;
    case WM_KILLFOCUS:
        win->_msgLostFocus();
        break;
    case WM_SIZE:
        if(wParam != SIZE_MINIMIZED)
            win->_msgResize();
        break;
    default:
        return DefWindowProcW(hWindow, msg, wParam, lParam);
    }

    return DefWindowProcW(hWindow, msg, wParam, lParam);
}

AGZ_D3D12_END
