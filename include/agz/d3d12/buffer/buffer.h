#pragma once

#include <d3d12.h>
#include <d3dx12.h>

#include <agz/d3d12/common.h>

AGZ_D3D12_BEGIN

class Buffer : public misc::uncopyable_t
{
protected:

    ComPtr<ID3D12Resource> buffer_;
    size_t byteSize_;

    void *mappedData_;

public:

    Buffer();

    Buffer(Buffer &&other) noexcept;

    Buffer &operator=(Buffer &&other) noexcept;
    
    [[nodiscard]] ComPtr<ID3D12Resource> initializeStatic(
        ID3D12Device              *device,
        ID3D12GraphicsCommandList *copyCmdList,
        size_t                     byteSize,
        const void                *initData,
        size_t                     initDataByteSize,
        D3D12_RESOURCE_STATES      initStates);

    void initializeDynamic(
        ID3D12Device              *device,
        size_t                     byteSize,
        const void                *initData,
        size_t                     initDataByteSize);

    bool isAvailable() const noexcept;

    void destroy();

    ID3D12Resource *getResource() const noexcept;

    size_t getTotalByteSize() const noexcept;

    void swap(Buffer &other) noexcept;

    void updateContentData(size_t offset, size_t byteSize, const void *data);
};

inline Buffer::Buffer()
    : byteSize_(0), mappedData_(nullptr)
{

}

inline Buffer::Buffer(Buffer &&other) noexcept
    : Buffer()
{
    this->swap(other);
}

inline Buffer &Buffer::operator=(Buffer &&other) noexcept
{
    this->swap(other);
    return *this;
}

[[nodiscard]] inline ComPtr<ID3D12Resource> Buffer::initializeStatic(
    ID3D12Device              *device,
    ID3D12GraphicsCommandList *copyCmdList,
    size_t                     byteSize,
    const void                *initData,
    size_t                     initDataByteSize,
    D3D12_RESOURCE_STATES      initStates)
{
    destroy();
    misc::scope_guard_t destroyGuard([&]{ destroy(); });

    byteSize_ = byteSize;

    const CD3DX12_HEAP_PROPERTIES vtxBufProp(D3D12_HEAP_TYPE_DEFAULT);
    const auto vtxBufDesc = CD3DX12_RESOURCE_DESC::Buffer(byteSize);

    AGZ_D3D12_CHECK_HR(
        device->CreateCommittedResource(
            &vtxBufProp,
            D3D12_HEAP_FLAG_NONE,
            &vtxBufDesc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(buffer_.GetAddressOf())));

    const CD3DX12_HEAP_PROPERTIES uploadBufProp(D3D12_HEAP_TYPE_UPLOAD);
    const auto uploadBufDesc = CD3DX12_RESOURCE_DESC::Buffer(byteSize);

    ComPtr<ID3D12Resource> uploadBuf;
    AGZ_D3D12_CHECK_HR(
        device->CreateCommittedResource(
            &uploadBufProp,
            D3D12_HEAP_FLAG_NONE,
            &uploadBufDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(uploadBuf.GetAddressOf())));
    
    D3D12_SUBRESOURCE_DATA subrscData;
    subrscData.pData      = initData;
    subrscData.RowPitch   = initDataByteSize;
    subrscData.SlicePitch = initDataByteSize;

    UpdateSubresources(
        copyCmdList, buffer_.Get(), uploadBuf.Get(),
        0, 0, 1, &subrscData);

    const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        buffer_.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        initStates);

    copyCmdList->ResourceBarrier(1, &barrier);

    destroyGuard.dismiss();
    return uploadBuf;
}

inline void Buffer::initializeDynamic(
    ID3D12Device              *device,
    size_t                     byteSize,
    const void                *initData,
    size_t                     initDataByteSize)
{
    destroy();
    misc::scope_guard_t destroyGuard([&] { destroy(); });

    byteSize_ = byteSize;

    const CD3DX12_HEAP_PROPERTIES vtxBufProp(D3D12_HEAP_TYPE_UPLOAD);
    const auto vtxBufDesc = CD3DX12_RESOURCE_DESC::Buffer(byteSize);

    AGZ_D3D12_CHECK_HR(
        device->CreateCommittedResource(
            &vtxBufProp,
            D3D12_HEAP_FLAG_NONE,
            &vtxBufDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(buffer_.GetAddressOf())));

    D3D12_RANGE readRange = { 0, 0 };
    AGZ_D3D12_CHECK_HR(
        buffer_->Map(0, &readRange, &mappedData_));

    if(initData)
        std::memcpy(mappedData_, initData, initDataByteSize);

    destroyGuard.dismiss();
}

inline bool Buffer::isAvailable() const noexcept
{
    return buffer_ != nullptr;
}

inline void Buffer::destroy()
{
    buffer_.Reset();
    byteSize_    = 0;
    mappedData_  = nullptr;
}

inline ID3D12Resource *Buffer::getResource() const noexcept
{
    return buffer_.Get();
}

inline size_t Buffer::getTotalByteSize() const noexcept
{
    return byteSize_;
}

inline void Buffer::swap(Buffer &other) noexcept
{
    std::swap(buffer_,       other.buffer_);
    std::swap(byteSize_,     other.byteSize_);
    std::swap(mappedData_,   other.mappedData_);
}

inline void Buffer::updateContentData(
    size_t offset, size_t byteSize, const void *data)
{
    std::memcpy(static_cast<char*>(mappedData_) + offset, data, byteSize);
}

AGZ_D3D12_END
