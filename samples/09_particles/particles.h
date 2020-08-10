#pragma once

#include <agz/d3d12/lab.h>

using namespace agz::d3d12;

class AttractorMesh
{
public:

    struct AttractorData
    {
        Vec3 position;
        float pad0 = 0;
    };

    void loadPlane();

    void loadFromFile(const std::string &filename);

    void sampleSurface(size_t samplesCnt, Vec3 *output) const;

    ComPtr<ID3D12Resource> generateAttractorData(
        ID3D12Device *device,
        ResourceUploader &uploader,
        uint32_t attractorCount) const;

private:

    struct Triangle { Vec3 a, b, c; };

    using Triangles = std::vector<Triangle>;
    using TriangleSampler = agz::math::distribution
        ::alias_sampler_t<float, size_t>;

    static void transformToUnitCube(Triangles &triangles);

    void setTriangles(Triangles triangles);

    Triangles       triangles_;
    TriangleSampler triangleSampler_;
};

class ParticleSystem : public agz::misc::uncopyable_t
{
public:

    struct Config
    {
        float maxVel                 = 4;
        float attractorForce         = 0.2f;
        float attractedRandomForce   = 0.05f;
        float unattractedRandomForce = 0.03f;

        float randomForceSFreq = 0.5f;
        float randomForceTFreq = 4;

        float colorSFreq = 0.1f;
        float colorTFreq = 1;
    };

    ParticleSystem(
        ComPtr<ID3D12Device> device,
        ResourceUploader    &uploader,
        int                  frameCount,
        int                  particleCnt);

    void setMesh(ComPtr<ID3D12Resource> attractorData, uint32_t attractorCount);

    void setAttractedCount(uint32_t attractedCount);

    void setAttractorWorld(const Mat4 &world) noexcept;

    void setParticleSize(float size);

    void setConfig(const Config &config) noexcept;

    // call before fg execution
    void update(
        int frameIndex, float deltaT,
        const Mat4 &viewProj, const Vec3 &eye);

    // register passes in framegraph
    void initPasses(
        int               width,
        int               height,
        fg::FrameGraph   &graph,
        fg::ResourceIndex renderTarget);

private:

    struct ParticleData
    {
        Vec3 position;
        float pad0 = 0;
        Vec3 velocity;
        float pad1 = 0;
    };

    struct SimulationConstants
    {
        uint32_t attractedParticleCount = 0;
        uint32_t attractorCount         = 0;
        float    bgForceFieldT          = 0;
        float    dT                     = 0;

        Mat4 attractorWorld;

        float maxVel = 4;
        float attractorForce = 0.2f;
        float attractedRandomForce = 0.05f;
        float unattractedRandomForce = 0.03f;

        float randomForceSFreq = 0.5f;
        float randomForceTFreq = 4;

        float pad0[2] = { 0 };
    };

    struct RenderingGeometryConsts
    {
        Mat4 viewProj;
        Vec3 eyePos;
        float geoSize = 0.01f;
    };

    struct RenderingPixelConsts
    {
        float colorT = 0;
        float colorSFreq = 0.1f;
        float colorTFreq = 1;
        float pad0[1] = { 0 };
    };

    ComPtr<ID3D12Device> device_;

    int currentFrameIndex_;

    // params

    uint32_t particleCount_;
    uint32_t attractorCount_;
    uint32_t attractedCount_;
    float particleSize_;

    Config config_;

    float simulationT_;
    float colorT_;

    Mat4 attractorWorld_;

    // in each frame:
    // 0. swap prevData with nextData
    // 1. fill nextData according to prevData
    // 2. draw particles according to prevData
    fg::ResourceIndex prevData_;
    fg::ResourceIndex nextData_;

    fg::FrameGraph *framegraph_;

    ComPtr<ID3D12Resource> dataA_;
    ComPtr<ID3D12Resource> dataB_;

    // mesh

    ComPtr<ID3D12Resource> attractors_;
    fg::ResourceIndex attractorsRsc_;

    // simulation

    ComPtr<ID3D12PipelineState> simPipeline_;
    ComPtr<ID3D12RootSignature> simRootSignature_;

    ConstantBuffer<SimulationConstants> simConsts_;

    // rendering

    ComPtr<ID3D12PipelineState> rdrPipeline_;
    ComPtr<ID3D12RootSignature> rdrRootSignature_;

    ConstantBuffer<RenderingGeometryConsts> rdrGeometryConsts_;
    ConstantBuffer<RenderingPixelConsts>    rdrPixelConsts_;
};
