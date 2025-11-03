#include "Include/Types.hlsli"
#include "Include/PBR.hlsli"

struct PS_Input {
	float4 Position : SV_POSITION;
    float4 WorldPos : POSITION;
	float3 Normal : NORMAL;
	float4 Tangent : TANGENT;
    float2 UV : TEXCOORD0;
};

PUSH_CONSTANTS(GBufferPushConstants, pc);
TEXTURE_POOL()

float4 main(PS_Input input) : SV_Target0 {
    Material mat = pc.Materials.Get()[pc.MaterialIndex];
	SceneData scene = pc.Scene.Get();

    float4 albedo = mat.Albedo;    
    if(mat.AlbedoTexture)
        albedo *= TEXTURE_SAMPLE2D(mat.AlbedoTexture, SAMPLER_ANISO_WRAP, input.UV);

    if(albedo.a < mat.AlphaCutoff)
        discard;

    float4 emissive = mat.Emissive;
    if(mat.EmissiveTexture)
        emissive *= TEXTURE_SAMPLE2D(mat.EmissiveTexture, SAMPLER_ANISO_WRAP, input.UV);

	float3 normal = normalize(input.Normal);
	if(mat.NormalsTexture) {
		float3 nmap = TEXTURE_SAMPLE2D(mat.NormalsTexture, SAMPLER_ANISO_WRAP, input.UV).rgb * 2.f - 1.f;

		float3 tangent = normalize(input.Tangent.xyz);
		float3 bitangent = normalize(cross(normal, tangent) * input.Tangent.w);  
       	float3x3 tbn = float3x3(tangent, bitangent, normal);

       	normal = normalize(mul(nmap, tbn));
    }

	float metallic = 0.f;
	float roughness = 1.f;
	if(mat.MetalRoughnessTexture) {
		float3 mr = TEXTURE_SAMPLE2D(mat.MetalRoughnessTexture, SAMPLER_ANISO_WRAP, input.UV).rgb;
		metallic = mr.b * mat.MetallicFactor;
		roughness = mr.g * mat.RoughnessFactor;
	}

    uint4 packed = uint4(0, 0, 0, 0);
    PackAlbedo(albedo, packed);
    PackNormal(normal, packed);
    PackMetallicRoughness(metallic, roughness, packed);

    return asfloat(packed); 
}