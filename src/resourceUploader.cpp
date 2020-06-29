#include <d3dx12.h>

#include <agz/d3d12/sync/resourceUploader.h>

AGZ_D3D12_BEGIN

namespace
{
    ComPtr<ID3D12CommandQueue> createCopyQueue(ID3D12Device *device)
    {
        D3D12_COMMAND_QUEUE_DESC copyQueueDesc = {};
        copyQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;

        ComPtr<ID3D12CommandQueue> copyQueue;
        AGZ_D3D12_CHECK_HR(
            device->CreateCommandQueue(
                &copyQueueDesc, IID_PPV_ARGS(copyQueue.GetAddressOf())));

        return copyQueue;
    }
}

ResourceUploader::ResourceUploader(
    ComPtr<ID3D12Device>       device,
    ComPtr<ID3D12CommandQueue> copyQueue,
    ComPtr<ID3D12CommandQueue> graphicsQueue,
    size_t                     ringCmdListCount)
    : device_(std::move(device)),
      copyQueue_(std::move(copyQueue)),
      graphicsQueue_(std::move(graphicsQueue)),
      nextExpectedCopyToGraphicsFenceValue_(1),
      nextExpectedFinishFenceValue_(1),
      curCmdListIdx_(0),
      isCurCmdListDirty_(false),
      isCurGraphicsCmdListDirty_(false)
{
    AGZ_D3D12_CHECK_HR(
        device_->CreateFence(
            0, D3D12_FENCE_FLAG_NONE,
            IID_PPV_ARGS(copyToGraphicsFence_.GetAddressOf())));

    AGZ_D3D12_CHECK_HR(
        device_->CreateFence(
            0, D3D12_FENCE_FLAG_NONE,
            IID_PPV_ARGS(finishFence_.GetAddressOf())));

    copyCmdLists_.resize(ringCmdListCount);
    graphicsCmdLists_.resize(ringCmdListCount);

    for(auto &c : copyCmdLists_)
    {
        c.expectedFenceValue = 0;
        c.cmdList.initialize(device_.Get(), D3D12_COMMAND_LIST_TYPE_COPY);
    }
    copyCmdLists_[0].cmdList.resetCommandList();

    for(auto &c : graphicsCmdLists_)
    {
        c.expectedFenceValue = 0;
        c.cmdList.initialize(device_.Get(), D3D12_COMMAND_LIST_TYPE_DIRECT);
    }
    graphicsCmdLists_[0].cmdList.resetCommandList();
}

ResourceUploader::ResourceUploader(
    Window &window,
    size_t  ringCmdListCount)
    : ResourceUploader(
        window.getDevice(),
        createCopyQueue(window.getDevice()),
        window.getCommandQueue(),
        ringCmdListCount)
{

}

ResourceUploader::~ResourceUploader()
{
    if(isCurCmdListDirty_)
        submit();
    waitForIdle();
}

void ResourceUploader::uploadBufferData(
    ComPtr<ID3D12Resource> dst,
    const void            *data,
    size_t                 byteSize,
    D3D12_RESOURCE_STATES  afterState)
{
    const auto uploadHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    const auto uploadDesc      = CD3DX12_RESOURCE_DESC::Buffer(byteSize);

    ComPtr<ID3D12Resource> uploadBuf;
    AGZ_D3D12_CHECK_HR(
        device_->CreateCommittedResource(
            &uploadHeapProps, D3D12_HEAP_FLAG_NONE, &uploadDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(uploadBuf.GetAddressOf())));

    auto &copyC     = copyCmdLists_[curCmdListIdx_];
    auto &graphicsC = graphicsCmdLists_[curCmdListIdx_];

    D3D12_SUBRESOURCE_DATA subrscData;
    subrscData.pData      = data;
    subrscData.RowPitch   = byteSize;
    subrscData.SlicePitch = byteSize;

    UpdateSubresources(
        copyC.cmdList, dst.Get(), uploadBuf.Get(), 0, 0, 1, &subrscData);

    if(afterState != D3D12_RESOURCE_STATE_COMMON)
    {
        graphicsC.cmdList->ResourceBarrier(
            1, get_temp_ptr(CD3DX12_RESOURCE_BARRIER::Transition(
                dst.Get(), D3D12_RESOURCE_STATE_COMMON, afterState)));

        isCurGraphicsCmdListDirty_ = true;
    }

    UploadingRsc rcd;
    rcd.expectedFenceValue = nextExpectedFinishFenceValue_;
    rcd.upload             = uploadBuf;
    rcd.rsc                = dst;

    uploadingRscs_.push_back(std::move(rcd));

    isCurCmdListDirty_ = true;
}

