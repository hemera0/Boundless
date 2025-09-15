struct VertexData {
    float3 Position;
    float UVx;
    float3 Normal;
    float UVy;
};

struct SceneData {
    float4x4 CameraViewProjectionMatrix;
    float4 SunDirection;
    float4 SunColor;
};

struct _ {    
    vk::BufferPointer<SceneData> Scene;
    // This is a lifehack since HLSL doesn't support unsized arrays in structs...
    vk::BufferPointer<VertexData[1]> Vertices;     
};

[[vk::push_constant]] 
ConstantBuffer<_> PushConstants;

struct VS_Output {
	float4 Position : SV_POSITION;
	float3 Normal : NORMAL;
    float2 UV : TEXCOORD0;
};

VS_Output main(uint VertexIndex : SV_VertexID) {
    VertexData vertex = PushConstants.Vertices.Get()[VertexIndex];
    SceneData scene = PushConstants.Scene.Get();

    VS_Output res;
    res.Position = mul(scene.CameraViewProjectionMatrix, float4(vertex.Position.xyz, 1.f) );
    res.Normal = normalize(vertex.Normal.xyz);
    res.UV = float2(vertex.UVx, vertex.UVy);

    return res;
}