#pragma once

#include <agz/d3d12/framegraph/graphData.h>

AGZ_D3D12_FG_BEGIN

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

AGZ_D3D12_FG_END
