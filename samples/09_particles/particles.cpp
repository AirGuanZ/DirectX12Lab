#include <random>

#include "./particles.h"

ParticleSystem::ParticleSystem(
    ID3D12Device *device, int frameCount, int particleCnt)
{
    const size_t bufByteSize = sizeof(ParticleData) * particleCnt;
    const fg::BufDesc bufDesc(
        bufByteSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

    const CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);

    AGZ_D3D12_CHECK_HR(
        device->CreateCommittedResource(
            &heapProps, D3D12_HEAP_FLAG_NONE, &bufDesc.desc,
            D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
            IID_PPV_ARGS(dataA_.GetAddressOf())));

    AGZ_D3D12_CHECK_HR(
        device->CreateCommittedResource(
            &heapProps, D3D12_HEAP_FLAG_NONE, &bufDesc.desc,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr,
            IID_PPV_ARGS(dataB_.GetAddressOf())));

    // initial particle data

    std::default_random_engine rng{ std::random_device()() };
    const std::uniform_real_distribution<float> dis(0, 1);

    std::vector<ParticleData> initParticles(particleCnt);
    for(auto &p : initParticles)
    {
        using agz::math::distribution::uniform_on_sphere;

        const float u1 = dis(rng);
        const float u2 = dis(rng);

        const Vec3 posOnUnitSphere = uniform_on_sphere(u1, u2).first;

        p.position = 10.0f * posOnUnitSphere;
        p.velocity = Vec3(0);
    }

    // TODO
}
