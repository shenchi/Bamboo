cbuffer FrameConstants : register (b0)
{
	matrix	matView;
	matrix	matProj;
};

cbuffer InstanceConstants : register (b1)
{
	matrix	matWorld;
	matrix	matWorld_IT;
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
	float3 normal	: NORMAL;
	float3 tangent	: TANGENT;
	float2 uv		: TEXCOORD0;
};

V2F main(Input input)
{
	V2F output;

	matrix matMVP = mul(mul(matWorld, matView), matProj);

	output.position = mul(float4(input.position, 1), matMVP);
	output.normal = mul(input.normal, (float3x3)matWorld_IT);
	output.tangent = mul(input.tangent, (float3x3)matWorld_IT);
	output.uv = input.uv;

	return output;
}