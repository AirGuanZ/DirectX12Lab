#pragma once

#include <agz/d3d12/lab.h>

using namespace agz::d3d12;

class Mesh : public agz::misc::uncopyable_t
{
public:

    struct Vertex
    {
        Vec3 pos;
        Vec3 nor;
        Vec2 uv;
    };

    struct MeshData
    {
        ComPtr<ID3D12Resource> albedo;

        const Vertex *vertices;
        size_t        vertexCount;
    };

    std::vector<ComPtr<ID3D12Resource>> loadFromFile(
        const Window              &window,
        DescriptorSubHeap         &descHeap,
        ID3D12GraphicsCommandList *copyCmdList,
        const std::string         &objFilename,
        const std::string         &albedoFilename);

    void setWorldTransform(const Mat4 &world) noexcept;

    void draw(
        ID3D12GraphicsCommandList *cmdList,
        int                        imageIndex,
        const WalkingCamera       &camera) const;

private:

    struct VSTransform
    {
        Mat4 WVP;
        Mat4 world;
    };

    Texture2D albedo_;
    DescriptorRange albedoDescTable_;

    Mat4 world_;
    mutable ConstantBuffer<VSTransform> vsTransform_;

    VertexBuffer<Vertex> vertexBuffer_;
};