void ResourceUploader::uploadBufferData(
    Buffer               &buffer,
    const void           *data,
    size_t                byteSize,
    D3D12_RESOURCE_STATES afterState)
{
    uploadBufferData(buffer.getResource(), data, byteSize, afterState);
}

void ResourceUploader::uploadBufferData(
    Buffer                &buffer,
    const void           *data,
    D3D12_RESOURCE_STATES afterState)
{
    uploadBufferData(buffer, data, buffer.getTotalByteSize(), afterState);
}

void ResourceUploader::uploadTex2DData(
    ComPtr<ID3D12Resource>  dst,
    const Tex2DSubInitData &initData,
    D3D12_RESOURCE_STATES   afterState)
{
    uploadTex2DData(dst, Tex2DInitData{ &initData }, afterState);
}

void ResourceUploader::uploadTex2DData(
    ComPtr<ID3D12Resource> dst,
    const Tex2DInitData   &initData,
    D3D12_RESOURCE_STATES  afterState)
{
    const auto dstDesc = dst->GetDesc();

    // texel size

    UINT texelSize;
    switch(dstDesc.Format)
    {
    case DXGI_FORMAT_R8G8B8A8_UNORM:     texelSize = 4;  break;
    case DXGI_FORMAT_R32G32B32_FLOAT:    texelSize = 12; break;
    case DXGI_FORMAT_R32G32B32A32_FLOAT: texelSize = 16; break;
    default:
        throw D3D12LabException(
            "resource uploader: unsupported texel format");
    }

    // construct D3D12_SUBRESOURCE_DATAs

    const UINT arraySize   = dstDesc.DepthOrArraySize;
    const UINT mipmapCount = dstDesc.MipLevels;

    const UINT width  = static_cast<UINT>(dstDesc.Width);
    const UINT height = static_cast<UINT>(dstDesc.Height);

    std::vector<D3D12_SUBRESOURCE_DATA> allSubrscData;
    allSubrscData.reserve(arraySize * mipmapCount);

    for(UINT arrIdx = 0; arrIdx < arraySize; ++arrIdx)
    {
        for(UINT mipIdx = 0; mipIdx < mipmapCount; ++mipIdx)
        {
            const UINT subrscIdx = arrIdx * mipmapCount + mipIdx;
            const auto &iData = initData.subrscInitData[subrscIdx];

            const UINT mipWidth  = (std::max<UINT>)(1, width >> mipIdx);
            const UINT mipHeight = (std::max<UINT>)(1, height >> mipIdx);

            const UINT initDataRowSize = iData.rowSize ?
                                         static_cast<UINT>(iData.rowSize) :
                                         (texelSize * mipWidth);

            D3D12_SUBRESOURCE_DATA subrscData;
            subrscData.pData      = iData.data;
            subrscData.RowPitch   = initDataRowSize;
            subrscData.SlicePitch = initDataRowSize * mipHeight;

            allSubrscData.push_back(subrscData);
        }
    }

    // create upload heap

    ComPtr<ID3D12Resource> uploadBuf;

    const UINT64 uploadBufSize = GetRequiredIntermediateSize(
        dst.Get(), 0, arraySize * mipmapCount);

    AGZ_D3D12_CHECK_HR(
        device_->CreateCommittedResource(
            get_temp_ptr(CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD)),
            D3D12_HEAP_FLAG_NONE,
            get_temp_ptr(CD3DX12_RESOURCE_DESC::Buffer(uploadBufSize)),
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(uploadBuf.GetAddressOf())));

    // upload data

    UpdateSubresources(
        copyCmdLists_[curCmdListIdx_].cmdList, dst.Get(), uploadBuf.Get(),
        0, 0, arraySize * mipmapCount, allSubrscData.data());

    // barrier

    if(afterState != D3D12_RESOURCE_STATE_COMMON)
    {
        graphicsCmdLists_[curCmdListIdx_].cmdList->ResourceBarrier(
            1, get_temp_ptr(CD3DX12_RESOURCE_BARRIER::Transition(
                dst.Get(), D3D12_RESOURCE_STATE_COMMON, afterState)));

        isCurGraphicsCmdListDirty_ = true;
    }

    UploadingRsc rcd;
    rcd.rsc                = dst;
    rcd.upload             = uploadBuf;
    rcd.expectedFenceValue = nextExpectedFinishFenceValue_;

    uploadingRscs_.push_back(rcd);

    isCurCmdListDirty_ = true;
}

