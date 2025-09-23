#include "Include/Types.hlsli"
#include "Include/PBR.hlsli"

struct PS_Input {
	float4 Position : SV_POSITION;
    float4 WorldPos : POSITION;
	float3 Normal : NORMAL;
	float4 Tangent : TANGENT;
    float2 UV : TEXCOORD0;
};

PUSH_CONSTANTS(MainPassPushConstants, PushConstants);
TEXTURE_POOL()
RTAS_POOL()

static RaytracingAccelerationStructure TLAS = AccelerationStructures[0];

float4 main(PS_Input input) : SV_Target0 {
    Material mat = PushConstants.Materials.Get()[PushConstants.MaterialIndex];
	SceneData scene = PushConstants.Scene.Get();

    float4 albedo = mat.Albedo;    
    if(mat.AlbedoTexture)
        albedo *= TEXTURE_SAMPLE2D(mat.AlbedoTexture, input.UV);

    float4 emissive = mat.Emissive;
    if(mat.EmissiveTexture)
        emissive *= TEXTURE_SAMPLE2D(mat.EmissiveTexture, input.UV);

	float3 normal = normalize(input.Normal);
	if(mat.NormalsTexture) {
		float3 nmap = TEXTURE_SAMPLE2D(mat.NormalsTexture, input.UV).rgb * 2.f - 1.f;

		// Calc tangent
		// I think this method is good enough but could switch to calculating these using mikktspace.
		// float3 q1 = ddx(input.WorldPos.xyz);
		// float3 q2 = ddy(input.WorldPos.xyz);
		// float2 st1 = ddx(input.UV);
		// float2 st2 = ddy(input.UV);

		// float3 tangent = normalize(q1 * st2.x - q2 * st1.x);
		float3 tangent = normalize(input.Tangent.xyz);
		float3 bitangent = normalize(cross(normal, tangent) * input.Tangent.w);  
       	float3x3 tbn = float3x3(tangent, bitangent, normal);

       	normal = normalize(mul(nmap, tbn));
	}

	float metallic = 0.f;
	float roughness = 1.f;	
	if(mat.MetalRoughnessTexture) {
		float3 mr = TEXTURE_SAMPLE2D(mat.MetalRoughnessTexture, input.UV).rgb;
		metallic = mr.b * mat.MetallicFactor;
		roughness = mr.g * mat.RoughnessFactor;
	}

    if(mat.Albedo.a <= mat.AlphaCutoff)
        discard;

	// PBR Lighting
	PBRMaterial material;
	material.LoadData(albedo.rgb, normal, metallic, roughness);
	
	float3 wi = normalize(-scene.SunDirection.xyz);
    float3 wo = normalize(scene.CameraPosition.xyz - input.WorldPos.xyz );
    float3 radiance = emissive.rgb;

	// RayQuery shadows...
	RayDesc ray;
    ray.TMin = 0.001f;
    ray.TMax = 1000.0;
    ray.Origin = input.WorldPos.xyz;	
    ray.Direction = wi;

    uint ray_flags = RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER;
    RayQuery< RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER > query;

    query.TraceRayInline(TLAS, ray_flags, 0xFF, ray);
    query.Proceed();

	bool hit = query.CommittedStatus() == COMMITTED_TRIANGLE_HIT;

    // Directional Light 
    radiance += material.CalculateDirectionalLight(wi, wo, scene.SunColor.rgb, hit);
	
	// Indirect Light
	int diffuseMap = scene.IblTextures.x;
	int specularMap = scene.IblTextures.y;
	int brdfLut = scene.IblTextures.z;

	float iblIntensity = 0.75f;

	radiance += material.CalculateIndirectLight( 
		TexturePoolSamplers[ diffuseMap ], 
		TexturePoolSamplers[ brdfLut ], 
		TexturePoolCube[ diffuseMap ], 
		TexturePoolCube[ specularMap ], 
		TexturePool2D[ brdfLut ], 
		wo
	);

	// normal * 0.5f + 0.5f
    return float4( radiance, 1.f );
}