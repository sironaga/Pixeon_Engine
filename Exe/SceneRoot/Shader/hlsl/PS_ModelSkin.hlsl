cbuffer MaterialCB : register(b0)
{
    float4 BaseColor;
    float2 MetallicRoughness; // x=metallic y=roughness
    float2 pad0;
    uint UseBaseTex; // 0: テクスチャ未使用(白) 1: 使用
};

Texture2D gBaseTex : register(t0);
SamplerState gSamp : register(s0);

struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float3 normal : NORMAL;
    float3 worldPos : TEXCOORD1;
    float2 uv : TEXCOORD0;
};

float4 main(PS_INPUT IN) : SV_Target
{
    float3 N = normalize(IN.normal);
    
    // テクスチャ or フォールバック
    float4 texCol = (UseBaseTex != 0) ? gBaseTex.Sample(gSamp, IN.uv) : float4(1, 1, 1, 1);
    
    float4 col = BaseColor * texCol;

    // 簡易Lambert
    float3 L = normalize(float3(0.5, 1.0, 0.3));
    float diff = saturate(dot(N, L));
    float3 lit = col.rgb * (0.2 + diff * 0.8);

    return float4(lit, col.a);
}