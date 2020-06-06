#pragma once

#include <d3d12.h>
#include <d3dx12.h>

#include <agz/d3d12/common.h>

AGZ_D3D12_BEGIN

class RawDescriptorHeap : public misc::uncopyable_t
{
public:

    RawDescriptorHeap();

    RawDescriptorHeap(
        ID3D12Device              *device,
        int                        size,
        D3D12_DESCRIPTOR_HEAP_TYPE type,
        bool                       shaderVisible);

    RawDescriptorHeap(RawDescriptorHeap &&other) noexcept;

    RawDescriptorHeap &operator=(RawDescriptorHeap &&other) noexcept;

    bool isAvailable() const noexcept;

    ID3D12DescriptorHeap *getHeap() const noexcept;

    UINT getDescIncSize() const noexcept;

    CD3DX12_CPU_DESCRIPTOR_HANDLE getCPUHandle(int index) const noexcept;

    CD3DX12_GPU_DESCRIPTOR_HANDLE getGPUHandle(int index) const noexcept;

    void swap(RawDescriptorHeap &other) noexcept;

private:

    ComPtr<ID3D12DescriptorHeap> heap_;
    UINT descIncSize_;
    CD3DX12_CPU_DESCRIPTOR_HANDLE cpuStart_;
    CD3DX12_GPU_DESCRIPTOR_HANDLE gpuStart_;
};

inline RawDescriptorHeap::RawDescriptorHeap()
    : descIncSize_(0), cpuStart_{}, gpuStart_{}
{

}

inline RawDescriptorHeap::RawDescriptorHeap(
    ID3D12Device              *device,
    int                        size,
    D3D12_DESCRIPTOR_HEAP_TYPE type,
    bool                       shaderVisible)
    : descIncSize_(0), cpuStart_{}, gpuStart_{}
{
    D3D12_DESCRIPTOR_HEAP_DESC desc;
    desc.NodeMask       = 0;
    desc.Flags          = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
                                        : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    desc.NumDescriptors = UINT(size);
    desc.Type           = type;

    AGZ_D3D12_CHECK_HR(
        device->CreateDescriptorHeap(
            &desc, IID_PPV_ARGS(heap_.GetAddressOf())));

    descIncSize_ = device->GetDescriptorHandleIncrementSize(type);

    cpuStart_ = CD3DX12_CPU_DESCRIPTOR_HANDLE(
        heap_->GetCPUDescriptorHandleForHeapStart());

    if(shaderVisible)
    {
        gpuStart_ = CD3DX12_GPU_DESCRIPTOR_HANDLE(
            heap_->GetGPUDescriptorHandleForHeapStart());
    }
    else
        gpuStart_.ptr = 0;
}

inline RawDescriptorHeap::RawDescriptorHeap(RawDescriptorHeap &&other) noexcept
    : descIncSize_(0), cpuStart_{}, gpuStart_{}
{
    swap(other);
}

inline RawDescriptorHeap &RawDescriptorHeap::operator=(
    RawDescriptorHeap &&other) noexcept
{
    heap_.Reset();
    descIncSize_  = 0;
    cpuStart_.ptr = 0;
    gpuStart_.ptr = 0;
    swap(other);
    return *this;
}

inline bool RawDescriptorHeap::isAvailable() const noexcept
{
    return heap_ != nullptr;
}

inline ID3D12DescriptorHeap *RawDescriptorHeap::getHeap() const noexcept
{
    return heap_.Get();
}

inline UINT RawDescriptorHeap::getDescIncSize() const noexcept
{
    return descIncSize_;
}

inline CD3DX12_CPU_DESCRIPTOR_HANDLE RawDescriptorHeap::getCPUHandle(
    int index) const noexcept
{
    auto ret = cpuStart_;
    return ret.Offset(index, descIncSize_);
}

inline CD3DX12_GPU_DESCRIPTOR_HANDLE RawDescriptorHeap::getGPUHandle(
    int index) const noexcept
{
    if(!gpuStart_.ptr)
        return CD3DX12_GPU_DESCRIPTOR_HANDLE(D3D12_DEFAULT);
    auto ret = gpuStart_;
    return ret.Offset(index, descIncSize_);
}

inline void RawDescriptorHeap::swap(RawDescriptorHeap &other) noexcept
{
    std::swap(heap_,        other.heap_);
    std::swap(descIncSize_, other.descIncSize_);
    std::swap(cpuStart_,    other.cpuStart_);
    std::swap(gpuStart_,    other.gpuStart_);
}

AGZ_D3D12_END
