#include "Include/Types.hlsli"
#include "Include/Util.hlsli"

PUSH_CONSTANTS(BlitPushConstants, pc);
TEXTURE_POOL()

float4 main(FullScreenPSInput input) : SV_Target0 {
    float3 color = TEXTURE_SAMPLE2D(pc.Texture, SAMPLER_ANISO_CLAMP, input.UV).rgb;
    return float4(color, 1.f);
}