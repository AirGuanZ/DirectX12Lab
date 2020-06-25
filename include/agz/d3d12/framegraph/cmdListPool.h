#pragma once

#include <mutex>

#include <d3d12.h>

#include <agz/d3d12/framegraph/common.h>

AGZ_D3D12_FG_BEGIN

class CommandListPool : public misc::uncopyable_t
{
public:

    CommandListPool(
        ComPtr<ID3D12Device> device, int threadCount, int frameCount);

    ~CommandListPool();

    void startFrame(
        int frameIndex);

    ComPtr<ID3D12GraphicsCommandList> requireGraphicsCommandList(int threadIndex);

    void addUnusedGraphicsCmdLists(ComPtr<ID3D12GraphicsCommandList> cmdList);

private:

    ComPtr<ID3D12Device> device_;

    int frameIndex_ = 0;

    struct ThreadResource
    {
        // frame index -> cmd alloc
        std::vector<ComPtr<ID3D12CommandAllocator>> graphicsCmdAllocs_;
    };

    std::vector<ThreadResource> threadResources_;

    // all unused cmd list
    // usage cycle of a cmd list
    // 1. required by a task. if unusedCmdLists_ is empty, create a new one
    // 2. reset to require allocator
    // 3. submitted by a task. goto 1/3.
    // 4. reported to execution thread
    // 5. (optional) pending
    // 6. added to unusedCmdLists_
    std::vector<ComPtr<ID3D12GraphicsCommandList>> unusedGraphicsCmdLists_;
    std::mutex unusedGraphicsCmdListsMutex_;
};

inline CommandListPool::CommandListPool(
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

inline CommandListPool::~CommandListPool()
{

}

inline void CommandListPool::startFrame(int frameIndex)
{
    for(auto &t : threadResources_)
        AGZ_D3D12_CHECK_HR(t.graphicsCmdAllocs_[frameIndex]->Reset());

    frameIndex_ = frameIndex;
}

inline ComPtr<ID3D12GraphicsCommandList>
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

inline void CommandListPool::addUnusedGraphicsCmdLists(
    ComPtr<ID3D12GraphicsCommandList> cmdList)
{
    std::lock_guard lk(unusedGraphicsCmdListsMutex_);
    unusedGraphicsCmdLists_.push_back(cmdList);
}

AGZ_D3D12_FG_END
