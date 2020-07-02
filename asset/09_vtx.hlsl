struct VSOutput
{
    float3 worldPos : POSITION;
};

struct Particle
{
    float3 position;
    float pad0;
    float3 velocity;
    float pad1;
};

StructuredBuffer<Particle> Particles : register(t0);

VSOutput main(uint vertexIdx : SV_VertexID)
{
    VSOutput output = (VSOutput)0;
    output.worldPos = Particles[vertexIdx].position;
    return output;
}
