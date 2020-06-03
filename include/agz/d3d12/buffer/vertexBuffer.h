#pragma once

#include <agz/d3d12/buffer/buffer.h>

AGZ_D3D12_BEGIN

class RawVertexBuffer : public misc::uncopyable_t
{
    Buffer buffer_;

    size_t vertexSize_  = 0;
    size_t vertexCount_ = 0;

public:

    RawVertexBuffer() = default;

    RawVertexBuffer(RawVertexBuffer &&other) noexcept;

    RawVertexBuffer &operator=(RawVertexBuffer &&other) noexcept;

    void swap(RawVertexBuffer &other) noexcept;

    bool isAvailable() const noexcept;

    void destroy();
    
    ComPtr<ID3D12Resource> initializeStatic(
        ID3D12Device              *device,
        ID3D12GraphicsCommandList *copyCmdList,
        size_t                     vertexCount,
        size_t                     vertexSize,
        const void                *initData);
    
    void initializeDynamic(
        ID3D12Device              *device,
        size_t                     vertexCount,
        size_t                     vertexSize,
        const void                *initData);

    ID3D12Resource *getResource() const noexcept;

    size_t getTotalByteSize() const noexcept;

    size_t getVertexByteSize() const noexcept;

    size_t getVertexCount() const noexcept;

    D3D12_VERTEX_BUFFER_VIEW getView() const noexcept;
};

template<typename Vertex>
class VertexBuffer : public misc::uncopyable_t
{
    RawVertexBuffer buffer_;

public:

    VertexBuffer() = default;

    VertexBuffer(VertexBuffer<Vertex> &&other) noexcept = default;

    VertexBuffer<Vertex> &operator=(
        VertexBuffer<Vertex> &&other) noexcept = default;

    void swap(VertexBuffer<Vertex> &&other);

    bool isAvailable() const noexcept;

    void destroy();
    
    ComPtr<ID3D12Resource> initializeStatic(
        ID3D12Device              *device,
        ID3D12GraphicsCommandList *copyCmdList,
        size_t                     vertexCount,
        const Vertex              *initData);
    
    void initializeDynamic(
        ID3D12Device              *device,
        size_t                     vertexCount,
        const Vertex              *initData);

    ID3D12Resource *getResource() const noexcept;

    size_t getTotalByteSize() const noexcept;

    size_t getVertexCount() const noexcept;

    D3D12_VERTEX_BUFFER_VIEW getView() const noexcept;

    operator RawVertexBuffer &() noexcept;

    operator const RawVertexBuffer &() const noexcept;
};

inline RawVertexBuffer::RawVertexBuffer(RawVertexBuffer &&other) noexcept
    : RawVertexBuffer()
{
    swap(other);
}

inline RawVertexBuffer &RawVertexBuffer::operator=(
    RawVertexBuffer &&other) noexcept
{
    swap(other);
    return *this;
}

inline void RawVertexBuffer::swap(RawVertexBuffer &other) noexcept
{
    buffer_.swap(other.buffer_);
    std::swap(vertexSize_,  other.vertexSize_);
    std::swap(vertexCount_, other.vertexCount_);
}

inline bool RawVertexBuffer::isAvailable() const noexcept
{
    return buffer_.isAvailable();
}

inline void RawVertexBuffer::destroy()
{
    buffer_.destroy();
    vertexSize_  = 0;
    vertexCount_ = 0;
}

inline ComPtr<ID3D12Resource> RawVertexBuffer::initializeStatic(
    ID3D12Device              *device,
    ID3D12GraphicsCommandList *copyCmdList,
    size_t                     vertexCount,
    size_t                     vertexSize,
    const void                *initData)
{
    const size_t byteSize = vertexCount * vertexSize;
    
    destroy();

    auto ret = buffer_.initializeStatic(
        device, copyCmdList, byteSize, initData, byteSize,
        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

    vertexSize_  = vertexSize;
    vertexCount_ = vertexCount;

    return ret;
}

inline void RawVertexBuffer::initializeDynamic(
    ID3D12Device  *device,
    size_t         vertexCount,
    size_t         vertexSize,
    const void    *initData)
{
    const size_t byteSize = vertexCount * vertexSize;

    destroy();

    buffer_.initializeDynamic(
        device, byteSize, initData, byteSize);

    vertexSize_  = vertexSize;
    vertexCount_ = vertexCount;
}

inline ID3D12Resource *RawVertexBuffer::getResource() const noexcept
{
    return buffer_.getResource();
}

inline size_t RawVertexBuffer::getTotalByteSize() const noexcept
{
    return buffer_.getTotalByteSize();
}

inline size_t RawVertexBuffer::getVertexByteSize() const noexcept
{
    return vertexSize_;
}

inline size_t RawVertexBuffer::getVertexCount() const noexcept
{
    return vertexCount_;
}

inline D3D12_VERTEX_BUFFER_VIEW RawVertexBuffer::getView() const noexcept
{
    D3D12_VERTEX_BUFFER_VIEW view;
    view.BufferLocation = buffer_.getResource()->GetGPUVirtualAddress();
    view.SizeInBytes    = UINT(buffer_.getTotalByteSize());
    view.StrideInBytes  = UINT(vertexSize_);
    return view;
}

template<typename Vertex>
void VertexBuffer<Vertex>::swap(VertexBuffer<Vertex> &&other)
{
    buffer_.swap(other.buffer_);
}

template<typename Vertex>
bool VertexBuffer<Vertex>::isAvailable() const noexcept
{
    return buffer_.isAvailable();
}

template<typename Vertex>
void VertexBuffer<Vertex>::destroy()
{
    buffer_.destroy();
}

template<typename Vertex>
ComPtr<ID3D12Resource> VertexBuffer<Vertex>::initializeStatic(
        ID3D12Device              *device,
        ID3D12GraphicsCommandList *copyCmdList,
        size_t                     vertexCount,
        const Vertex              *initData)
{
    return buffer_.initializeStatic(
        device, copyCmdList, vertexCount, sizeof(Vertex), initData);
}

template<typename Vertex>
void VertexBuffer<Vertex>::initializeDynamic(
    ID3D12Device *device,
    size_t        vertexCount,
    const Vertex *initData)
{
    buffer_.initializeDynamic(device, vertexCount, initData);
}

template<typename Vertex>
ID3D12Resource *VertexBuffer<Vertex>::getResource() const noexcept
{
    return buffer_.getResource();
}

template<typename Vertex>
size_t VertexBuffer<Vertex>::getTotalByteSize() const noexcept
{
    return buffer_.getTotalByteSize();
}

template<typename Vertex>
size_t VertexBuffer<Vertex>::getVertexCount() const noexcept
{
    return buffer_.getVertexCount();
}

template<typename Vertex>
D3D12_VERTEX_BUFFER_VIEW VertexBuffer<Vertex>::getView() const noexcept
{
    return buffer_.getView();
}

template<typename Vertex>
VertexBuffer<Vertex>::operator RawVertexBuffer&() noexcept
{
    return buffer_;
}

template<typename Vertex>
VertexBuffer<Vertex>::operator const RawVertexBuffer&() const noexcept
{
    return buffer_;
}

AGZ_D3D12_END
