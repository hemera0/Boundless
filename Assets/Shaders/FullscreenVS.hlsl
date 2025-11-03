#include "Include/Types.hlsli"

FullScreenPSInput main(uint VertexIndex : SV_VertexID) {
    FullScreenPSInput res;

    res.UV = float2((VertexIndex << 1) & 2, VertexIndex & 2);
	res.Position = float4(res.UV * 2.0f - 1.0f, 0.0f, 1.0f);

    return res;
}