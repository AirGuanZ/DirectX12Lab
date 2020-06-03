#pragma once

#include <agz/d3d12/common.h>
#include <agz/utility/event.h>

AGZ_D3D12_BEGIN

struct WindowCloseEvent      { };
struct WindowGetFocusEvent   { };
struct WindowLostFocusEvent  { };
struct WindowPreResizeEvent  { };
struct WindowPostResizeEvent { int width; int height; };

using WindowCloseHandler      = event::functional_receiver_t<WindowCloseEvent>;
using WindowGetFocusHandler   = event::functional_receiver_t<WindowGetFocusEvent>;
using WindowLostFocusHandler  = event::functional_receiver_t<WindowLostFocusEvent>;
using WindowPreResizeHandler  = event::functional_receiver_t<WindowPreResizeEvent>;
using WindowPostResizeHandler = event::functional_receiver_t<WindowPostResizeEvent>;

using WindowEventManager = event::sender_t<
    WindowCloseEvent,
    WindowGetFocusEvent,
    WindowLostFocusEvent,
    WindowPreResizeEvent,
    WindowPostResizeEvent>;

AGZ_D3D12_END
