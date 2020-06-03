#pragma once

#include <d3d12.h>

#include <agz/d3d12/common.h>

AGZ_D3D12_LAB_BEGIN

struct RootDescriptor
{
    UINT shaderRegister;
    UINT registerSpace;
};

struct RootConstant
{
    UINT shaderRegister;
    UINT registerSpace;
    UINT num32BitValues;
};

class RootDescriptorTable
{
    std::vector<D3D12_DESCRIPTOR_RANGE> ranges_;

public:

    RootDescriptorTable();
};

class RootSignature : public misc::uncopyable_t
{
public:


};

AGZ_D3D12_LAB_END
