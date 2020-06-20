#pragma once

#include <agz/d3d12/buffer/constantBuffer.h>
#include <agz/d3d12/buffer/vertexBuffer.h>

#include <agz/d3d12/camera/walkingCamera.h>

#include <agz/d3d12/cmd/perFrameCmdList.h>
#include <agz/d3d12/cmd/singleCmdList.h>

#include <agz/d3d12/descriptor/rawDescriptorHeap.h>
#include <agz/d3d12/descriptor/descriptorHeap.h>

#include <agz/d3d12/framegraph/rootSignature.h>

#include <agz/d3d12/framegraph/resourceView/depthStencilViewDesc.h>
#include <agz/d3d12/framegraph/resourceView/descriptorTableRangeDesc.h>
#include <agz/d3d12/framegraph/resourceView/renderTargetViewDesc.h>
#include <agz/d3d12/framegraph/resourceView/shaderResourceViewDesc.h>
#include <agz/d3d12/framegraph/resourceView/staticSamplerDesc.h>
#include <agz/d3d12/framegraph/resourceView/unorderedAccessViewDesc.h>

#include <agz/d3d12/imgui/imguiIntegration.h>

#include <agz/d3d12/misc/resourceReleaser.h>

#include <agz/d3d12/pipeline/pipelineState.h>
#include <agz/d3d12/pipeline/shader.h>

#include <agz/d3d12/sync/cmdQueueWaiter.h>

#include <agz/d3d12/texture/depthStencilBuffer.h>
#include <agz/d3d12/texture/mipmap.h>
#include <agz/d3d12/texture/texture2d.h>

#include <agz/d3d12/window/debugLayer.h>
#include <agz/d3d12/window/window.h>
