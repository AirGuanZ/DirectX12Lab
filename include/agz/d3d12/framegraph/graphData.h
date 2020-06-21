#pragma once

#include <agz/d3d12/descriptor/descriptorHeap.h>
#include <agz/d3d12/framegraph/resourceView/depthStencilViewDesc.h>
#include <agz/d3d12/framegraph/resourceView/renderTargetViewDesc.h>
#include <agz/d3d12/framegraph/resourceView/shaderResourceViewDesc.h>
#include <agz/d3d12/framegraph/resourceView/unorderedAccessViewDesc.h>
#include <agz/d3d12/framegraph/resourceBinding.h>
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

        bool doInitialTransition = false;
        bool doFinalTransition   = false;

        D3D12_RESOURCE_STATES beforeState  = {};
        D3D12_RESOURCE_STATES inState      = {};
        D3D12_RESOURCE_STATES afterState   = {};

        using ViewDesc = misc::variant_t<std::monostate, SRV, UAV, RTV, DSV>;
        ViewDesc viewDesc;

        DescriptorIndex descIdx = 0;
        mutable Descriptor descriptor;

        struct RTB
        {
            bool isBound = false;
            bool clear = false;
            ClearColor clearColor;
        };

        RTB renderTargetBinding;
    };

    FrameGraphPassNode(
        std::map<ResourceIndex, PassResource> rscs,
        FrameGraphPassFunc                    passFunc) noexcept;

    bool execute(
        ID3D12Device                        *device,
        std::vector<FrameGraphResourceNode> &rscNodes,
        DescriptorRange                      allGPUDescs,
        DescriptorRange                      allRTVDescs,
        DescriptorRange                      allDSVDescs,
        ID3D12GraphicsCommandList           *cmdList) const;

private:

    friend class FrameGraphPassContext;

    std::map<ResourceIndex, PassResource> rscs_;

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
    FrameGraphPassFunc                    passFunc) noexcept
    : rscs_(std::move(rscs)), passFunc_(std::move(passFunc))
{

}

inline bool FrameGraphPassNode::execute(
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

        if(r.doInitialTransition)
        {
            inBarriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
                d3dRsc, r.beforeState, r.inState));
        }

        if(r.doFinalTransition)
        {
            outBarriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
                d3dRsc, r.inState, r.afterState));
        }

        // create descriptor

    }

    cmdList->ResourceBarrier(
        static_cast<UINT>(inBarriers.size()), inBarriers.data());

    // create descriptors

    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> renderTargetHandles;

    for(auto &p : rscs_)
    {
        auto &r = p.second;
        auto d3dRsc = rscNodes[r.rscIdx.idx].getD3DResource();
        
        match_variant(r.viewDesc,
            [&](const SRV &srv)
        {
            r.descriptor = allGPUDescs[r.descIdx];
            device->CreateShaderResourceView(
                d3dRsc, &srv.desc, r.descriptor);
        },
            [&](const UAV &uav)
        {
            r.descriptor = allGPUDescs[r.descIdx];
            device->CreateUnorderedAccessView(
                d3dRsc, nullptr, &uav.desc, r.descriptor);
        },
            [&](const RTV &rtv)
        {
            r.descriptor = allRTVDescs[r.descIdx];
            device->CreateRenderTargetView(
                d3dRsc, &rtv.desc, r.descriptor);

            if(r.renderTargetBinding.isBound)
            {
                renderTargetHandles.push_back(r.descriptor);
                if(r.renderTargetBinding.clear)
                {
                    cmdList->ClearRenderTargetView(
                        r.descriptor,
                        &r.renderTargetBinding.clearColor.r,
                        0, nullptr);
                }
            }
        },
            [&](const DSV &dsv)
        {
            r.descriptor = allDSVDescs[r.descIdx];
            device->CreateDepthStencilView(
                d3dRsc, &dsv.desc, r.descriptor);
        },
            [&](const std::monostate &) {});
    }

    // bind render target

    if(!renderTargetHandles.empty())
    {
        cmdList->OMSetRenderTargets(
            static_cast<UINT>(renderTargetHandles.size()),
            renderTargetHandles.data(),
            false,
            nullptr);
    }
    else
    {
        cmdList->OMSetRenderTargets(
            0, nullptr, false, nullptr);
    }

    // pass func context

    FrameGraphPassContext passCtx(
        rscNodes, *this, allGPUDescs, allRTVDescs, allDSVDescs);

    // call pass func

    assert(passFunc_);
    passFunc_(cmdList, passCtx);

    // final state transitions

    cmdList->ResourceBarrier(
        static_cast<UINT>(outBarriers.size()), outBarriers.data());

    return passCtx.isCmdListSubmissionRequested();
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
