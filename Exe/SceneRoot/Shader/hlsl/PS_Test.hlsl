cbuffer MaterialCB : register(b0)
{
    float4 LineColor;
};
struct PS_INPUT {
    float4 pos   : SV_POSITION;
    float4 color : COLOR0;
};
float4 main(PS_INPUT input) : SV_Target {
    // ’¸“_F‚ÆMaterialF‚ÌæZ—á
    float4 Color = { 1.0f, 0.0f, 0.0f, 1.0f };
    return Color;
}
