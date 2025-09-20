#include "Bindless.hlsli"
#include "Material.hlsli"

struct Vertex {
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

struct MainPassPushConstants {
    vk::BufferPointer<SceneData> Scene;
    vk::BufferPointer<Material[1]> Materials;
    vk::BufferPointer<Vertex[1]> Vertices;
    uint MaterialIndex; 
};