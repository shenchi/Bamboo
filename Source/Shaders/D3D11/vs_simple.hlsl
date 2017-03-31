struct Input
{
	float3 position	: POSITION;
	float3 normal	: NORMAL;
	float3 tangent	: TANGENT;
	float2 uv		: TEXCOORD0;
};

struct V2F
{
	float2 uv	: TEXCOORD0;
	float4 pos	: SV_POSITION;
};

V2F main(Input input)
{
	V2F output;
	output.pos = float4(input.position, 1);
	output.uv = input.uv;
	return output;
}