#include "Include/Types.hlsli"

struct PS_Input {
	float4 Position : SV_POSITION;
	float3 Normal : NORMAL;
    float2 UV : TEXCOORD0;
};

PUSH_CONSTANTS(MainPassPushConstants, PushConstants);
TEXTURE_POOL()

float4 main(PS_Input input) : SV_Target0 {
    Material mat = PushConstants.Materials.Get()[PushConstants.MaterialIndex];

    float4 albedo = TEXTURE_SAMPLE2D(mat.AlbedoTexture, input.UV);
    // if(mat.Albedo.a < mat.AlphaCutoff)
    //     discard;

    return float4(albedo.rgb, 1.f);
}