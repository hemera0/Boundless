struct Material {
	int Index;

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