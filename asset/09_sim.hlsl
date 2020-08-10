#include "./09_noise.hlsl"

cbuffer SimulationConstants
{
    uint AttractedParticleCount;
    uint AttractorCount;
    float BackgroundForceT;
    float DeltaT;

    float4x4 AttractorWorld;

    float MaxVel;
    float AttractorForce;
    float AttractedRandomForce;
    float UnattractedRandomForce;

    float RandomForceSFreq;
    float RandomForceTFreq;
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
#define PARTICLE_MAX_RANGE 4

float3 getAttractorForce(float3 p2A)
{
    float len = length(p2A);
    return AttractorForce * p2A;
}

float3 limitVel(float3 oldVel)
{
    float len = length(oldVel);
    float fac = len > MaxVel ? MaxVel / len : 1;
    return fac * oldVel;
}

float3 randomForce(float3 pos)
{
    float x = snoise(float4(
        RandomForceSFreq * pos + float3(100, 0, 0),
        RandomForceTFreq * BackgroundForceT));
    float y = snoise(float4(
        RandomForceSFreq * pos + float3(200, 0, 0),
        RandomForceTFreq * BackgroundForceT));
    float z = snoise(float4(
        RandomForceSFreq * pos + float3(300, 0, 0),
        RandomForceTFreq * BackgroundForceT));
    return float3(x, y, z);
}

void processAttracted(int particleIdx)
{
    Particle particle = PrevParticles[particleIdx];
    Attractor attractor = Attractors[particleIdx % AttractorCount];

    float4 aPw = mul(float4(attractor.position, 1), AttractorWorld);
    float3 aPos = aPw.xyz / aPw.w;
    
    float3 p2A = aPos - particle.position;
    float3 attractorForce = float3(0, 0, 0);

    float dis = length(p2A);
    if(dis > 0.1)
        attractorForce = getAttractorForce(p2A) +
                         AttractedRandomForce * randomForce(particle.position);
    else
        particle.velocity = float3(0, 0, 0);

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
        particle.velocity + UnattractedRandomForce * randomForce(particle.position));

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
