#include <agz/d3d12/framegraph/compiler.h>

AGZ_D3D12_FG_BEGIN

std::optional<D3D12_CLEAR_VALUE>
    FrameGraphCompiler::CompilerInternalResourceNode
        ::getClearValue() const noexcept
{
    if(clearColor)
    {
        D3D12_CLEAR_VALUE clearValue;
        clearValue.Format = clearFormat;
        std::memcpy(clearValue.Color, &clearColorValue.r, sizeof(ClearColor));
        return clearValue;
    }

    if(clearDepthStencil)
    {
        D3D12_CLEAR_VALUE clearValue;
        clearValue.Format               = clearFormat;
        clearValue.DepthStencil.Depth   = clearDepthStencilValue.depth;
        clearValue.DepthStencil.Stencil = clearDepthStencilValue.stencil;
        return clearValue;
    }

    return std::nullopt;
}

ResourceIndex FrameGraphCompiler::addInternalResource(
    const RscDesc &rscDesc, D3D12_RESOURCE_STATES initialState)
{
    CompilerInternalResourceNode newNode;
    newNode.desc         = rscDesc;
    newNode.initialState = initialState;

    const auto idx = static_cast<int32_t>(rscs_.size());
    rscs_.emplace_back(newNode);
    return { idx };
}

ResourceIndex FrameGraphCompiler::addInternalResource(
    const RscDesc        &rscDesc,
    D3D12_RESOURCE_STATES initialState,
    const ClearColor     &clearColorValue,
    DXGI_FORMAT           clearFormat)
{
    CompilerInternalResourceNode newNode;
    newNode.desc            = rscDesc;
    newNode.initialState    = initialState;
    newNode.clearColor      = true;
    newNode.clearColorValue = clearColorValue;
    
    if(clearFormat == DXGI_FORMAT_UNKNOWN)
        newNode.clearFormat = rscDesc.desc.Format;
    else
        newNode.clearFormat = clearFormat;

    const auto idx = static_cast<int32_t>(rscs_.size());
    rscs_.emplace_back(newNode);
    return { idx };
}

ResourceIndex FrameGraphCompiler::addInternalResource(
    const RscDesc           &rscDesc,
    D3D12_RESOURCE_STATES    initialState,
    const ClearDepthStencil &clearDepthStencilValue,
    DXGI_FORMAT              clearFormat)
{
    CompilerInternalResourceNode newNode;
    newNode.desc                   = rscDesc;
    newNode.initialState           = initialState;
    newNode.clearDepthStencil      = true;
    newNode.clearDepthStencilValue = clearDepthStencilValue;

    if(clearFormat == DXGI_FORMAT_UNKNOWN)
        newNode.clearFormat = rscDesc.desc.Format;
    else
        newNode.clearFormat = clearFormat;

    const auto idx = static_cast<int32_t>(rscs_.size());
    rscs_.emplace_back(newNode);
    return { idx };
}

ResourceIndex FrameGraphCompiler::addExternalResource(
    ComPtr<ID3D12Resource> rsc,
    D3D12_RESOURCE_STATES  initialState,
    D3D12_RESOURCE_STATES  finalState)
{
    CompilerExternalResourceNode newNode;
    newNode.rsc          = rsc;
    newNode.initialState = initialState;
    newNode.finalState   = finalState;

    const auto idx = static_cast<int32_t>(rscs_.size());
    rscs_.emplace_back(newNode);
    return { idx };
}

