struct V2F
{
	float4 position	: SV_POSITION;
	float3 normal	: NORMAL;
	float3 tangent	: TANGENT;
	float2 uv		: TEXCOORD0;
};

Texture2D tex : register(t0);
SamplerState samp : register(s0);

float4 main(V2F input) : SV_TARGET
{
	return tex.Sample(samp, input.uv);
}