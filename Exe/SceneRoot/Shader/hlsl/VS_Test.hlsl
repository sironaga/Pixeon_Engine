cbuffer CameraCB : register(b0)
{
    matrix world;
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
    // ���[���h���W�ɕϊ�
    float4 worldPos = mul(float4(input.pos, 1.0f), world);
    // �r���[���W�ɕϊ�
    float4 viewPos = mul(worldPos, view);
    // �v���W�F�N�V�������W�ɕϊ�
    output.pos = mul(viewPos, proj);
    // �F�����̂܂܏o��
    output.color = input.color;
    return output;
}