FrameGraphData FrameGraphCompiler::compile(
    ResourceAllocator &rscAlloc,
    ResourceReleaser  &rscReleaser)
{
    FrameGraphData ret;
    ret.rscNodes.reserve(rscs_.size());
    ret.passNodes.reserve(passes_.size());

    // collect usages

    const auto usageInfo = collectRscUsages();

    ret.gpuDescCount = usageInfo.gpuDescCount;
    ret.rtvDescCount = usageInfo.rtvDescCount;
    ret.dsvDescCount = usageInfo.dsvDescCount;

    // infer rsc flags & clear values

    for(auto &pass : passes_)
    {
        for(auto &rscUsage : pass.rscs)
            inferRscCreationFlagAndClearValue(rscUsage);
    }

    // allocate d3d rsc

    for(auto &rsc : rscs_)
    {
        ret.rscNodes.push_back(
            createD3DRscNode(rsc, rscAlloc, rscReleaser));
    }

    // allocate cpu/gpu desc range

    DescriptorIndex rtvDescIdx = 0;
    DescriptorIndex dsvDescIdx = 0;
    DescriptorIndex gpuDescIdx = 0;

    // fill fg pass nodes

    for(auto &pass : passes_)
    {
        std::map<ResourceIndex, FrameGraphPassNode::PassResource> passRscs;

        // create final pass resource node

        const CompilerPassNode::RscInPass::ViewDesc *rtdsView = nullptr;
        for(auto &rscUsage : pass.rscs)
        {
            passRscs[rscUsage.idx] = createFinalPassResource(
                rscUsage, usageInfo.rscTempNodes, ret.rscNodes,
                gpuDescIdx, rtvDescIdx, dsvDescIdx, rtdsView);
        }

        // viewport & scissor

        auto inferredVP = inferDefaultViewportAndScissor(
            rtdsView, ret.rscNodes);

        FrameGraphPassNode::PassViewport vp;
        if(pass.defaultViewport)
            vp.viewports = std::move(inferredVP.viewports);
        else
            vp.viewports = pass.viewports;

        if(pass.defaultScissor)
            vp.scissors = std::move(inferredVP.scissors);
        else
            vp.scissors = pass.scissors;

        if(pass.isGraphics)
        {
            ret.passNodes.emplace_back(
                std::move(passRscs), vp, pass.passFunc,
                pass.pipelineState, pass.rootSignature);
        }
        else
        {
            ret.passNodes.emplace_back(
                std::move(passRscs), pass.passFunc, pass.rootSignature);
        }
    }

    return ret;
}

