#include "Include/Types.hlsli"

struct VS_Output {
    float4 Position : SV_POSITION;
    float2 UV : TEXCOORD0;
};

VS_Output main(uint VertexIndex : SV_VertexID) {
    VS_Output res;

    res.UV = float2((VertexIndex << 1) & 2, VertexIndex & 2);
	res.Position = float4(res.UV * 2.0f - 1.0f, 0.0f, 1.0f);

    return res;
}