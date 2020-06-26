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

AGZ_D3D12_FG_END
