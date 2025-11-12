#include "Include/Types.hlsli"

struct VS_Output {
	float4 Position : SV_POSITION;
    float4 WorldPos : POSITION;
	float3 Normal : NORMAL;
    float4 Tangent : TANGENT;
    float2 UV : TEXCOORD0;
};

PUSH_CONSTANTS(GBufferPushConstants, pc);

VS_Output main(uint VertexIndex : SV_VertexID) {
    Vertex vertex = pc.Vertices.Get()[VertexIndex];
    SceneData scene = pc.Scene.Get();

    VS_Output res;
    res.Position = mul(pc.WorldTransform, float4(vertex.Position.xyz, 1.f));
    res.WorldPos = res.Position;

    float3 normal = normalize(vertex.Normal.xyz);
    res.Tangent  = mul(pc.WorldTransform, vertex.Tangent);
    res.Normal   = mul(pc.WorldTransform, float4(normal, 0.f));

    res.Position = mul(scene.CameraViewProjectionMatrix, res.Position);
    res.UV = float2(vertex.UVx, vertex.UVy);

    return res;
}