cbuffer ModelCB : register(b0)
{
    matrix gWorld;
    matrix gView;
    matrix gProj;
    float4 gBaseColor;
};

struct VS_INPUT
{
    float3 pos : POSITION;
    float3 normal : NORMAL;
    float4 tangent : TANGENT;
    float2 uv : TEXCOORD;
    uint4 bi : BLENDINDICES; // ���g�p�i�����X�L���p�j
    float4 bw : BLENDWEIGHT; // ���g�p
};

struct VS_OUTPUT
{
    float4 pos : SV_POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
};

VS_OUTPUT main(VS_INPUT i)
{
    VS_OUTPUT o;
    float4 wp = mul(float4(i.pos, 1), gWorld);
    float4 vp = mul(wp, gView);
    o.pos = mul(vp, gProj);
    // �@���̓��[���h�s��̏� 3x3 ���g�p�i��X�P�[�� or �����X�P�[���z��j
    o.normal = mul(float4(i.normal, 0), gWorld).xyz;
    o.uv = i.uv;
    return o;
}