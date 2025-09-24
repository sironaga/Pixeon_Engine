cbuffer CameraCB : register(b0)
{
    matrix view;
    matrix proj;
};

struct VS_INPUT {
    float3 pos : POSITION;
    float4 color : COLOR0;
};
struct VS_OUTPUT {
    float4 pos : SV_POSITION;
    float4 color : COLOR0;
};
VS_OUTPUT main(VS_INPUT input) {
    VS_OUTPUT output;
    float4 wpos = float4(input.pos, 1.0f);
    output.pos = mul(float4(input.pos, 1.0f), view);
    output.pos = mul(output.pos, proj);
    output.color = input.color;
    return output;
}
