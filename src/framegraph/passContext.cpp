#include <agz/d3d12/framegraph/passContext.h>

AGZ_D3D12_FG_BEGIN

FrameGraphPassContext::FrameGraphPassContext(
    const std::vector<FrameGraphResourceNode> &rscNodes,
    const FrameGraphPassNode                  &passNode,
    DescriptorRange                            allGPUDescs,
    DescriptorRange                            allRTVDescs,
    DescriptorRange                            allDSVDescs) noexcept
    : requestCmdListSubmission_(false), rscNodes_(rscNodes), passNode_(passNode),
      allGPUDescs_(allGPUDescs), allRTVDescs_(allRTVDescs), allDSVDescs_(allDSVDescs)
{
    
}

FrameGraphPassContext::Resource FrameGraphPassContext::getResource(
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

void FrameGraphPassContext::requestCmdListSubmission() noexcept
{
    requestCmdListSubmission_ = true;
}

bool FrameGraphPassContext::isCmdListSubmissionRequested() const noexcept
{
    return requestCmdListSubmission_;
}

AGZ_D3D12_FG_END
