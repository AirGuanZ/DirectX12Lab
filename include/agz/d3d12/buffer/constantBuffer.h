#pragma once

#include <agz/d3d12/buffer/buffer.h>

AGZ_D3D12_LAB_BEGIN

class RawConstantBuffer : public misc::uncopyable_t
{
    Buffer buffer_;

    size_t unpaddedSize_ = 0;
    size_t paddedize_    = 0;
    size_t count_        = 0;

    static size_t padTo256(size_t unpadded) noexcept;

public:

    RawConstantBuffer() = default;

    RawConstantBuffer(RawConstantBuffer &&other) noexcept;

    RawConstantBuffer &operator=(RawConstantBuffer &&other) noexcept;

    void swap(RawConstantBuffer &other) noexcept;

    bool isAvailable() const noexcept;

    void destroy();

    void initializeDynamic(
        ID3D12Device *device,
        size_t        cbCount,
        size_t        cbSize);

    ID3D12Resource *getResource() const noexcept;

    D3D12_GPU_VIRTUAL_ADDRESS getGPUVirtualAddress(size_t index) const noexcept;

    size_t getConstantBufferCount() const noexcept;

    D3D12_CONSTANT_BUFFER_VIEW_DESC getViewDesc(size_t index) const noexcept;

    void updateContentData(size_t index, const void *data);
};

template<typename Struct>
class ConstantBuffer : public misc::uncopyable_t
{
    RawConstantBuffer buffer_;

public:

    ConstantBuffer() = default;

    ConstantBuffer(ConstantBuffer<Struct> &&other) noexcept = default;

    ConstantBuffer<Struct> &operator=(
        ConstantBuffer<Struct> &&other) noexcept = default;

    void swap(ConstantBuffer<Struct> &other) noexcept;

    bool isAvailable() const noexcept;

    void destroy();
    
    void initializeDynamic(
        ID3D12Device *device,
        size_t        cbCount = 1);
    
    ID3D12Resource *getResource() const noexcept;

    D3D12_GPU_VIRTUAL_ADDRESS getGpuVirtualAddress(size_t index) const noexcept;

    size_t getConstantBufferCount() const noexcept;

    D3D12_CONSTANT_BUFFER_VIEW_DESC getViewDesc(size_t index) const noexcept;

    void updateContentData(size_t index, const Struct &data);

    operator RawConstantBuffer &() noexcept;

    operator const RawConstantBuffer &() const noexcept;
};

inline size_t RawConstantBuffer::padTo256(size_t unpadded) noexcept
{
    return (unpadded + 255) & ~255;
}

inline RawConstantBuffer::RawConstantBuffer(RawConstantBuffer &&other) noexcept
    : RawConstantBuffer()
{
    swap(other);
}

inline RawConstantBuffer &RawConstantBuffer::operator=(
    RawConstantBuffer &&other) noexcept
{
    swap(other);
    return *this;
}

inline void RawConstantBuffer::swap(RawConstantBuffer &other) noexcept
{
    buffer_.swap(other.buffer_);
    std::swap(unpaddedSize_, other.unpaddedSize_);
    std::swap(paddedize_,    other.paddedize_);
    std::swap(count_,        other.count_);
}

inline bool RawConstantBuffer::isAvailable() const noexcept
{
    return buffer_.isAvailable();
}

inline void RawConstantBuffer::destroy()
{
    buffer_.destroy();
    unpaddedSize_ = 0;
    paddedize_ = 0;
    count_ = 0;
}

inline void RawConstantBuffer::initializeDynamic(
    ID3D12Device *device,
    size_t        cbCount,
    size_t        cbSize)
{
    destroy();

    const size_t paddedSize = padTo256(cbSize);

    const size_t byteSize = paddedSize * cbCount;

    buffer_.initializeDynamic(device, byteSize, nullptr, 0);

    unpaddedSize_ = cbSize;
    paddedize_ = paddedSize;
    count_ = cbCount;
}

inline ID3D12Resource *RawConstantBuffer::getResource() const noexcept
{
    return buffer_.getResource();
}

inline D3D12_GPU_VIRTUAL_ADDRESS RawConstantBuffer::getGPUVirtualAddress(
    size_t index) const noexcept
{
    return buffer_.getResource()->GetGPUVirtualAddress() + paddedize_ * index;
}

inline size_t RawConstantBuffer::getConstantBufferCount() const noexcept
{
    return count_;
}

inline D3D12_CONSTANT_BUFFER_VIEW_DESC RawConstantBuffer::getViewDesc(
    size_t index) const noexcept
{
    const auto start = buffer_.getResource()->GetGPUVirtualAddress();

    D3D12_CONSTANT_BUFFER_VIEW_DESC desc;
    desc.BufferLocation = start + paddedize_ * index;
    desc.SizeInBytes    = UINT(paddedize_);
    return desc;
}

inline void RawConstantBuffer::updateContentData(size_t index, const void *data)
{
    const size_t offset = paddedize_ * index;
    buffer_.updateContentData(offset, unpaddedSize_, data);
}

template<typename Struct>
void ConstantBuffer<Struct>::swap(ConstantBuffer<Struct> &other) noexcept
{
    buffer_.swap(other);
}

template<typename Struct>
bool ConstantBuffer<Struct>::isAvailable() const noexcept
{
    return buffer_.isAvailable();
}

template<typename Struct>
void ConstantBuffer<Struct>::destroy()
{
    buffer_.destroy();
}

template<typename Struct>
void ConstantBuffer<Struct>::initializeDynamic(
    ID3D12Device *device,
    size_t        cbCount)
{
    buffer_.initializeDynamic(device, cbCount, sizeof(Struct));
}

template<typename Struct>
ID3D12Resource *ConstantBuffer<Struct>::getResource() const noexcept
{
    return buffer_.getResource();
}

template<typename Struct>
D3D12_GPU_VIRTUAL_ADDRESS ConstantBuffer<Struct>::getGpuVirtualAddress(
    size_t index) const noexcept
{
    return buffer_.getGPUVirtualAddress(index);
}

template<typename Struct>
size_t ConstantBuffer<Struct>::getConstantBufferCount() const noexcept
{
    return buffer_.getConstantBufferCount();
}

template<typename Struct>
D3D12_CONSTANT_BUFFER_VIEW_DESC ConstantBuffer<Struct>::getViewDesc(
    size_t index) const noexcept
{
    return buffer_.getViewDesc(index);
}

template<typename Struct>
void ConstantBuffer<Struct>::updateContentData(size_t index, const Struct &data)
{
    buffer_.updateContentData(index, &data);
}

template<typename Struct>
ConstantBuffer<Struct>::operator RawConstantBuffer&() noexcept
{
    return buffer_;
}

template<typename Struct>
ConstantBuffer<Struct>::operator const RawConstantBuffer&() const noexcept
{
    return buffer_;
}

AGZ_D3D12_LAB_END
