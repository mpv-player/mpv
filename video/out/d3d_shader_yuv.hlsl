// Compile with:
// fxc.exe /Tps_2_0 -DUSE_420P=1 /Fhd3d_shader_420p.h d3d_shader_yuv.hlsl /Vnd3d_shader_420p
// fxc.exe /Tps_2_0 -DUSE_NV12=1 /Fhd3d_shader_nv12.h d3d_shader_yuv.hlsl /Vnd3d_shader_nv12

// Be careful with this shader. You can't use constant slots, since we don't
// load the shader with D3DX. All uniform variables are mapped to hardcoded
// constant slots.

sampler2D tex0 : register(s0);
sampler2D tex1 : register(s1);
sampler2D tex2 : register(s2);

uniform float4x4 colormatrix : register(c0);

float4 main(float2 t0 : TEXCOORD0,
            float2 t1 : TEXCOORD1,
            float2 t2 : TEXCOORD2)
            : COLOR
{
#ifdef USE_420P
    float4 c = float4(tex2D(tex0, t0).x,
                      tex2D(tex1, t1).x,
                      tex2D(tex2, t2).x,
                      1);
#endif
#ifdef USE_NV12
    float4 c = float4(tex2D(tex0, t0).x,
                      tex2D(tex1, t1).xz,
                      1);
#endif
    return mul(c, colormatrix);
}
