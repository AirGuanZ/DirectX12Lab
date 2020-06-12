#pragma once

#include <agz/d3d12/common.h>

#define AGZ_D3D12_FG_BEGIN namespace agz::d3d12::fg {
#define AGZ_D3D12_FG_END   }

AGZ_D3D12_FG_BEGIN

struct ResourceIndex
{
    int32_t idx = -1;

    bool isNil() const noexcept { return idx < 0; }
};

struct TaskIndex
{
    int32_t idx = -1;

    bool isNil() const noexcept { return idx < 0; }
};

constexpr ResourceIndex RESOURCE_NIL = { -1 };

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

AGZ_D3D12_FG_END
