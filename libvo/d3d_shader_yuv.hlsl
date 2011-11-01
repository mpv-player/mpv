// Compile with:
// fxc.exe /Tps_2_0 /Fhd3d_shader_yuv.h d3d_shader_yuv.hlsl /Vnd3d_shader_yuv

sampler2D tex0 : register(s0);
sampler2D tex1 : register(s1);
sampler2D tex2 : register(s2);

uniform float4x4 colormatrix : register(c0);

float4 main(float2 t0 : TEXCOORD0,
            float2 t1 : TEXCOORD1,
            float2 t2 : TEXCOORD2)
            : COLOR
{
    float4 c = float4(tex2D(tex0, t0).x,
                      tex2D(tex1, t1).x,
                      tex2D(tex2, t2).x,
                      1);
    return mul(c, colormatrix);
}
