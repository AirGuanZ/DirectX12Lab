#include <agz/d3d12/framegraph/cmdListPool.h>

AGZ_D3D12_FG_BEGIN

CommandListPool::CommandListPool(
    ComPtr<ID3D12Device> device, int threadCount, int frameCount)
    : device_(std::move(device))
{
    // per-thread resources

    threadResources_.resize(threadCount);
    for(auto &t : threadResources_)
    {
        t.graphicsCmdAllocs_.resize(frameCount);

        for(auto &ca : t.graphicsCmdAllocs_)
        {
            AGZ_D3D12_CHECK_HR(
                device_->CreateCommandAllocator(
                    D3D12_COMMAND_LIST_TYPE_DIRECT,
                    IID_PPV_ARGS(ca.GetAddressOf())));
        }
    }
}

void CommandListPool::startFrame(int frameIndex)
{
    for(auto &t : threadResources_)
        AGZ_D3D12_CHECK_HR(t.graphicsCmdAllocs_[frameIndex]->Reset());

    frameIndex_ = frameIndex;
}

ComPtr<ID3D12GraphicsCommandList>
    CommandListPool::requireGraphicsCommandList(int threadIndex)
{
    ComPtr<ID3D12GraphicsCommandList> ret;

    {
        std::lock_guard lk(unusedGraphicsCmdListsMutex_);
        if(!unusedGraphicsCmdLists_.empty())
        {
            ret = unusedGraphicsCmdLists_.front();
            unusedGraphicsCmdLists_.erase(unusedGraphicsCmdLists_.begin());
        }
    }

    auto alloc = threadResources_[threadIndex]
        .graphicsCmdAllocs_[frameIndex_].Get();

    if(!ret)
    {
        AGZ_D3D12_CHECK_HR(
            device_->CreateCommandList(
                0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc, nullptr,
                IID_PPV_ARGS(ret.GetAddressOf())));
    }
    else
        AGZ_D3D12_CHECK_HR(ret->Reset(alloc, nullptr));

    return ret;
}

void CommandListPool::addUnusedGraphicsCmdLists(
    ComPtr<ID3D12GraphicsCommandList> cmdList)
{
    std::lock_guard lk(unusedGraphicsCmdListsMutex_);
    unusedGraphicsCmdLists_.push_back(cmdList);
}

AGZ_D3D12_FG_END
