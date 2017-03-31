cbuffer LightingConstants : register (b0)
{
	float4	cameraPos;
}

struct V2F
{
	float4 position	: SV_POSITION;
	float3 worldPos	: POSITION;
	float3 normal	: NORMAL;
	float3 tangent	: TANGENT;
	float2 uv		: TEXCOORD0;
};

TextureCube cubeMap : register(t0);
Texture2D diffuseTex : register(t1);
Texture2D normalMap : register(t2);
SamplerState samp : register(s0);

float4 main(V2F input) : SV_TARGET
{
	float3 normal = normalize(input.normal);
	float3 tangent = normalize(input.tangent - dot(input.tangent, normal) * normal);
	float3 bitangent = cross(tangent, normal);

	float3x3 TBN = float3x3(tangent, bitangent, normal);

	float3 normTexel = normalMap.Sample(samp, input.uv).xyz * 2.0 - 1.0;

	normal = mul(normTexel, TBN);

	float3 viewDir = normalize(cameraPos.xyz - input.worldPos);
	float3 refl = reflect(-viewDir, normal);

	float3 color = diffuseTex.Sample(samp, input.uv).rgb;
	color =  color * 0.7 + cubeMap.Sample(samp, refl).rgb * 0.3;

	return float4(color, 1);
}