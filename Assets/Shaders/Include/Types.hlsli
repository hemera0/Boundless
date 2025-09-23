#include "Bindless.hlsli"
#include "Material.hlsli"

struct Vertex {
    float3 Position;
    float UVx;
    float3 Normal;
    float UVy;
    float4 Tangent;
};

struct SceneData {
    float4x4 CameraViewProjectionMatrix;
    float4 CameraPosition;
    float4 SunDirection;
    float4 SunColor;
    int4 IblTextures;
};

struct MainPassPushConstants {
    vk::BufferPointer<SceneData> Scene;
    vk::BufferPointer<Material[1]> Materials;
    vk::BufferPointer<Vertex[1]> Vertices;
    // uint TLASIndex;
    // float4x4 ModelMatrix;
    uint MaterialIndex; 
};

struct CompositePassPushConstants {
    int FrameBufferTexture;
};