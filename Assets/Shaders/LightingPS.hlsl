#include "Include/Types.hlsli"
#include "Include/PBR.hlsli"

PUSH_CONSTANTS(LightingPushConstants, pc);
TEXTURE_POOL()

RTAS_POOL()

static RaytracingAccelerationStructure TLAS = AccelerationStructures[0];

float4 main(FullScreenPSInput input) : SV_Target0 {
    SceneData scene = pc.Scene.Get();

	// Indirect Lighting Indices
	int diffuseMap = scene.IblTextures.x;
	int specularMap = scene.IblTextures.y;
	int brdfLut = scene.IblTextures.z;

    // Unpack GBuffer Data 
    Texture2D gbuffer = GET_TEXTURE2D(pc.GBufferTexture);
    uint4 packed = asuint(gbuffer[input.Position.xy]);
    float4 albedo = UnpackAlbedo(packed);
    float3 normal = UnpackNormal(packed);
    float2 metallicRoughness = UnpackMetallicRoughness(packed);
    float3 emissive = float3(0.f, 0.f, 0.f); // TODO

    Texture2D depthBuffer = GET_TEXTURE2D(pc.GBufferDepthTexture);
    float depth = depthBuffer[input.Position.xy].r;
    float3 worldPos = ReconstructWorldPosition(input.UV, depth, scene.CameraInvViewProjectionMatrix);

    if (depth == 1.f) 
    {    
        float3 skyColor = TexturePoolCube[ specularMap ].SampleLevel(TexturePoolSamplers[ SAMPLER_LINEAR_CLAMP ], worldPos, 0);
        return float4(max(skyColor, 0.0.xxx), 1.0);
    }

    PBRMaterial material;
	material.LoadData(albedo.rgb, normal, metallicRoughness.x, metallicRoughness.y);
	
	float3 wi = normalize(-scene.SunDirection.xyz);
    float3 wo = normalize(scene.CameraPosition.xyz - worldPos);
    float3 radiance = emissive.rgb;

    // Rayquery shadows.
	RayDesc ray;
    ray.TMin = 0.01f;
    ray.TMax = 1000.0;
    ray.Origin = worldPos;
    ray.Direction = wi;

    uint rayFlags = RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER;

    RayQuery<RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER> query;
    query.TraceRayInline(TLAS, rayFlags, 0xFF, ray);
    query.Proceed();

	bool hit = query.CommittedStatus() == COMMITTED_TRIANGLE_HIT;

    // Directional Light 
    radiance += material.CalculateDirectionalLight(wi, wo, scene.SunColor.rgb, hit);

	float iblIntensity = 0.5f;

	radiance += material.CalculateIndirectLight( 
		TexturePoolSamplers[ SAMPLER_ANISO_CLAMP ], 
		TexturePoolSamplers[ SAMPLER_LINEAR_CLAMP ], 
		TexturePoolCube[ diffuseMap ], 
		TexturePoolCube[ specularMap ], 
		TexturePool2D[ brdfLut ], 
		wo
	) * iblIntensity;

    return float4( radiance, 1.f );
}