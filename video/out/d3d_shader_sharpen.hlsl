sampler2D tex0 : register(s0);

uniform float4 pt : register(c0);
uniform float4 param : register(c1);

float4 main(float2 t0 : TEXCOORD0) : COLOR
{
    float2 st1 = pt.xy * 1.2;
    float4 p = tex2D(tex0, t0);
    float4 sum1 = tex2D(tex0, t0 + st1 * float2(+1, +1))
                + tex2D(tex0, t0 + st1 * float2(+1, -1))
                + tex2D(tex0, t0 + st1 * float2(-1, +1))
                + tex2D(tex0, t0 + st1 * float2(-1, -1));
    float2 st2 = pt.xy * 1.5;
    float4 sum2 = tex2D(tex0, t0 + st2 * float2(+1,  0))
                + tex2D(tex0, t0 + st2 * float2( 0, +1))
                + tex2D(tex0, t0 + st2 * float2(-1,  0))
                + tex2D(tex0, t0 + st2 * float2( 0, -1));
    float4 t = p * 0.859375 + sum2 * -0.1171875 + sum1 * -0.09765625;
    return float4(p + t * param.x);
}