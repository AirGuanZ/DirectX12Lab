#pragma once

#include <map>

#include <d3d12.h>

#include <agz/d3d12/framegraph/cmdListPool.h>
#include <agz/d3d12/descriptor/descriptorHeap.h>
#include <agz/d3d12/framegraph/resourceView/shaderResourceViewDesc.h>
#include <agz/d3d12/framegraph/resourceView/unorderedAccessViewDesc.h>
#include <agz/d3d12/framegraph/resourceView/renderTargetViewDesc.h>
#include <agz/d3d12/framegraph/resourceView/depthStencilViewDesc.h>
#include <agz/d3d12/framegraph/resourceBinding.h>
#include <agz/utility/misc.h>

AGZ_D3D12_FG_BEGIN

class FrameGraphPassContext;

using FrameGraphPassFunc = std::function<
    void(
        ID3D12GraphicsCommandList *,
        FrameGraphPassContext &
    )>;

class FrameGraphPassNode
{
public:

    friend class FrameGraphPassContext;

    struct PassResource
    {
        ComPtr<ID3D12Resource> rsc;

        bool doInitialTransition;
        bool doFinalTransition;
        D3D12_RESOURCE_STATES beforeState;
        D3D12_RESOURCE_STATES afterState;
        D3D12_RESOURCE_STATES finalState;

        enum ParamType { None, DescriptorTable };
        ParamType rootSignatureParamType;
        UINT rootSignatureParamIndex;

        // rtv & dsv are prepared for only those non-bound rt & ds
        // bound rt & ds views are recorded in PassRenderTarget & PassDepthStencil
        misc::variant_t<std::monostate, SRV, UAV, RTV, DSV> viewDesc;
        Descriptor descriptor;
    };

    struct PassRenderTarget
    {
        ComPtr<ID3D12Resource> rsc;

        RTV rtv;
        Descriptor descriptor;

        bool clear;
        ClearColor clearValue;
    };

    struct PassDepthStencil
    {
        ComPtr<ID3D12Resource> rsc;

        DSV dsv;
        Descriptor descriptor;

        bool clear;
        ClearDepthStencil clearValue;
    };

    FrameGraphPassNode(
        std::map<ResourceIndex, PassResource> &&rscs,
        std::vector<PassRenderTarget>         &&renderTargetBindings,
        std::optional<PassDepthStencil>       &&depthStencilBinding,
        ComPtr<ID3D12PipelineState>             pipelineState,
        ComPtr<ID3D12RootSignature>             rootSignature,
        FrameGraphPassFunc                    &&passFunc) noexcept;

    bool execute(ID3D12Device *device, ID3D12GraphicsCommandList *cmdList);

private:

    std::map<ResourceIndex, PassResource> rscs_;

    std::vector<PassRenderTarget> renderTargetBindings_;
    std::optional<PassDepthStencil> depthStencilBinding_;

    ComPtr<ID3D12PipelineState> pipelineState_;
    ComPtr<ID3D12RootSignature> rootSignature_;

    FrameGraphPassFunc passFunc_;
};

class FrameGraphPassContext
{
public:

    struct Resource
    {
        ComPtr<ID3D12Resource> rsc;
        D3D12_RESOURCE_STATES  currentState = D3D12_RESOURCE_STATE_COMMON;
        Descriptor             descriptor;
    };

    explicit FrameGraphPassContext(FrameGraphPassNode &passNode) noexcept;

    Resource getResource(ResourceIndex index) const;

    void requestCmdListSubmission() noexcept;

    bool isCmdListSubmissionRequested() const noexcept;

private:

    bool requestCmdListSubmission_;

    FrameGraphPassNode &passNode_;
};

class FrameGraphTaskScheduler : public misc::uncopyable_t
{
public:

    FrameGraphTaskScheduler(
        std::vector<FrameGraphPassNode> &passNodes,
        CommandListPool                 &cmdListPool,
        ComPtr<ID3D12CommandQueue>       cmdQueue);

    struct TaskRange
    {
        FrameGraphPassNode *begNode = nullptr;
        FrameGraphPassNode *endNode = nullptr;
    };

    void restart();

    TaskRange requestTask();

    void submitTask(
        TaskRange                         taskRange,
        ComPtr<ID3D12GraphicsCommandList> cmdList);

    void waitForAllTasksFinished();

private:

    std::vector<FrameGraphPassNode> &passNodes_;
    CommandListPool                 &cmdListPool_;
    ComPtr<ID3D12CommandQueue>       cmdQueue_;

    enum class TaskState
    {
        NotFinished,
        Pending,
        Submitted
    };

    struct Task
    {
        TaskState taskState = TaskState::NotFinished;
        size_t nodeCount = 0;
        ComPtr<ID3D12GraphicsCommandList> cmdList;
    };

    std::mutex tasksMutex_;
    std::vector<Task> tasks_;

    size_t dispatchedNodeCount_;
    size_t finishedNodeCount_;

    std::condition_variable waitingForAllTasks_;
};

AGZ_D3D12_FG_END

#include "./impl/graph.inl"
