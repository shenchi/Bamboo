cbuffer FrameConstants : register (b0)
{
	matrix	matView;
	matrix	matProj;
};

struct Input
{
	float3 position	: POSITION;
	float3 normal	: NORMAL;
	float3 tangent	: TANGENT;
	float2 uv		: TEXCOORD0;
};

struct V2F
{
	float4 position	: SV_POSITION;
	float3 uv		: TEXCOORD0;
};

V2F main(Input input)
{
	V2F output;

	matrix matViewNoMove = matView;
	matViewNoMove._41 = 0;
	matViewNoMove._42 = 0;
	matViewNoMove._43 = 0;
	matrix matVP = mul(matViewNoMove, matProj);

	output.position = mul(float4(input.position, 1), matVP).xyww;
	output.uv = input.position;

	return output;
}