#pragma once

#include <agz/d3d12/descriptor/descriptorHeap.h>
#include <agz/d3d12/framegraph/resourceView/depthStencilViewDesc.h>
#include <agz/d3d12/framegraph/resourceView/renderTargetViewDesc.h>
#include <agz/d3d12/framegraph/resourceView/shaderResourceViewDesc.h>
#include <agz/d3d12/framegraph/resourceView/unorderedAccessViewDesc.h>
#include <agz/d3d12/framegraph/RTDSBinding.h>
#include <agz/utility/misc.h>

AGZ_D3D12_FG_BEGIN

class FrameGraphPassContext;
class FrameGraphTaskScheduler;

using FrameGraphPassFunc = std::function<
    void(
        ID3D12GraphicsCommandList *,
        FrameGraphPassContext &
        )>;

class FrameGraphResourceNode
{
public:

    FrameGraphResourceNode(bool isExternal, ComPtr<ID3D12Resource> d3dRsc);

    void setExternalResource(ComPtr<ID3D12Resource> d3dRsc);

    ID3D12Resource *getD3DResource() const noexcept;

private:

    bool isExternal_;

    ComPtr<ID3D12Resource> d3dRsc_;
};

class FrameGraphPassNode
{
public:

    struct PassResource
    {
        ResourceIndex rscIdx;

        D3D12_RESOURCE_STATES beforeState  = {};
        D3D12_RESOURCE_STATES inState      = {};
        D3D12_RESOURCE_STATES afterState   = {};

        using ViewDesc = misc::variant_t<
            std::monostate,
            _internalSRV,
            _internalUAV,
            _internalRTV,
            _internalDSV>;
        ViewDesc viewDesc;

        DescriptorIndex descIdx = 0;
        mutable Descriptor descriptor;

        // render target & depth stencil binding

        struct RTB
        {
            bool clear = false;
            ClearColor clearColor;
        };

        struct DSB
        {
            bool clearDepth   = false;
            bool clearStencil = false;
            ClearDepthStencil clearDethpStencil;
        };

        using RTDSBinding = misc::variant_t<std::monostate, RTB, DSB>;

        RTDSBinding rtdsBinding;
    };

    struct PassViewport
    {
        std::vector<D3D12_VIEWPORT> viewports;
        std::vector<D3D12_RECT> scissors;
    };

    // init as graphics node
    FrameGraphPassNode(
        std::map<ResourceIndex, PassResource> rscs,
        PassViewport                          passViewport,
        FrameGraphPassFunc                    passFunc,
        ComPtr<ID3D12PipelineState>           pipelineState,
        ComPtr<ID3D12RootSignature>           rootSignature) noexcept;

    // init as compute node
    FrameGraphPassNode(
        std::map<ResourceIndex, PassResource> rscs,
        FrameGraphPassFunc                    passFunc,
        ComPtr<ID3D12RootSignature>           rootSignature) noexcept;

    bool execute(
        ID3D12Device                        *device,
        std::vector<FrameGraphResourceNode> &rscNodes,
        DescriptorRange                      allGPUDescs,
        DescriptorRange                      allRTVDescs,
        DescriptorRange                      allDSVDescs,
        ID3D12GraphicsCommandList           *cmdList) const;

private:

    template<bool IS_GRAPHICS>
    bool executeImpl(
        ID3D12Device                        *device,
        std::vector<FrameGraphResourceNode> &rscNodes,
        DescriptorRange                      allGPUDescs,
        DescriptorRange                      allRTVDescs,
        DescriptorRange                      allDSVDescs,
        ID3D12GraphicsCommandList           *cmdList) const;

    friend class FrameGraphPassContext;

    bool isGraphics_;

    std::map<ResourceIndex, PassResource> rscs_;

    PassViewport viewport_;

    FrameGraphPassFunc passFunc_;

    ComPtr<ID3D12PipelineState> pipelineState_;
    ComPtr<ID3D12RootSignature> rootSignature_;
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

    FrameGraphPassContext(
        const std::vector<FrameGraphResourceNode> &rscNodes,
        const FrameGraphPassNode                  &passNode,
        DescriptorRange                            allGPUDescs,
        DescriptorRange                            allRTVDescs,
        DescriptorRange                            allDSVDescs) noexcept;

    Resource getResource(ResourceIndex index) const;

    void requestCmdListSubmission() noexcept;

    bool isCmdListSubmissionRequested() const noexcept;

private:

    bool requestCmdListSubmission_;

    const std::vector<FrameGraphResourceNode> &rscNodes_;
    const FrameGraphPassNode                  &passNode_;

    DescriptorRange allGPUDescs_;
    DescriptorRange allRTVDescs_;
    DescriptorRange allDSVDescs_;
};

struct FrameGraphData
{
    std::vector<FrameGraphPassNode>     passNodes;
    std::vector<FrameGraphResourceNode> rscNodes;

    DescriptorIndex gpuDescCount = 0;
    DescriptorIndex rtvDescCount = 0;
    DescriptorIndex dsvDescCount = 0;
};

