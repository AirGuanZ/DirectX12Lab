#pragma once

#include <d3d12.h>
#include <d3dx12.h>

#include <agz/d3d12/common.h>

AGZ_D3D12_LAB_BEGIN

template<typename Vertex>
class VertexBuffer : public misc::uncopyable_t
{
    ComPtr<ID3D12Resource> vertexBuffer_;

    int vertexCount_;

public:

    VertexBuffer();
    
    ComPtr<ID3D12Resource> initializeStatic(
        ID3D12Device              *device,
        ID3D12GraphicsCommandList *copyCmdList,
        int                        count,
        const Vertex              *initData);

    void initializeDynamic(
        ID3D12Device *device,
        int           count,
        const Vertex *initData);

    bool isAvailable() const noexcept;

    void destroy();

    ID3D12Resource *get() const noexcept;

    D3D12_VERTEX_BUFFER_VIEW getView() const noexcept;
};

template<typename Vertex>
VertexBuffer<Vertex>::VertexBuffer()
    : vertexCount_(0)
{

}

template<typename Vertex>
[[nodiscard]] ComPtr<ID3D12Resource> VertexBuffer<Vertex>::initializeStatic(
    ID3D12Device              *device,
    ID3D12GraphicsCommandList *copyCmdList,
    int                        count,
    const Vertex              *initData)
{
    vertexCount_ = count;

    const size_t byteSize = sizeof(Vertex) * count;

    const CD3DX12_HEAP_PROPERTIES vtxBufProp(D3D12_HEAP_TYPE_DEFAULT);
    const auto vtxBufDesc = CD3DX12_RESOURCE_DESC::Buffer(byteSize);

    AGZ_D3D12_CHECK_HR(
        device->CreateCommittedResource(
            &vtxBufProp,
            D3D12_HEAP_FLAG_NONE,
            &vtxBufDesc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(vertexBuffer_.GetAddressOf())));

    vertexBuffer_->SetName(L"Vertex buffer");

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
    uploadBuf->SetName(L"Upload resource heap for vertex buffer");

    D3D12_SUBRESOURCE_DATA subrscData;
    subrscData.pData      = initData;
    subrscData.RowPitch   = byteSize;
    subrscData.SlicePitch = byteSize;

    UpdateSubresources(
        copyCmdList, vertexBuffer_.Get(), uploadBuf.Get(),
        0, 0, 1, &subrscData);

    const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        vertexBuffer_.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

    copyCmdList->ResourceBarrier(1, &barrier);

    return uploadBuf;
}

template<typename Vertex>
void VertexBuffer<Vertex>::initializeDynamic(
    ID3D12Device      *device,
    int                count,
    const Vertex      *initData)
{
    // TODO
}

template<typename Vertex>
bool VertexBuffer<Vertex>::isAvailable() const noexcept
{
    return vertexBuffer_;
}

template<typename Vertex>
void VertexBuffer<Vertex>::destroy()
{
    vertexBuffer_.Reset();
    vertexCount_ = 0;
}

template<typename Vertex>
ID3D12Resource *VertexBuffer<Vertex>::get() const noexcept
{
    return vertexBuffer_.Get();
}

template<typename Vertex>
D3D12_VERTEX_BUFFER_VIEW VertexBuffer<Vertex>::getView() const noexcept
{
    D3D12_VERTEX_BUFFER_VIEW view;
    view.BufferLocation = vertexBuffer_->GetGPUVirtualAddress();
    view.SizeInBytes    = static_cast<UINT>(sizeof(Vertex) * vertexCount_);
    view.StrideInBytes  = sizeof(Vertex);
    return view;
}

AGZ_D3D12_LAB_END
