struct PS_Input {
	float4 Position : SV_POSITION;
	float3 Normal : NORMAL;
    float2 UV : TEXCOORD0;
};

[[vk::binding(0, 0)]]
Texture2D TexturePool[];

[[vk::binding(0, 0)]]
SamplerState TexturePoolSamplers[];

float4 main(PS_Input input) : SV_Target0 {
    return TexturePool[0].Sample(TexturePoolSamplers[0], input.UV);
}