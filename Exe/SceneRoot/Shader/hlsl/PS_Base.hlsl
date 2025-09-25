cbuffer MaterialCB : register(b0)
{
    float4 LineColor;
};
struct PS_INPUT {
    float4 pos   : SV_POSITION;
    float4 color : COLOR0;
};
float4 main(PS_INPUT input) : SV_Target {
    // ���_�F��Material�F�̏�Z��
    return input.color * LineColor;
}
