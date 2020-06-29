#pragma once

#include <agz/d3d12/lab.h>

using namespace agz::d3d12;

class ParticleSystem : public agz::misc::uncopyable_t
{
public:

    ParticleSystem(
        ID3D12Device *device, int frameCount,
        int particleCnt);

    // called right before the fg execution
    void update(fg::FrameGraph &graph);

    // register passes in framegraph
    void initPasses(fg::FrameGraph &graph, fg::ResourceIndex renderTarget);

private:

    struct ParticleData
    {
        Vec3 position;
        float pad0 = 0;
        Vec3 velocity;
        float pad1 = 0;
    };

    // in each frame:
    // 0. swap prevData with nextData
    // 1. fill nextData according to prevData
    // 2. draw particles according to prevData
    fg::ResourceIndex prevData_;
    fg::ResourceIndex nextData_;

    ComPtr<ID3D12Resource> dataA_;
    ComPtr<ID3D12Resource> dataB_;
};