inline FrameGraphResourceNode::FrameGraphResourceNode(
    bool isExternal, ComPtr<ID3D12Resource> d3dRsc)
    : isExternal_(isExternal), d3dRsc_(d3dRsc)
{
    
}

inline void FrameGraphResourceNode::setExternalResource(
    ComPtr<ID3D12Resource> d3dRsc)
{
    assert(isExternal_);
    d3dRsc_ = d3dRsc;
}

inline ID3D12Resource *FrameGraphResourceNode::getD3DResource() const noexcept
{
    return d3dRsc_.Get();
}

inline FrameGraphPassNode::FrameGraphPassNode(
    std::map<ResourceIndex, PassResource> rscs,
    PassViewport                          passViewport,
    FrameGraphPassFunc                    passFunc,
    ComPtr<ID3D12PipelineState>           pipelineState,
    ComPtr<ID3D12RootSignature>           rootSignature) noexcept
    : isGraphics_(true),
      rscs_(std::move(rscs)),
      viewport_(std::move(passViewport)),
      passFunc_(std::move(passFunc)),
      pipelineState_(std::move(pipelineState)),
      rootSignature_(std::move(rootSignature))
{

}

inline FrameGraphPassNode::FrameGraphPassNode(
    std::map<ResourceIndex, PassResource> rscs,
    FrameGraphPassFunc                    passFunc,
    ComPtr<ID3D12RootSignature>           rootSignature) noexcept
    : isGraphics_(false),
      rscs_(std::move(rscs)),
      viewport_({}),
      passFunc_(std::move(passFunc)),
      rootSignature_(std::move(rootSignature))
{
    
}

template<bool IS_GRAPHICS>
bool FrameGraphPassNode::executeImpl(
    ID3D12Device                        *device,
    std::vector<FrameGraphResourceNode> &rscNodes,
    DescriptorRange                      allGPUDescs,
    DescriptorRange                      allRTVDescs,
    DescriptorRange                      allDSVDescs,
    ID3D12GraphicsCommandList           *cmdList) const
{
    // rsc barriers & descs

    std::vector<D3D12_RESOURCE_BARRIER> inBarriers, outBarriers;
    inBarriers.reserve(rscs_.size());
    outBarriers.reserve(rscs_.size());

    for(auto &p : rscs_)
    {
        auto &r     = p.second;
        auto d3dRsc = rscNodes[r.rscIdx.idx].getD3DResource();

        if(r.beforeState != r.inState)
        {
            inBarriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
                d3dRsc, r.beforeState, r.inState));
        }

        if(r.inState != r.afterState)
        {
            outBarriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
                d3dRsc, r.inState, r.afterState));
        }
    }

    if(!inBarriers.empty())
    {
        cmdList->ResourceBarrier(
            static_cast<UINT>(inBarriers.size()), inBarriers.data());
    }

    // create descriptors

    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> renderTargetHandles;
    std::optional<D3D12_CPU_DESCRIPTOR_HANDLE> depthStencilHandle;

    D3D12_RESOURCE_DESC firstRTVOrDSVDesc;
    firstRTVOrDSVDesc.Width = 0;

    for(auto &p : rscs_)
    {
        auto &r = p.second;
        auto d3dRsc = rscNodes[r.rscIdx.idx].getD3DResource();
        
        match_variant(r.viewDesc,
            [&](const _internalSRV &srv)
        {
            r.descriptor = allGPUDescs[r.descIdx];
            device->CreateShaderResourceView(
                d3dRsc, &srv.desc, r.descriptor);
        },
            [&](const _internalUAV &uav)
        {
            r.descriptor = allGPUDescs[r.descIdx];
            device->CreateUnorderedAccessView(
                d3dRsc, nullptr, &uav.desc, r.descriptor);
        },
            [&](const _internalRTV &rtv)
        {
            r.descriptor = allRTVDescs[r.descIdx];
            device->CreateRenderTargetView(
                d3dRsc, &rtv.desc, r.descriptor);

            if constexpr(IS_GRAPHICS)
            {
                if(auto rtBinding = r.rtdsBinding.as_if<PassResource::RTB>();
                    rtBinding)
                {
                    renderTargetHandles.push_back(r.descriptor);
                    if(rtBinding->clear)
                    {
                        cmdList->ClearRenderTargetView(
                            r.descriptor,
                            &rtBinding->clearColor.r,
                            0, nullptr);
                    }

                    if(!firstRTVOrDSVDesc.Width)
                    {
                        firstRTVOrDSVDesc = rscNodes[r.rscIdx.idx]
                            .getD3DResource()->GetDesc();
                    }
                }
            }
        },
            [&](const _internalDSV &dsv)
        {
            r.descriptor = allDSVDescs[r.descIdx];
            device->CreateDepthStencilView(
                d3dRsc, &dsv.desc, r.descriptor);

            if constexpr(IS_GRAPHICS)
            {
                if(auto dsBinding = r.rtdsBinding.as_if<PassResource::DSB>();
                    dsBinding)
                {
                    depthStencilHandle = r.descriptor;
                    if(dsBinding->clearDepth || dsBinding->clearStencil)
                    {
                        const D3D12_CLEAR_FLAGS clearFlags =
                            dsBinding->clearDepth && dsBinding->clearStencil ?
                            D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL :
                            (dsBinding->clearDepth ?
                                D3D12_CLEAR_FLAG_DEPTH :
                                D3D12_CLEAR_FLAG_STENCIL);

                        cmdList->ClearDepthStencilView(
                            r.descriptor, clearFlags,
                            dsBinding->clearDethpStencil.depth,
                            dsBinding->clearDethpStencil.stencil,
                            0, nullptr);
                    }

                    if(!firstRTVOrDSVDesc.Width)
                    {
                        firstRTVOrDSVDesc = rscNodes[r.rscIdx.idx]
                            .getD3DResource()->GetDesc();
                    }
                }
            }
        },
            [&](const std::monostate &) {});
    }

    // bind render target

    if constexpr(IS_GRAPHICS)
    {
        if(!renderTargetHandles.empty())
        {
            if(depthStencilHandle)
            {
                cmdList->OMSetRenderTargets(
                    static_cast<UINT>(renderTargetHandles.size()),
                    renderTargetHandles.data(),
                    false,
                    &*depthStencilHandle);
            }
            else
            {
                cmdList->OMSetRenderTargets(
                    static_cast<UINT>(renderTargetHandles.size()),
                    renderTargetHandles.data(),
                    false,
                    nullptr);
            }
        }
        else
        {
            if(depthStencilHandle)
            {
                cmdList->OMSetRenderTargets(
                    0, nullptr, false, &*depthStencilHandle);
            }
            else
            {
                cmdList->OMSetRenderTargets(
                    0, nullptr, false, nullptr);
            }
        }
    }

    // viewport & scissor

    if constexpr(IS_GRAPHICS)
    {
        cmdList->RSSetViewports(
            static_cast<UINT>(viewport_.viewports.size()),
            viewport_.viewports.data());

        cmdList->RSSetScissorRects(
            static_cast<UINT>(viewport_.scissors.size()),
            viewport_.scissors.data());
    }

    // pipeline state

    if constexpr(IS_GRAPHICS)
    {
        if(pipelineState_)
            cmdList->SetPipelineState(pipelineState_.Get());
    }

    // root signature

    if(rootSignature_)
    {
        if constexpr(IS_GRAPHICS)
            cmdList->SetGraphicsRootSignature(rootSignature_.Get());
        else
            cmdList->SetComputeRootSignature(rootSignature_.Get());
    }

    // pass func context

    FrameGraphPassContext passCtx(
        rscNodes, *this, allGPUDescs, allRTVDescs, allDSVDescs);

    // call pass func

    assert(passFunc_);
    passFunc_(cmdList, passCtx);

    // final state transitions

    if(!outBarriers.empty())
    {
        cmdList->ResourceBarrier(
            static_cast<UINT>(outBarriers.size()), outBarriers.data());
    }

    return passCtx.isCmdListSubmissionRequested();
}

