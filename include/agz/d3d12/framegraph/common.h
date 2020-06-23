#pragma once

#include <agz/d3d12/common.h>

#define AGZ_D3D12_FG_BEGIN namespace agz::d3d12::fg {
#define AGZ_D3D12_FG_END   }

AGZ_D3D12_FG_BEGIN

struct ResourceIndex
{
    int32_t idx = -1;

    bool isNil() const noexcept { return idx < 0; }

    bool operator<(ResourceIndex rhs) const noexcept { return idx < rhs.idx; }
};

constexpr ResourceIndex RESOURCE_NIL = { -1 };

struct PassIndex
{
    int32_t idx = -1;

    bool isNil() const noexcept { return idx < 0; }

    bool operator<(PassIndex rhs) const noexcept { return idx < rhs.idx; }
};

constexpr PassIndex PASS_NIL = { -1 };

struct Register
{
    constexpr Register(UINT num) noexcept : Register(0, num) { }

    constexpr Register(UINT space, UINT num) noexcept
        : registerSpace(space), registerNumber(num)
    {
        
    }

    UINT registerSpace;
    UINT registerNumber;
};

struct TRegister : Register { using Register::Register; };
struct BRegister : Register { using Register::Register; };
struct URegister : Register { using Register::Register; };
struct SRegister : Register { using Register::Register; };

#define AGZ_D3D12_DECL_REG(X, Y) \
    constexpr TRegister s##X##t##Y = { X, Y }; \
    constexpr BRegister s##X##b##Y = { X, Y }; \
    constexpr URegister s##X##u##Y = { X, Y }; \
    constexpr SRegister s##X##s##Y = { X, Y };

#define AGZ_D3D12_DECL_REG_SPACE(X) \
    AGZ_D3D12_DECL_REG(X, 0)  AGZ_D3D12_DECL_REG(X, 1)  AGZ_D3D12_DECL_REG(X, 2)  AGZ_D3D12_DECL_REG(X, 3) \
    AGZ_D3D12_DECL_REG(X, 4)  AGZ_D3D12_DECL_REG(X, 5)  AGZ_D3D12_DECL_REG(X, 6)  AGZ_D3D12_DECL_REG(X, 7) \
    AGZ_D3D12_DECL_REG(X, 8)  AGZ_D3D12_DECL_REG(X, 9)  AGZ_D3D12_DECL_REG(X, 10) AGZ_D3D12_DECL_REG(X, 11) \
    AGZ_D3D12_DECL_REG(X, 12) AGZ_D3D12_DECL_REG(X, 13) AGZ_D3D12_DECL_REG(X, 14) AGZ_D3D12_DECL_REG(X, 15) \
    AGZ_D3D12_DECL_REG(X, 16) AGZ_D3D12_DECL_REG(X, 17) AGZ_D3D12_DECL_REG(X, 18) AGZ_D3D12_DECL_REG(X, 19) \
    AGZ_D3D12_DECL_REG(X, 20) AGZ_D3D12_DECL_REG(X, 21) AGZ_D3D12_DECL_REG(X, 22) AGZ_D3D12_DECL_REG(X, 23) \
    AGZ_D3D12_DECL_REG(X, 24) AGZ_D3D12_DECL_REG(X, 25) AGZ_D3D12_DECL_REG(X, 26) AGZ_D3D12_DECL_REG(X, 27) \
    AGZ_D3D12_DECL_REG(X, 28) AGZ_D3D12_DECL_REG(X, 29) AGZ_D3D12_DECL_REG(X, 30) AGZ_D3D12_DECL_REG(X, 31) \
    AGZ_D3D12_DECL_REG(X, 32) AGZ_D3D12_DECL_REG(X, 33) AGZ_D3D12_DECL_REG(X, 34) AGZ_D3D12_DECL_REG(X, 35)

AGZ_D3D12_DECL_REG_SPACE(0)
AGZ_D3D12_DECL_REG_SPACE(1)
AGZ_D3D12_DECL_REG_SPACE(2)
AGZ_D3D12_DECL_REG_SPACE(3)
AGZ_D3D12_DECL_REG_SPACE(4)
AGZ_D3D12_DECL_REG_SPACE(5)
AGZ_D3D12_DECL_REG_SPACE(6)
AGZ_D3D12_DECL_REG_SPACE(7)

inline void InvokeAll() noexcept { }

template<typename F>
void InvokeAll(F &&f) noexcept(noexcept(std::forward<F>(f)))
{
    std::forward<F>(f)();
}

template<typename F0, typename F1, typename...Fs>
void InvokeAll(F0 &&f0, F1 &&f1, Fs &&...fs)
    noexcept(noexcept(std::forward<F0>(f0)()) &&
             noexcept(InvokeAll(std::forward<F1>(f1), std::forward<Fs>(fs)...)))
{
    std::forward<F0>(f0)();
    InvokeAll(std::forward<F1>(f1), std::forward<Fs>(fs)...);
}

// IMPROVE: use LUT
inline bool isTypeless(DXGI_FORMAT format) noexcept
{
    static const DXGI_FORMAT TYPELESS_FMTS[] =
    {
        DXGI_FORMAT_R32G32B32A32_TYPELESS,
        DXGI_FORMAT_R32G32B32_TYPELESS,
        DXGI_FORMAT_R16G16B16A16_TYPELESS,
        DXGI_FORMAT_R32G32_TYPELESS,
        DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS,
        DXGI_FORMAT_R10G10B10A2_TYPELESS,
        DXGI_FORMAT_R8G8B8A8_TYPELESS,
        DXGI_FORMAT_R16G16_TYPELESS,
        DXGI_FORMAT_R32_TYPELESS,
        DXGI_FORMAT_R24G8_TYPELESS,
        DXGI_FORMAT_R24_UNORM_X8_TYPELESS,
        DXGI_FORMAT_X24_TYPELESS_G8_UINT,
        DXGI_FORMAT_R8G8_TYPELESS,
        DXGI_FORMAT_R16_TYPELESS,
        DXGI_FORMAT_R8_TYPELESS,
        DXGI_FORMAT_BC1_TYPELESS,
        DXGI_FORMAT_BC2_TYPELESS,
        DXGI_FORMAT_BC3_TYPELESS,
        DXGI_FORMAT_BC4_TYPELESS,
        DXGI_FORMAT_BC5_TYPELESS,
        DXGI_FORMAT_B8G8R8A8_TYPELESS,
        DXGI_FORMAT_B8G8R8X8_TYPELESS,
        DXGI_FORMAT_BC6H_TYPELESS,
        DXGI_FORMAT_BC7_TYPELESS
    };

    for(auto f : TYPELESS_FMTS)
    {
        if(f == format)
            return true;
    }

    return false;
}

AGZ_D3D12_FG_END
