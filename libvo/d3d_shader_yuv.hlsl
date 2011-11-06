// Compile with:
// fxc.exe /Tps_2_0 /Fhd3d_shader_yuv.h d3d_shader_yuv.hlsl /Vnd3d_shader_yuv
// fxc.exe /Tps_2_0 /Fhd3d_shader_yuv_2ch.h d3d_shader_yuv.hlsl /Vnd3d_shader_yuv_2ch /DUSE_2CH=1

// Be careful with this shader. You can't use constant slots, since we don't
// load the shader with D3DX. All uniform variables are mapped to hardcoded
// constant slots.

sampler2D tex0 : register(s0);
sampler2D tex1 : register(s1);
sampler2D tex2 : register(s2);

uniform float4x4 colormatrix : register(c0);
uniform float2 depth : register(c5);

#ifdef USE_2CH

float1 sample(sampler2D tex, float2 t)
{
    // Sample from A8L8 format as if we sampled a single value from L16.
    // We compute the 2 channel values back into one.
    return dot(tex2D(tex, t).xw, depth);
}

#else

float1 sample(sampler2D tex, float2 t)
{
    return tex2D(tex, t).x;
}

#endif

float4 main(float2 t0 : TEXCOORD0,
            float2 t1 : TEXCOORD1,
            float2 t2 : TEXCOORD2)
            : COLOR
{
    float4 c = float4(sample(tex0, t0),
                      sample(tex1, t1),
                      sample(tex2, t2),
                      1);
    return mul(c, colormatrix);
}
