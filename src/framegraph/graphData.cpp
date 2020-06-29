#include <agz/d3d12/framegraph/graphData.h>
#include <agz/d3d12/framegraph/passContext.h>

AGZ_D3D12_FG_BEGIN

FrameGraphResourceNode::FrameGraphResourceNode(
    bool isExternal, ComPtr<ID3D12Resource> d3dRsc)
    : isExternal_(isExternal), d3dRsc_(d3dRsc)
{
    
}

void FrameGraphResourceNode::setExternalResource(
    ComPtr<ID3D12Resource> d3dRsc)
{
    assert(isExternal_);
    d3dRsc_ = d3dRsc;
}

ID3D12Resource *FrameGraphResourceNode::getD3DResource() const noexcept
{
    return d3dRsc_.Get();
}

FrameGraphPassNode::FrameGraphPassNode(
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

FrameGraphPassNode::FrameGraphPassNode(
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
        else if(r.beforeState == D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
            inBarriers.push_back(CD3DX12_RESOURCE_BARRIER::UAV(d3dRsc));

        if(r.inState != r.afterState)
        {
            outBarriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
                d3dRsc, r.inState, r.afterState));
        }

        // IMPROVE: out UAV barrier is omitted, which may cause problems
        // if user uses it through uav after the fg execution
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

bool FrameGraphPassNode::execute(
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

AGZ_D3D12_FG_END
