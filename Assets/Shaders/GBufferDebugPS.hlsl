#include "Include/Types.hlsli"
#include "Include/Util.hlsli"

PUSH_CONSTANTS(GBufferDebugPushConstants, pc);
TEXTURE_POOL()

float4 main(FullScreenPSInput input) : SV_Target0 {
    Texture2D gbuffer = GET_TEXTURE2D(pc.GBufferTexture);

    uint4 packed = asuint(gbuffer[input.Position.xy]);

    float4 color = float4(0.f, 0.f, 0.f, 1.f);
    if(pc.GBufferChannelIndex == 0 ) {
        color = UnpackAlbedo(packed);
        color.a = 1.f;
    }

    if(pc.GBufferChannelIndex == 1) {
        color.rgb = UnpackNormal(packed);
        color.a = 1.f;
    }

    if(pc.GBufferChannelIndex == 2 || pc.GBufferChannelIndex == 3) {
        float2 val = UnpackMetallicRoughness(packed);

        if(pc.GBufferChannelIndex == 2 )
            color.rgb = float3(val.x, val.x, val.x);
        if(pc.GBufferChannelIndex == 3)
            color.rgb = float3(val.y, val.y, val.y);

        color.a = 1.f;
    }

    // float depth = depthBuffer.Sample(TexturePoolSamplers[ SAMPLER_ANISO_CLAMP ], input.UV).r;
    return color; // float4(color, 1.f);
}