#pragma once

#include <map>

#include <d3d12.h>

#include <agz/d3d12/framegraph/common.h>

AGZ_D3D12_FG_BEGIN

struct ResourceNode
{
    std::vector<PassIndex> users;

    D3D12_RESOURCE_DESC desc;
    ComPtr<ID3D12Resource> d3dRsc;

    D3D12_RESOURCE_STATES initialState;
    D3D12_RESOURCE_STATES finalState;
};

struct ResourceInPass
{
    size_t idxInRscUsers;

    D3D12_RESOURCE_STATES inState;
    D3D12_RESOURCE_STATES useState;
    D3D12_RESOURCE_STATES outState;
};

struct PassNode
{
    std::map<ResourceIndex, ResourceInPass> usedRscs;
};

AGZ_D3D12_FG_END
