struct Input
{
	float4 pos	: POSITION;
	float2 uv	: TEXCOORD0;
};

struct V2F
{
	float2 uv	: TEXCOORD0;
	float4 pos	: SV_POSITION;
};

V2F main(Input input)
{
	V2F output;
	output.pos = input.pos;
	output.uv = input.uv;
	return output;
}