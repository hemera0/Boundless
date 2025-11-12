#ifndef _TYPES_HLSLI_
#define _TYPES_HLSLI_
#include "Bindless.hlsli"
#include "Material.hlsli"

struct FullScreenPSInput {
    float4 Position : SV_POSITION;
    float2 UV : TEXCOORD0;
};

struct Vertex {
    float3 Position;
    float UVx;
    float3 Normal;
    float UVy;
    float4 Tangent;
};

struct SceneData {
    float4x4 CameraViewProjectionMatrix;
    float4x4 CameraInvViewProjectionMatrix;
    float4 CameraPosition;
    float4 SunDirection;
    float4 SunColor;
    int4 IblTextures;
};

struct GBufferPushConstants {
    vk::BufferPointer<SceneData> Scene;
	vk::BufferPointer<Material[1]> Materials;
	vk::BufferPointer<Vertex[1]> Vertices;
    float4x4 WorldTransform;
	uint MaterialIndex;
};

struct GBufferDebugPushConstants {
    int GBufferTexture;
    int GBufferDepthTexture;
    int GBufferChannelIndex;
};

struct LightingPushConstants {
    vk::BufferPointer<SceneData> Scene;
    int GBufferTexture;
    int GBufferDepthTexture;
};

struct CompositePushConstants {
    int Texture;
    int ApplyGammaCurve;
    int ApplyTonemapping;
};

struct BlitPushConstants {
    int Texture;
};

struct SkinningPushConstants {
	vk::BufferPointer<Vertex[1]>   VertexBuffer;
	vk::BufferPointer<Vertex[1]>   SkinnedVertexBuffer;
	vk::BufferPointer<uint4[1]>    BoneIndicesBuffer;
	vk::BufferPointer<float4[1]>   BoneWeightsBuffer;
	vk::BufferPointer<float4x4[1]> BoneTransformsBuffer;
	uint		                   VertexCount;
};
#endif