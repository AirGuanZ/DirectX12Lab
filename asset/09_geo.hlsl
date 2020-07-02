cbuffer GeometryConsts : register(b0)
{
    float4x4 ViewProj;
    float3 EyePos;
    float GeoSize;
};

struct GSInput
{
    float3 worldPos : POSITION;
};

struct GSOutput
{
    float4 position : SV_POSITION;
    float3 worldPos : POSITION;
    float2 texCoord : TEXCOORD;
};

[maxvertexcount(4)]
void main(
    point GSInput input[1], inout TriangleStream<GSOutput> output)
{
    float3 nor = normalize(EyePos - input[0].worldPos);
    float3 laux = abs(nor.z) > 0.98 ? float3(1, 0, 0) : float3(0, 0, 1);

    float3 localX = normalize(cross(nor, laux));
    float3 localY = cross(nor, localX);

    GSOutput vtx;
    
    vtx.worldPos = input[0].worldPos + GeoSize * (localX + localY);
    vtx.position = mul(float4(vtx.worldPos, 1), ViewProj);
    vtx.texCoord = float2(1, 0);
    output.Append(vtx);

    vtx.worldPos = input[0].worldPos + GeoSize * (-localX + localY);
    vtx.position = mul(float4(vtx.worldPos, 1), ViewProj);
    vtx.texCoord = float2(0, 0);
    output.Append(vtx);

    vtx.worldPos = input[0].worldPos + GeoSize * (localX - localY);
    vtx.position = mul(float4(vtx.worldPos, 1), ViewProj);
    vtx.texCoord = float2(1, 1);
    output.Append(vtx);

    vtx.worldPos = input[0].worldPos + GeoSize * (-localX - localY);
    vtx.position = mul(float4(vtx.worldPos, 1), ViewProj);
    vtx.texCoord = float2(0, 1);
    output.Append(vtx);
}
