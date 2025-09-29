Texture2D gTex : register(t0);
SamplerState gS : register(s0);
struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
};
float4 main(PS_INPUT i) : SV_Target
{
    return gTex.Sample(gS, i.uv);
}