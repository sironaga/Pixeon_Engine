cbuffer MaterialCB : register(b0)
{
    float4 BaseColor;
    float2 MetallicRoughness; // x=metallic (–¢Žg—p‚Å‚ ‚ê‚Î0), y=roughness
    uint UseBaseTex;
    uint _Pad0;
}

Texture2D BaseTex : register(t0);
SamplerState SampLin : register(s0);

struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
};

float3 ApplyLighting(float3 albedo, float3 N)
{
    float3 L = normalize(float3(0.4, 0.6, 0.7));
    float ndl = saturate(dot(N, L));
    float3 diff = albedo * ndl;
    float3 ambient = albedo * 0.2;
    return diff + ambient;
}

float4 main(PS_INPUT input) : SV_Target
{
    float4 base = BaseColor;
    if (UseBaseTex == 1)
    {
        float4 tex = BaseTex.Sample(SampLin, input.uv);
        base *= tex;
    }
    float3 n = normalize(input.normal);
    float3 lit = ApplyLighting(base.rgb, n);
    return float4(lit, base.a);
}