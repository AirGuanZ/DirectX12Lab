#pragma once

#include <agz/d3d12/lab.h>

using namespace agz::d3d12;

class ParticleSystem : public agz::misc::uncopyable_t
{
public:

    ParticleSystem();

    // set max count of particles, count of new particles per seconds,
    // and lifetime of each particle (in ms)
    void setParticles(
        int maxCount, int cntPerFrame, float lifetimeMS);

    // set pos, dir, initial vel and range of the emitter
    void setEmitter(
        const Vec3 &pos, const Vec3 &dir,
        float vel, float rangeDeg);

    // update the emitter in each frame
    void update(int frameIndex, float elapsedTimeMS);

    // register particle pass(es) in framegraph
    void addGraphPass(fg::FrameGraph &graph, fg::ResourceIndex renderTarget);
};