inline bool FrameGraphPassNode::execute(
    ID3D12Device                        *device,
    std::vector<FrameGraphResourceNode> &rscNodes,
    DescriptorRange                      allGPUDescs,
    DescriptorRange                      allRTVDescs,
    DescriptorRange                      allDSVDescs,
    ID3D12GraphicsCommandList           *cmdList) const
{
    if(isGraphics_)
    {
        return executeImpl<true>(
            device, rscNodes, allGPUDescs, allRTVDescs, allDSVDescs, cmdList);
    }
    return executeImpl<false>(
        device, rscNodes, allGPUDescs, allRTVDescs, allDSVDescs, cmdList);
}

inline FrameGraphPassContext::FrameGraphPassContext(
    const std::vector<FrameGraphResourceNode> &rscNodes,
    const FrameGraphPassNode                  &passNode,
    DescriptorRange                            allGPUDescs,
    DescriptorRange                            allRTVDescs,
    DescriptorRange                            allDSVDescs) noexcept
    : requestCmdListSubmission_(false), rscNodes_(rscNodes), passNode_(passNode),
      allGPUDescs_(allGPUDescs), allRTVDescs_(allRTVDescs), allDSVDescs_(allDSVDescs)
{
    
}

inline FrameGraphPassContext::Resource FrameGraphPassContext::getResource(
    ResourceIndex index) const
{
    const auto it = passNode_.rscs_.find(index);
    if(it == passNode_.rscs_.end())
        return {};

    Resource ret;
    ret.rsc          = rscNodes_[index.idx].getD3DResource();
    ret.currentState = it->second.afterState;
    ret.descriptor   = it->second.descriptor;
    return ret;
}

inline void FrameGraphPassContext::requestCmdListSubmission() noexcept
{
    requestCmdListSubmission_ = true;
}

inline bool FrameGraphPassContext::isCmdListSubmissionRequested() const noexcept
{
    return requestCmdListSubmission_;
}

AGZ_D3D12_FG_END
