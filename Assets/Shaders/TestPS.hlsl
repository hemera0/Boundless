#include "Include/Types.hlsli"
#include "Include/PBR.hlsli"

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
	SceneData scene = PushConstants.Scene.Get();

    float4 albedo = mat.Albedo;    
    if(mat.AlbedoTexture)
        albedo *= TEXTURE_SAMPLE2D(mat.AlbedoTexture, input.UV);

    float4 emissive = mat.Emissive;
    if(mat.EmissiveTexture)
        emissive *= TEXTURE_SAMPLE2D(mat.EmissiveTexture, input.UV);

    if(mat.Albedo.a <= mat.AlphaCutoff)
        discard;

	float3 normal = normalize(input.Normal);
	if(mat.NormalsTexture) {
		float3 nmap = TEXTURE_SAMPLE2D(mat.NormalsTexture, input.UV).rgb * 2.f - 1.f;

		// Calc tangent
		// I think this method is good enough but could switch to calculating these using mikktspace.
		float3 q1 = ddx(input.WorldPos);
		float3 q2 = ddy(input.WorldPos);
		float2 st1 = ddx(input.UV);
		float2 st2 = ddy(input.UV);

		float3 tangent = normalize(q1 * st2.x - q2 * st1.x);
		float3 bitangent = normalize(cross(normal, tangent));  
        float3x3 tbn = float3x3(tangent, bitangent, normal);

        normal = normalize(mul(nmap, tbn));
	}

	float metallic = 0.f;
	float roughness = 1.f;	
	if(mat.MetalRoughnessTexture) {
		float3 mro = TEXTURE_SAMPLE2D(mat.MetalRoughnessTexture, input.UV).rgb * 2.f - 1.f;
		roughness = mro.g * mat.RoughnessFactor;
		metallic = mro.b * mat.MetallicFactor;
	}

	// PBR Lighting
	PBRMaterial material;
	material.LoadData(albedo.rgb, normal, metallic, roughness);
	
	float3 wi = normalize(-scene.SunDirection.xyz);
    float3 wo = normalize(scene.CameraPosition.xyz - input.WorldPos.xyz);
    float3 radiance = emissive.rgb;

    // Directional Light 
    radiance += material.CalculateDirectionalLight(wi, wo, scene.SunColor.rgb);
	
	// TODO: IBL
	radiance += float3(0.05, 0.05, 0.05) * albedo.rgb;

    return float4( radiance, 1.f );
}