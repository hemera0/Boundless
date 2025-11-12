#include "Include/Types.hlsli"
#include "Include/Util.hlsli"

PUSH_CONSTANTS(SkinningPushConstants, pc)

[numthreads(64, 1, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID, uint3 groupID : SV_GroupID, uint3 groupThreadID : SV_GroupThreadID, uint groupIndex : SV_GroupIndex) {
    uint vertexIndex = dispatchThreadID.x;
    if ( vertexIndex > pc.VertexCount )
        return;

    uint4 bone = pc.BoneIndicesBuffer.Get()[vertexIndex];
    float4 weight = pc.BoneWeightsBuffer.Get()[vertexIndex];

    float4x4 boneTransform  = mul( pc.BoneTransformsBuffer.Get()[ bone[ 0 ] ], weight[ 0 ] );
             boneTransform += mul( pc.BoneTransformsBuffer.Get()[ bone[ 1 ] ], weight[ 1 ] );
             boneTransform += mul( pc.BoneTransformsBuffer.Get()[ bone[ 2 ] ], weight[ 2 ] );
             boneTransform += mul( pc.BoneTransformsBuffer.Get()[ bone[ 3 ] ], weight[ 3 ] );

    Vertex vertex   = pc.VertexBuffer.Get()[vertexIndex];
    vertex.Position = mul(float4(vertex.Position, 1.f), boneTransform).xyz;
    vertex.Normal   = mul(float4(vertex.Normal, 0.f), boneTransform).xyz;
    vertex.Tangent  = mul(vertex.Tangent, boneTransform);
    
    pc.SkinnedVertexBuffer.Get()[vertexIndex] = vertex;
}