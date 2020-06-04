#include <d3dx12.h>

#include <agz/d3d12/texture/texture2d.h>

AGZ_D3D12_BEGIN

LoadTextureResult loadTexture2DFromMemory(
    ID3D12Device                              *device,
    ID3D12GraphicsCommandList                 *cmdList,
    const texture::texture2d_t<math::color4b> &data)
{
    // resource

    const auto rscDesc = CD3DX12_RESOURCE_DESC::Tex2D(
        DXGI_FORMAT_R8G8B8A8_UNORM,
        UINT64(data.width()),
        UINT64(data.height()));

    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);

    ComPtr<ID3D12Resource> rsc;
    AGZ_D3D12_CHECK_HR(
        device->CreateCommittedResource(
            &heapProps, D3D12_HEAP_FLAG_NONE, &rscDesc,
            D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
            IID_PPV_ARGS(rsc.GetAddressOf())));

    // compute upload heap size

    const size_t texelSize     = sizeof(rm_rcv_t<decltype(data)>::texel_t);
    const size_t rawRowSize    = data.width() * texelSize;
    const size_t paddedRowSize = (rawRowSize + 255) & ~255;
    const size_t uploadSize    = paddedRowSize * (data.height() - 1) + rawRowSize;

    const auto uploadHeapDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadSize);
    
    CD3DX12_HEAP_PROPERTIES uploadHeapProps(D3D12_HEAP_TYPE_UPLOAD);

    ComPtr<ID3D12Resource> uploadHeap;
    AGZ_D3D12_CHECK_HR(
        device->CreateCommittedResource(
            &uploadHeapProps, D3D12_HEAP_FLAG_NONE, &uploadHeapDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(uploadHeap.GetAddressOf())));

    // upload data

    D3D12_SUBRESOURCE_DATA subrscData;
    subrscData.pData      = data.raw_data();
    subrscData.RowPitch   = rawRowSize;
    subrscData.SlicePitch = rawRowSize * data.height();

    UpdateSubresources(
        cmdList, rsc.Get(), uploadHeap.Get(), 0, 0, 1, &subrscData);

    const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        rsc.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    cmdList->ResourceBarrier(1, &barrier);

    return { std::move(rsc), std::move(uploadHeap) };
}

AGZ_D3D12_END
