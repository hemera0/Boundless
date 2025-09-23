#include "Include/Types.hlsli"
#include "Include/Util.hlsli"

struct PS_Input {
    float4 Position : SV_POSITION;
    float2 UV : TEXCOORD0;
};

PUSH_CONSTANTS(CompositePassPushConstants, pc);
TEXTURE_POOL()

float4 main(PS_Input input) : SV_Target0 {
    float3 color = TEXTURE_SAMPLE2D(pc.FrameBufferTexture, input.UV).rgb;

    color = ApplyACES(color);
    color = ApplyGamma(color);

    return float4(color, 1.f);
}