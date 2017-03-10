struct V2F
{
	float2 uv	: TEXCOORD0;
	float4 pos	: SV_POSITION;
};

cbuffer firstCB : register(b0)
{
	float4		color;
}

Texture2D tex : register(t0);
SamplerState samp : register(s0);

float4 main(V2F input) : SV_TARGET
{
	return tex.Sample(samp, input.uv) * color;
}