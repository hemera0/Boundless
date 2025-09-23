#include "Include/Types.hlsli"

struct VS_Output {
	float4 Position : SV_POSITION;
    float4 WorldPos : POSITION;
	float3 Normal : NORMAL;
    float4 Tangent : TANGENT;
    float2 UV : TEXCOORD0;
};

PUSH_CONSTANTS(MainPassPushConstants, PushConstants);

VS_Output main(uint VertexIndex : SV_VertexID) {
    Vertex vertex = PushConstants.Vertices.Get()[VertexIndex];
    SceneData scene = PushConstants.Scene.Get();

    VS_Output res;

    res.Position = float4(vertex.Position.xyz, 1.f);
    // res.Position = mul(PushConstants.ModelMatrix, res.Position);
    res.WorldPos = res.Position;
    
    res.Tangent = vertex.Tangent;
    res.Position = mul(scene.CameraViewProjectionMatrix, res.Position);
    res.Normal = normalize(vertex.Normal.xyz);
    res.UV = float2(vertex.UVx, vertex.UVy);

    return res;
}