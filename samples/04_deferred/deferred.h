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

// initialize
// in each frame:
//     bind g-buffer render targets
//     for each mesh
//          mesh.draw
//     bind default render target
//     draw fullscreen triangle
class DeferredRenderer : public agz::misc::uncopyable_t
{
public:

    explicit DeferredRenderer(
        Window &window, DescriptorHeap &shaderRscHeap);

    ~DeferredRenderer();

    void setLight(
        const Vec3 &pos, const Vec3 &intensity, const Vec3 &ambient) noexcept;

    void startGBuffer(ID3D12GraphicsCommandList *cmdList);

    void endGBuffer(ID3D12GraphicsCommandList *cmdList);

    void render(ID3D12GraphicsCommandList *cmdList);

private:

    void createGBuffers();

    void onResize();

    Window         &window_;
    DescriptorHeap &shaderRscHeap_;

    // g-buffer render targets

    Texture2D gBufferPosition_;
    Texture2D gBufferNormal_;
    Texture2D gBufferColor_;

    RawDescriptorHeap gBufferRTVHeap_;
    DescriptorRange gBufferSRVDescTable_;

    DepthStencilBuffer depthStencilBuffer_;

    // g-buffer pipeline

    ComPtr<ID3D12RootSignature> gBufferRootSignature_;
    ComPtr<ID3D12PipelineState> gBufferPipeline_;

    // lighting pipeline

    ComPtr<ID3D12RootSignature> lightingRootSignature_;
    ComPtr<ID3D12PipelineState> lightingPipeline_;

    // lighting constant buffer

    struct PointLight
    {
        Vec3 pos;
        float pad0 = 0;
        Vec3 color;
        float pad1 = 0;
        Vec3 ambient;
        float pad2 = 0;
    };

    PointLight lightData_;

    ConstantBuffer<PointLight> lightingPointLight_;

    // resize event handler

    WindowPostResizeHandler postResizeHandler_;
};
