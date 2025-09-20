#include "Include/Types.hlsli"
#include "Include/Util.hlsli"

struct PS_Input {
	float4 Position : SV_POSITION;
    float4 WorldPos : POSITION;
	float3 Normal : NORMAL;
    float2 UV : TEXCOORD0;
};

PUSH_CONSTANTS(MainPassPushConstants, PushConstants);
TEXTURE_POOL()

float4 main(PS_Input input) : SV_Target0 {
    Material mat = PushConstants.Materials.Get()[PushConstants.MaterialIndex];

    float4 albedo = mat.Albedo;    
    if(mat.AlbedoTexture) {
        albedo *= sRGBToLinear( TEXTURE_SAMPLE2D(mat.AlbedoTexture, input.UV) );
    }

    float4 emissive = mat.Emissive;
    if(mat.EmissiveTexture)
        emissive *= TEXTURE_SAMPLE2D(mat.EmissiveTexture, input.UV);

    if(mat.Albedo.a <= mat.AlphaCutoff)
        discard;

	float3 normal = normalize(input.Normal);
	if(mat.NormalsTexture > 0 ) {
		float3 nmap = TEXTURE_SAMPLE2D(mat.NormalsTexture, input.UV).rgb * 2.f - 1.f;

		// calc tangent
		float3 q1 = ddx(input.WorldPos);
		float3 q2 = ddy(input.WorldPos);
		float2 st1 = ddx(input.UV);
		float2 st2 = ddy(input.UV);

		float3 tangent = normalize(q1 * st2.x - q2 * st1.x);
		float3 bitangent = normalize(cross(normal, tangent));
  
		// calc tbn
        float3x3 tbn = float3x3(tangent, bitangent, normal);

        normal = normalize(mul(nmap, tbn));
	}

	float3 lightDirection = float3(-0.3f, 1.f, -0.7f);
	float diffuse = max(0.f, dot(-lightDirection, normal));

    return float4( LinearTosRGB(albedo.rgb + emissive.rgb), 1.f);
}