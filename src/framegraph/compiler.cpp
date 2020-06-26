#include <agz/d3d12/framegraph/compiler.h>

AGZ_D3D12_FG_BEGIN

std::pair<bool, D3D12_CLEAR_VALUE>
    FrameGraphCompiler::CompilerInternalResourceNode
        ::getClearValue() const noexcept
{
    if(clearColor)
    {
        D3D12_CLEAR_VALUE clearValue;
        clearValue.Format = clearFormat;
        std::memcpy(clearValue.Color, &clearColorValue.r, sizeof(ClearColor));
        return { true, clearValue };
    }

    if(clearDepthStencil)
    {
        D3D12_CLEAR_VALUE clearValue;
        clearValue.Format               = clearFormat;
        clearValue.DepthStencil.Depth   = clearDepthStencilValue.depth;
        clearValue.DepthStencil.Stencil = clearDepthStencilValue.stencil;
        return { true, clearValue };
    }

    return { false, {} };
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
    ID3D12Device      *device,
    ResourceAllocator &rscAlloc,
    ResourceReleaser  &rscReleaser)
{
    FrameGraphData ret;
    ret.rscNodes.reserve(rscs_.size());
    ret.passNodes.reserve(passes_.size());

    struct TempRscNode
    {
        D3D12_RESOURCE_STATES actualInitialState = {};
        std::vector<std::pair<PassIndex, D3D12_RESOURCE_STATES>> users;
    };

    std::vector<TempRscNode> tempRscNodes(rscs_.size());

    // collect tempRscNode.users
    // count cpu/gpu descriptors
    // fill rsc clear values

    for(size_t i = 0; i < passes_.size(); ++i)
    {
        const PassIndex passIdx = { static_cast<int32_t>(i) };
        auto &pass = passes_[i];

        for(auto &rscUsage : pass.rscs)
        {
            auto &tempRsc = tempRscNodes[rscUsage.idx.idx];

            const int idxInRscUsers = static_cast<int>(tempRsc.users.size());
            rscUsage.idxInRscUsers = idxInRscUsers;

            tempRsc.users.push_back({ passIdx, rscUsage.inState });

            // collect rsc creation flags

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

            const DXGI_FORMAT rtdsFmt = match_variant(rscUsage.viewDesc,
                [&](const _internalSRV &)  { ++ret.gpuDescCount; return DXGI_FORMAT_UNKNOWN; },
                [&](const _internalUAV &)  { ++ret.gpuDescCount; return DXGI_FORMAT_UNKNOWN; },
                [&](const _internalRTV &r) { ++ret.rtvDescCount; return r.desc.Format; },
                [&](const _internalDSV &d) { ++ret.dsvDescCount; return d.desc.Format; },
                [&](const std::monostate &) { return DXGI_FORMAT_UNKNOWN; });
            
            // fill clear value

            auto tn = rscs_[rscUsage.idx.idx]
                .as_if<CompilerInternalResourceNode>();

            if(!rscUsage.rtdsBinding.is<std::monostate>() &&
                tn && !tn->clearColor && !tn->clearDepthStencil)
            {
                DXGI_FORMAT clearFormat;
                if(rtdsFmt != DXGI_FORMAT_UNKNOWN)
                    clearFormat = rtdsFmt;
                else
                    clearFormat = tn->desc.desc.Format;
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
    }

    // allocate d3d rsc & record actual initial state

    for(size_t i = 0; i < rscs_.size(); ++i)
    {
        const auto &rsc     = rscs_[i];
        auto       &tempRsc = tempRscNodes[i];

        ComPtr<ID3D12Resource> d3dRsc;

        match_variant(rsc,
            [&](const CompilerInternalResourceNode &tn)
        {
            const auto [clear, clearValue] = tn.getClearValue();

            d3dRsc = rscAlloc.allocResource(
                { tn.desc.desc, clear, clearValue }, tn.initialState);

            tempRsc.actualInitialState = tn.initialState;

            rscReleaser.add(rscAlloc, d3dRsc);
        },
            [&](const CompilerExternalResourceNode &en)
        {
            d3dRsc                     = en.rsc;
            tempRsc.actualInitialState = en.initialState;
        });

        ret.rscNodes.emplace_back(
            rsc.is<CompilerExternalResourceNode>(), d3dRsc);
    }

    // allocate cpu/gpu desc range

    DescriptorIndex nextRTVDescIdx = 0;
    DescriptorIndex nextDSVDescIdx = 0;
    DescriptorIndex nextGPUDescIdx = 0;

    // fill fg pass nodes

    D3D12_RESOURCE_DESC firstRTDSDesc;
    firstRTDSDesc.Width = 0;

    for(auto &pass : passes_)
    {
        std::map<ResourceIndex, FrameGraphPassNode::PassResource> passRscs;
        for(auto &rscUsage : pass.rscs)
        {
            FrameGraphPassNode::PassResource passRsc;

            const auto &passNode = rscs_[rscUsage.idx.idx];
            const auto &tempRsc  = tempRscNodes[rscUsage.idx.idx];

            // d3d rsc

            passRsc.rscIdx = rscUsage.idx;
            
            // state transitions

            if(rscUsage.idxInRscUsers > 0)
            {
                passRsc.beforeState =
                    tempRsc.users[rscUsage.idxInRscUsers - 1].second;
            }
            else
                passRsc.beforeState = tempRsc.actualInitialState;

            passRsc.inState = rscUsage.inState;

            if(rscUsage.idxInRscUsers + 1 ==
                static_cast<int>(tempRsc.users.size()))
            {
                match_variant(passNode,
                    [&](const CompilerExternalResourceNode &en)
                {
                    passRsc.afterState = en.finalState;
                },
                    [&](const CompilerInternalResourceNode &tn)
                {
                    passRsc.afterState = tempRsc.actualInitialState;
                });
            }
            else
                passRsc.afterState = rscUsage.inState;

            // assign descriptor

            passRsc.viewDesc = rscUsage.viewDesc;

            match_variant(rscUsage.viewDesc,
                [&](const _internalSRV &)
            {
                passRsc.descIdx = nextGPUDescIdx++;
            },
                [&](const _internalUAV &)
            {
                passRsc.descIdx = nextGPUDescIdx++;
            },
                [&](const _internalRTV &)
            {
                passRsc.descIdx = nextRTVDescIdx++;
            },
                [&](const _internalDSV &)
            {
                passRsc.descIdx = nextDSVDescIdx++;
            },
                [&](const std::monostate &) { }
            );

            passRsc.rtdsBinding = rscUsage.rtdsBinding;

            if(!firstRTDSDesc.Width && !rscUsage.rtdsBinding.is<std::monostate>())
            {
                match_variant(rscUsage.viewDesc,
                    [&](const _internalRTV &rt)
                {
                    firstRTDSDesc = ret.rscNodes[rt.rsc.idx]
                        .getD3DResource()->GetDesc();
                },
                    [&](const _internalDSV &ds)
                {
                    firstRTDSDesc = ret.rscNodes[ds.rsc.idx]
                        .getD3DResource()->GetDesc();
                },
                    [](const auto &) {});
            }
            
            // fill descriptor format

            auto fillFmt = [&](DXGI_FORMAT &fmt)
            {
                if(fmt == DXGI_FORMAT_UNKNOWN)
                {
                    fmt = ret.rscNodes[rscUsage.idx.idx]
                        .getD3DResource()->GetDesc().Format;
                }
            };

            match_variant(
                rscUsage.viewDesc,
                [&](_internalSRV &view) { fillFmt(view.desc.Format); },
                [&](_internalUAV &view) { fillFmt(view.desc.Format); },
                [&](_internalRTV &view) { fillFmt(view.desc.Format); },
                [&](_internalDSV &view) { fillFmt(view.desc.Format); },
                [&](const std::monostate &) {});

            passRscs[rscUsage.idx] = passRsc;
        }

        // viewport & scissor

        FrameGraphPassNode::PassViewport vp;
        if(pass.defaultViewport)
        {
            if(firstRTDSDesc.Width > 0)
            {
                D3D12_VIEWPORT defaultVP;
                defaultVP.TopLeftX = 0;
                defaultVP.TopLeftY = 0;
                defaultVP.Width    = static_cast<float>(firstRTDSDesc.Width);
                defaultVP.Height   = static_cast<float>(firstRTDSDesc.Height);
                defaultVP.MinDepth = 0;
                defaultVP.MaxDepth = 1;
                vp.viewports = { defaultVP };
            }
        }
        else
            vp.viewports = pass.viewports;

        if(pass.defaultScissor)
        {
            if(firstRTDSDesc.Width > 0)
            {
                D3D12_RECT defaultSc;
                defaultSc.top    = 0;
                defaultSc.left   = 0;
                defaultSc.right  = static_cast<LONG>(firstRTDSDesc.Width);
                defaultSc.bottom = static_cast<LONG>(firstRTDSDesc.Height);
                vp.scissors = { defaultSc };
            }
        }
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

AGZ_D3D12_FG_END