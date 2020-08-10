#include "./09_noise.hlsl"

cbuffer PixelConsts : register(b1)
{
    float ColorT;
    float ColorSFreq;
    float ColorTFreq;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float3 worldPos : POSITION;
    float2 texCoord : TEXCOORD;
};

// see https://en.wikipedia.org/wiki/HSL_and_HSV#To_RGB
float3 HSV2RGB(float3 HSV)
{
    float3 RGB = 0;
    float C = HSV.z * HSV.y;
    float H = HSV.x * 6;
    float X = C * (1 - abs(fmod(H, 2) - 1));

    if(HSV.y != 0)
    {
        float I = floor(H);
        if(I == 0)
            RGB = float3(C, X, 0);
        else if(I == 1)
            RGB = float3(X, C, 0);
        else if(I == 2)
            RGB = float3(0, C, X);
        else if(I == 3)
            RGB = float3(0, X, C);
        else if(I == 4)
            RGB = float3(X, 0, C);
        else
            RGB = float3(C, 0, X);
    }

    return RGB + HSV.z - C;
}

float4 main(PSInput input) : SV_TARGET
{
    float H = 0.5 * snoise(float4(
        ColorSFreq * input.worldPos, ColorTFreq * ColorT)) + 0.5;
    float3 c = saturate(HSV2RGB(float3(H, 0.8, 0.8)));
    return float4(pow(c, 1 / 2.2), 1);
}
