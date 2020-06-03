#pragma once

#include <d3d12.h>
#include <d3dx12.h>

#include <agz/d3d12/common.h>

AGZ_D3D12_BEGIN

class DescriptorHeap : public misc::uncopyable_t
{
public:

    DescriptorHeap();

    DescriptorHeap(
        ID3D12Device              *device,
        int                        size,
        D3D12_DESCRIPTOR_HEAP_TYPE type,
        bool                       shaderVisible);

    DescriptorHeap(DescriptorHeap &&other) noexcept;

    DescriptorHeap &operator=(DescriptorHeap &&other) noexcept;

    bool isAvailable() const noexcept;

    ID3D12DescriptorHeap *getHeap() const noexcept;

    UINT getDescIncSize() const noexcept;

    CD3DX12_CPU_DESCRIPTOR_HANDLE getCPUHandle(int index) const noexcept;

    CD3DX12_GPU_DESCRIPTOR_HANDLE getGPUHandle(int index) const noexcept;

    void swap(DescriptorHeap &other) noexcept;

private:

    ComPtr<ID3D12DescriptorHeap> heap_;
    UINT descIncSize_;
    CD3DX12_CPU_DESCRIPTOR_HANDLE cpuStart_;
    CD3DX12_GPU_DESCRIPTOR_HANDLE gpuStart_;
};

inline DescriptorHeap::DescriptorHeap()
    : descIncSize_(0), cpuStart_{}, gpuStart_{}
{

}

inline DescriptorHeap::DescriptorHeap(
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

    gpuStart_ = CD3DX12_GPU_DESCRIPTOR_HANDLE(
        heap_->GetGPUDescriptorHandleForHeapStart());
}

inline DescriptorHeap::DescriptorHeap(DescriptorHeap &&other) noexcept
    : descIncSize_(0), cpuStart_{}, gpuStart_{}
{
    swap(other);
}

inline DescriptorHeap &DescriptorHeap::operator=(
    DescriptorHeap &&other) noexcept
{
    heap_.Reset();
    descIncSize_  = 0;
    cpuStart_.ptr = 0;
    gpuStart_.ptr = 0;
    swap(other);
    return *this;
}

inline bool DescriptorHeap::isAvailable() const noexcept
{
    return heap_ != nullptr;
}

inline ID3D12DescriptorHeap *DescriptorHeap::getHeap() const noexcept
{
    return heap_.Get();
}

inline UINT DescriptorHeap::getDescIncSize() const noexcept
{
    return descIncSize_;
}

inline CD3DX12_CPU_DESCRIPTOR_HANDLE DescriptorHeap::getCPUHandle(
    int index) const noexcept
{
    auto ret = cpuStart_;
    return ret.Offset(index, descIncSize_);
}

inline CD3DX12_GPU_DESCRIPTOR_HANDLE DescriptorHeap::getGPUHandle(
    int index) const noexcept
{
    auto ret = gpuStart_;
    return ret.Offset(index, descIncSize_);
}

inline void DescriptorHeap::swap(DescriptorHeap &other) noexcept
{
    std::swap(heap_,        other.heap_);
    std::swap(descIncSize_, other.descIncSize_);
    std::swap(cpuStart_,    other.cpuStart_);
    std::swap(gpuStart_,    other.gpuStart_);
}

AGZ_D3D12_END
