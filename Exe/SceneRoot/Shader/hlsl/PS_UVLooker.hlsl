struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
};

float4 main(PS_INPUT i) : SV_Target
{
    return float4(frac(i.uv.x), frac(i.uv.y), 0, 1);
}