void ResourceUploader::submit()
{
    // submit current cmd lists

    ID3D12CommandList *rawCopyCmdLists[] =
        { copyCmdLists_[curCmdListIdx_].cmdList };
    ID3D12CommandList *rawGraphicsCmdLists[] =
        { graphicsCmdLists_[curCmdListIdx_].cmdList };

    copyCmdLists_    [curCmdListIdx_].cmdList->Close();
    graphicsCmdLists_[curCmdListIdx_].cmdList->Close();

    if(isCurGraphicsCmdListDirty_)
    {
        copyQueue_->ExecuteCommandLists(1, rawCopyCmdLists);
        copyQueue_->Signal(
            copyToGraphicsFence_.Get(), nextExpectedCopyToGraphicsFenceValue_);

        graphicsQueue_->Wait(
            copyToGraphicsFence_.Get(), nextExpectedCopyToGraphicsFenceValue_++);
        graphicsQueue_->ExecuteCommandLists(1, rawGraphicsCmdLists);
        graphicsQueue_->Signal(
            finishFence_.Get(), nextExpectedFinishFenceValue_++);
    }
    else
    {
        copyQueue_->ExecuteCommandLists(1, rawCopyCmdLists);
        copyQueue_->Signal(
            finishFence_.Get(), nextExpectedFinishFenceValue_++);
    }

    // switch to next cmd lists

    curCmdListIdx_ = (curCmdListIdx_ + 1) % copyCmdLists_.size();

    finishFence_->SetEventOnCompletion(
        copyCmdLists_[curCmdListIdx_].expectedFenceValue, nullptr);
    finishFence_->SetEventOnCompletion(
        graphicsCmdLists_[curCmdListIdx_].expectedFenceValue, nullptr);

    copyCmdLists_[curCmdListIdx_].expectedFenceValue =
        nextExpectedFinishFenceValue_;
    graphicsCmdLists_[curCmdListIdx_].expectedFenceValue =
        nextExpectedFinishFenceValue_;

    copyCmdLists_    [curCmdListIdx_].cmdList.resetCommandList();
    graphicsCmdLists_[curCmdListIdx_].cmdList.resetCommandList();

    isCurCmdListDirty_         = false;
    isCurGraphicsCmdListDirty_ = false;
}

void ResourceUploader::collect()
{
    std::vector<UploadingRsc> newRscs;
    for(auto &rsc : uploadingRscs_)
    {
        if(finishFence_->GetCompletedValue() < rsc.expectedFenceValue)
            newRscs.push_back(std::move(rsc));
    }
    uploadingRscs_.swap(newRscs);
}

void ResourceUploader::waitForIdle()
{
    if(isCurCmdListDirty_)
        submit();

    for(size_t i = 0; i < copyCmdLists_.size(); ++i)
    {
        if(i == curCmdListIdx_)
            continue;

        finishFence_->SetEventOnCompletion(
            copyCmdLists_[i].expectedFenceValue, nullptr);

        finishFence_->SetEventOnCompletion(
            graphicsCmdLists_[i].expectedFenceValue, nullptr);
    }

    collect();
}

AGZ_D3D12_END
