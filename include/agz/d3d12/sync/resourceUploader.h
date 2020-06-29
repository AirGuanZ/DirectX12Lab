#pragma once

#include <d3d12.h>

#include <agz/d3d12/buffer/buffer.h>
#include <agz/d3d12/cmd/singleCmdList.h>

AGZ_D3D12_BEGIN

class ResourceUploader : public misc::uncopyable_t
{
public:

    struct Tex2DSubInitData
    {
        Tex2DSubInitData(
            const void *data = nullptr, size_t rowSize = 0) noexcept
            : data(data), rowSize(rowSize)
        {

        }

        const void *data;
        size_t rowSize;
    };

    struct Tex2DInitData
    {
        const Tex2DSubInitData *subrscInitData = nullptr;
    };

    ResourceUploader(
        ComPtr<ID3D12Device>       device,
        ComPtr<ID3D12CommandQueue> copyQueue,
        ComPtr<ID3D12CommandQueue> graphicsQueue,
        size_t                     ringCmdListCount);

    ~ResourceUploader();

    void uploadBufferData(
        ComPtr<ID3D12Resource> dst,
        const void            *data,
        size_t                 byteSize,
        D3D12_RESOURCE_STATES  afterState);

    void uploadBufferData(
        Buffer               &buffer,
        const void           *data,
        size_t                byteSize,
        D3D12_RESOURCE_STATES afterState);

    void uploadTex2DData(
        ComPtr<ID3D12Resource>  dst,
        const Tex2DSubInitData &initData,
        D3D12_RESOURCE_STATES   afterState);

    void uploadTex2DData(
        ComPtr<ID3D12Resource> dst,
        const Tex2DInitData   &initData,
        D3D12_RESOURCE_STATES  afterState);

    void submit();

    void collect();

    void waitForIdle();

private:

    ComPtr<ID3D12Device>       device_;
    ComPtr<ID3D12CommandQueue> copyQueue_;
    ComPtr<ID3D12CommandQueue> graphicsQueue_;

    ComPtr<ID3D12Fence> copyToGraphicsFence_;
    ComPtr<ID3D12Fence> finishFence_;

    UINT64 nextExpectedCopyToGraphicsFenceValue_;
    UINT64 nextExpectedFinishFenceValue_;

    struct RingCmdList
    {
        UINT64 expectedFenceValue = 0;
        SingleCommandList cmdList;
    };

    std::vector<RingCmdList> copyCmdLists_;
    std::vector<RingCmdList> graphicsCmdLists_;
    size_t curCmdListIdx_;

    bool isCurCmdListDirty_;
    bool isCurGraphicsCmdListDirty_;

    struct UploadingRsc
    {
        UINT64 expectedFenceValue = 0;
        ComPtr<ID3D12Resource> upload;
        ComPtr<ID3D12Resource> rsc;
    };

    std::vector<UploadingRsc> uploadingRscs_;
};

AGZ_D3D12_END
