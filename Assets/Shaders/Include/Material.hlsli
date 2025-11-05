#ifndef _MATERIAL_HLSLI_
#define _MATERIAL_HLSLI_
struct Material {
	int Index;
	float3 Pad0;
	
	float MetallicFactor;
	float RoughnessFactor;
	uint AlphaMode;
	float AlphaCutoff;

    int AlbedoTexture;
	int NormalsTexture;
	int MetalRoughnessTexture;
	int EmissiveTexture;

	float4 Albedo;
	float4 Emissive;
};
#endif