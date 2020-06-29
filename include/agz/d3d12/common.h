#pragma once

#include <d3d12.h>
#include <dxgi.h>

#include <string>
#include <system_error>

#include <agz/utility/math.h>

#include <wrl/client.h>

#define AGZ_D3D12_BEGIN namespace agz::d3d12 {
#define AGZ_D3D12_END   }

AGZ_D3D12_BEGIN

using Vec2 = math::vec2f;
using Vec3 = math::vec3f;
using Vec4 = math::vec4f;

using Vec2i = math::vec2i;
using Vec3i = math::vec3i;

using Mat4   = math::mat4f_c;
using Trans4 = Mat4::right_transform;

using Microsoft::WRL::ComPtr;

class D3D12LabException : public std::runtime_error
{
public:

    using runtime_error::runtime_error;
};

inline D3D12_CLEAR_VALUE CreateClearColorValue(
    DXGI_FORMAT format, float r, float g, float b, float a) noexcept
{
    D3D12_CLEAR_VALUE ret;
    ret.Format   = format;
    ret.Color[0] = r;
    ret.Color[1] = g;
    ret.Color[2] = b;
    ret.Color[3] = a;
    return ret;
}

inline D3D12_CLEAR_VALUE CreateClearDepthStencilValue(
    DXGI_FORMAT format, float d, UINT8 s) noexcept
{
    D3D12_CLEAR_VALUE ret;
    ret.Format               = format;
    ret.DepthStencil.Depth   = d;
    ret.DepthStencil.Stencil = s;
    return ret;
}

#define AGZ_D3D12_DECL_EVENT_MGR_HANDLER(EventMgr, EventName)                  \
    void attach(event::receiver_t<EventName> *handler)                         \
        { EventMgr.attach(handler); }                                          \
    void attach(std::shared_ptr<event::receiver_t<EventName>> handler)         \
        { EventMgr.attach(handler); }                                          \
    void detach(event::receiver_t<EventName> *handler)                         \
        { EventMgr.detach(handler); }                                          \
    void detach(std::shared_ptr<event::receiver_t<EventName>> handler)         \
        { EventMgr.detach(handler); }

#define AGZ_D3D12_CHECK_HR(X)                                                  \
    do {                                                                       \
        const HRESULT autoname_hr = X;                                         \
        if(!SUCCEEDED(autoname_hr))                                            \
        {                                                                      \
            throw D3D12LabException(                                           \
                    std::string("errcode = ")                                  \
                    + std::to_string(autoname_hr) + "."                        \
                    + std::system_category().message(autoname_hr));            \
        }                                                                      \
    } while(false)

#define AGZ_D3D12_CHECK_HR_MSG(E, X)                                           \
    do {                                                                       \
        const HRESULT autoname_hr = X;                                         \
        if(!SUCCEEDED(autoname_hr))                                            \
        {                                                                      \
            throw D3D12LabException((E) + std::string(".errcode = ")           \
                    + std::to_string(autoname_hr) + "."                        \
                    + std::system_category().message(autoname_hr));            \
        }                                                                      \
    } while(false)

AGZ_D3D12_END
