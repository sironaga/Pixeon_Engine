cbuffer CameraCB : register(b0)
{
    matrix world;
    matrix view;
    matrix proj;
};

cbuffer SkinCB : register(b1)
{
    matrix gBones[128];
};

struct VS_INPUT
{
    float3 pos : POSITION;
    float3 normal : NORMAL;
    float4 tangent : TANGENT;
    float2 uv : TEXCOORD0;
    uint4 boneIdx : BONEINDICES;
    float4 boneW : BONEWEIGHTS;
};

struct VS_OUTPUT
{
    float4 pos : SV_POSITION;
    float3 normal : NORMAL;
    float3 worldPos : TEXCOORD1;
    float2 uv : TEXCOORD0;
};

VS_OUTPUT main(VS_INPUT IN)
{
    VS_OUTPUT o;
    float4 localPos = float4(IN.pos, 1.0f);

    float4 skinnedPos = 0;
    float3 skinnedNormal = 0;

    float weightSum = IN.boneW.x + IN.boneW.y + IN.boneW.z + IN.boneW.w;

    [unroll]
    for (int i = 0; i < 4; i++)
    {
        float w = IN.boneW[i];
        if (w > 0.0f)
        {
            uint bi = IN.boneIdx[i];
            matrix m = gBones[bi];
            skinnedPos += mul(localPos, m) * w;
            float3 n = mul(float4(IN.normal, 0), m).xyz;
            skinnedNormal += n * w;
        }
    }

    // ”ñƒXƒLƒ“: ‚»‚Ì‚Ü‚Ü
    if (weightSum == 0.0f)
    {
        skinnedPos = localPos;
        skinnedNormal = IN.normal;
    }
    else
    {
        skinnedNormal = normalize(skinnedNormal);
    }

    float4 wp = mul(skinnedPos, world);
    float4 vp = mul(wp, view);
    o.pos = mul(vp, proj);
    o.normal = mul(float4(skinnedNormal, 0), world).xyz;
    o.worldPos = wp.xyz;
    o.uv = IN.uv;
    return o;
}