void FrameGraphCompiler::inferRscCreationFlagAndClearValue(
    CompilerPassNode::RscInPass &rscUsage)
{
    if(auto tn = rscs_[rscUsage.idx.idx].as_if
        <CompilerInternalResourceNode>(); tn)
    {
        if(tn->initialState == D3D12_RESOURCE_STATE_COMMON)
            tn->initialState = rscUsage.inState;

        if(rscUsage.inState & D3D12_RESOURCE_STATE_RENDER_TARGET)
        {
            tn->desc.desc.Flags |=
                D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        }

        if(rscUsage.inState & D3D12_RESOURCE_STATE_DEPTH_WRITE)
        {
            tn->desc.desc.Flags |=
                D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        }

        if(rscUsage.inState & D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
        {
            tn->desc.desc.Flags |=
                D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        }
    }
    
    // fill clear value

    auto tn = rscs_[rscUsage.idx.idx]
        .as_if<CompilerInternalResourceNode>();

    if(!rscUsage.rtdsBinding.is<std::monostate>() &&
        tn && !tn->clearColor && !tn->clearDepthStencil)
    {
        const DXGI_FORMAT rtdsFmt = match_variant(rscUsage.viewDesc,
            [&](const _internalSRV &)   { return DXGI_FORMAT_UNKNOWN; },
            [&](const _internalUAV &)   { return DXGI_FORMAT_UNKNOWN; },
            [&](const _internalRTV &r)  { return r.desc.Format; },
            [&](const _internalDSV &d)  { return d.desc.Format; },
            [&](const std::monostate &) { return DXGI_FORMAT_UNKNOWN; });

        const DXGI_FORMAT clearFormat = rtdsFmt != DXGI_FORMAT_UNKNOWN ?
                                        rtdsFmt : tn->desc.desc.Format;
        assert(clearFormat != DXGI_FORMAT_UNKNOWN);

        if(!isTypeless(clearFormat))
        {
            if(auto rtb = rscUsage.rtdsBinding
                .as_if<FrameGraphPassNode::PassResource::RTB>();
                rtb)
            {
                tn->clearColor      = rtb->clear;
                tn->clearColorValue = rtb->clearColor;
                tn->clearFormat     = clearFormat;
            }
            else
            {
                assert(rscUsage.rtdsBinding.is<
                    FrameGraphPassNode::PassResource::DSB>());
                auto &dsb = rscUsage.rtdsBinding.as<
                    FrameGraphPassNode::PassResource::DSB>();

                tn->clearDepthStencil = dsb.clearDepth ||
                    dsb.clearStencil;
                tn->clearDepthStencilValue = dsb.clearDethpStencil;
                tn->clearFormat = clearFormat;
            }
        }
    }
}

FrameGraphCompiler::RscUsageInfo FrameGraphCompiler::collectRscUsages()
{
    RscUsageInfo info;
    info.rscTempNodes.resize(rscs_.size());

    for(size_t i = 0; i < passes_.size(); ++i)
    {
        const PassIndex passIdx = { static_cast<int32_t>(i) };
        auto &pass = passes_[i];

        for(auto &rscUsage : pass.rscs)
        {
            auto &tempRsc = info.rscTempNodes[rscUsage.idx.idx];

            const int idxInRscUsers = static_cast<int>(tempRsc.users.size());
            rscUsage.idxInRscUsers = idxInRscUsers;

            tempRsc.users.push_back({ passIdx, rscUsage.inState });

            match_variant(rscUsage.viewDesc,
                [&](const _internalSRV &)  { ++info.gpuDescCount; },
                [&](const _internalUAV &)  { ++info.gpuDescCount; },
                [&](const _internalRTV &r) { ++info.rtvDescCount; },
                [&](const _internalDSV &d) { ++info.dsvDescCount; },
                [&](const std::monostate &) { });
        }
    }

    return info;
}

FrameGraphResourceNode FrameGraphCompiler::createD3DRscNode(
    const CompilerResourceNode &cn,
    ResourceAllocator &rscAlloc,
    ResourceReleaser &rscReleaser) const
{
    ComPtr<ID3D12Resource> d3dRsc;

    match_variant(cn,
        [&](const CompilerInternalResourceNode &tn)
    {
        const auto clearValue = tn.getClearValue();

        d3dRsc = rscAlloc.allocResource(
            {
                tn.desc.desc,
                clearValue.has_value(),
                clearValue.has_value() ? *clearValue : D3D12_CLEAR_VALUE{}
            },
            tn.initialState);

        rscReleaser.add(rscAlloc, d3dRsc);
    },
        [&](const CompilerExternalResourceNode &en)
    {
        d3dRsc = en.rsc;
    });

    return FrameGraphResourceNode(
        cn.is<CompilerExternalResourceNode>(), d3dRsc);
}

FrameGraphCompiler::PassRscStates FrameGraphCompiler::getPassRscStates(
    const CompilerPassNode::RscInPass &rscUsage,
    const CompilerResourceNode &rscNode,
    const TempRscNode &tempNode) const
{
    PassRscStates ret = {};

    const D3D12_RESOURCE_STATES rscInitState = match_variant(
        rscs_[rscUsage.idx.idx],
        [&](const CompilerExternalResourceNode &en)
    {
        return en.initialState;
    },
        [&](const CompilerInternalResourceNode &in)
    {
        return in.initialState;
    });

    if(rscUsage.idxInRscUsers > 0)
    {
        ret.beforeState =
            tempNode.users[rscUsage.idxInRscUsers - 1].second;
    }
    else
        ret.beforeState = rscInitState;

    ret.inState = rscUsage.inState;

    if(rscUsage.idxInRscUsers + 1 ==
        static_cast<int>(tempNode.users.size()))
    {
        match_variant(rscNode,
            [&](const CompilerExternalResourceNode &en)
        {
            ret.afterState = en.finalState;
        },
            [&](const CompilerInternalResourceNode &)
        {
            ret.afterState = rscInitState;
        });
    }
    else
        ret.afterState = rscUsage.inState;

    return ret;
}

void FrameGraphCompiler::inferDescFormat(
    CompilerPassNode::RscInPass &rscUsage, ID3D12Resource *d3dRsc) const
{
    auto fillFmt = [&](DXGI_FORMAT &fmt)
    {
        if(fmt == DXGI_FORMAT_UNKNOWN)
            fmt = d3dRsc->GetDesc().Format;
    };

    match_variant(
        rscUsage.viewDesc,
        [&](_internalSRV &view) { fillFmt(view.desc.Format); },
        [&](_internalUAV &view) { fillFmt(view.desc.Format); },
        [&](_internalRTV &view) { fillFmt(view.desc.Format); },
        [&](_internalDSV &view) { fillFmt(view.desc.Format); },
        [&](const std::monostate &) {});
}

FrameGraphPassNode::PassViewport FrameGraphCompiler::inferDefaultViewportAndScissor(
    const CompilerPassNode::RscInPass::ViewDesc *view,
    const std::vector<FrameGraphResourceNode> &rscNodes)
{
    FrameGraphPassNode::PassViewport ret;
    if(!view)
        return ret;

    const D3D12_RESOURCE_DESC rtdsDesc = match_variant(*view,
        [&](const _internalRTV &rt)
    {
        return rscNodes[rt.rsc.idx].getD3DResource()->GetDesc();
    },
        [&](const _internalDSV &ds)
    {
        return rscNodes[ds.rsc.idx].getD3DResource()->GetDesc();
    },
        [](const auto &) { return D3D12_RESOURCE_DESC{}; });

    D3D12_VIEWPORT defaultVP;
    defaultVP.TopLeftX = 0;
    defaultVP.TopLeftY = 0;
    defaultVP.Width    = static_cast<float>(rtdsDesc.Width);
    defaultVP.Height   = static_cast<float>(rtdsDesc.Height);
    defaultVP.MinDepth = 0;
    defaultVP.MaxDepth = 1;
    ret.viewports = { defaultVP };

    D3D12_RECT defaultSc;
    defaultSc.top    = 0;
    defaultSc.left   = 0;
    defaultSc.right  = static_cast<LONG>(rtdsDesc.Width);
    defaultSc.bottom = static_cast<LONG>(rtdsDesc.Height);
    ret.scissors = { defaultSc };

    return ret;
}

FrameGraphPassNode::PassResource FrameGraphCompiler::createFinalPassResource(
    CompilerPassNode::RscInPass                  &rscUsage,
    const std::vector<TempRscNode>               &rscTempNodes,
    const std::vector<FrameGraphResourceNode>    &rscNodes,
    DescriptorIndex                              &gpuDescIdx,
    DescriptorIndex                              &rtvDescIdx,
    DescriptorIndex                              &dsvDescIdx,
    const CompilerPassNode::RscInPass::ViewDesc *&rtdsView)
{
    FrameGraphPassNode::PassResource passRsc;
    
    passRsc.rscIdx = rscUsage.idx;
    
    // state transitions
    
    const auto &rscNode = rscs_[rscUsage.idx.idx];
    const auto &tempRsc = rscTempNodes[rscUsage.idx.idx];
    
    const auto states = getPassRscStates(
        rscUsage, rscNode, tempRsc);
    
    passRsc.beforeState = states.beforeState;
    passRsc.inState     = states.inState;
    passRsc.afterState  = states.afterState;
    
    // assign descriptor
    
    match_variant(rscUsage.viewDesc,
        [&](const _internalSRV &) { passRsc.descIdx = gpuDescIdx++; },
        [&](const _internalUAV &) { passRsc.descIdx = gpuDescIdx++; },
        [&](const _internalRTV &) { passRsc.descIdx = rtvDescIdx++; },
        [&](const _internalDSV &) { passRsc.descIdx = dsvDescIdx++; },
        [&](const std::monostate &) { });
    
    // render target / depth stencil binding
    
    passRsc.rtdsBinding = rscUsage.rtdsBinding;
    
    if(!rscUsage.rtdsBinding.is<std::monostate>())
    {
        assert(rscUsage.viewDesc.is<_internalRTV>() ||
               rscUsage.viewDesc.is<_internalDSV>());
        rtdsView = &rscUsage.viewDesc;
    }
    
    inferDescFormat(
        rscUsage, rscNodes[rscUsage.idx.idx].getD3DResource());

    passRsc.viewDesc = rscUsage.viewDesc;

    return passRsc;
}

AGZ_D3D12_FG_END
