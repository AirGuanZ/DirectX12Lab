#include "./09_noise.hlsl"

cbuffer SimulationConstants
{
    uint AttractedParticleCount;
    uint AttractorCount;
    float BackgroundForceT;
    float DeltaT;

    float4x4 AttractorWorld;
};

struct Particle
{
    float3 position;
    float pad0;
    float3 velocity;
    float pad1;
};

struct Attractor
{
    float3 position;
    float pad0;
};

StructuredBuffer<Particle>   PrevParticles : register(t0);
RWStructuredBuffer<Particle> NextParticles : register(u0);

StructuredBuffer<Attractor> Attractors : register(t1);

#define GROUP_SIZE         256
#define PARTICLE_MAX_RANGE 5
#define PARTICLE_MAX_VEL   3

float3 getAttractorForce(float3 p2A)
{
    float len = length(p2A);
    return 0.2f * p2A / pow(max(len, 0.1f), 2);
}

float3 limitVel(float3 oldVel)
{
    float len = length(oldVel);
    float fac = len > PARTICLE_MAX_VEL ? PARTICLE_MAX_VEL / len : 1;
    return fac * oldVel;
}

float3 randomForce(float3 pos)
{
    float x = snoise(float4(2 * pos + float3(100, 0, 0), 4 * BackgroundForceT));
    float y = snoise(float4(2 * pos + float3(200, 0, 0), 4 * BackgroundForceT));
    float z = snoise(float4(2 * pos + float3(300, 0, 0), 4 * BackgroundForceT));
    return float3(x, y, z);
}

void processAttracted(int particleIdx)
{
    Particle particle = PrevParticles[particleIdx];
    Attractor attractor = Attractors[particleIdx % AttractorCount];

    float4 aPw = mul(float4(attractor.position, 1), AttractorWorld);
    float3 aPos = aPw.xyz / aPw.w;
    
    float3 attractorForce = getAttractorForce(aPos - particle.position);

    if(distance(aPos, particle.position) > 0.1)
        attractorForce += 0.05 * randomForce(particle.position);

    float3 newVel = limitVel(
        particle.velocity + attractorForce);
    
    float3 newPos = particle.position + newVel * DeltaT;

    particle.position = newPos;
    particle.velocity = newVel;

    NextParticles[particleIdx] = particle;
}

void processBackgroundForced(int particleIdx)
{
    Particle particle = PrevParticles[particleIdx];

    float3 newVel = limitVel(
        particle.velocity + 0.03 * randomForce(particle.position));

    float3 newPos = particle.position + newVel * DeltaT;
    if(dot(newPos, newPos) > PARTICLE_MAX_RANGE * PARTICLE_MAX_RANGE)
        newPos = PARTICLE_MAX_RANGE * normalize(newPos);

    particle.position = newPos;
    particle.velocity = newVel;

    NextParticles[particleIdx] = particle;
}

[numthreads(GROUP_SIZE, 1, 1)]
void main(int3 threadIdx : SV_DispatchThreadID)
{
    int particleIdx = threadIdx.x;

    if(particleIdx < AttractedParticleCount)
        processAttracted(particleIdx);
    else
        processBackgroundForced(particleIdx);
}
