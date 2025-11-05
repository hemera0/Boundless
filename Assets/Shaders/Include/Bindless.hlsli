#ifndef _BINDLESS_HLSLI_
#define _BINDLESS_HLSLI_
#define PUSH_CONSTANTS(T, N) [[vk::push_constant]] ConstantBuffer<T> N;

#define TEXTURE_POOL() \
    [[vk::binding(0, 0)]] Texture1D<float4> TexturePool1D[]; \
    [[vk::binding(0, 0)]] Texture2D<float4> TexturePool2D[]; \
    [[vk::binding(0, 0)]] TextureCube TexturePoolCube[]; \
    [[vk::binding(2, 0)]] SamplerState TexturePoolSamplers[]; \

#define RTAS_POOL() \
    [[vk::binding(1, 0)]] RaytracingAccelerationStructure AccelerationStructures[];

#define GET_TEXTURE2D( Index ) TexturePool2D[ Index ]
#define TEXTURE_SAMPLE1D( Index, SamplerIndex, UV ) TexturePool1D[ Index ].Sample( TexturePoolSamplers[SamplerIndex], UV )
#define TEXTURE_SAMPLE2D( Index, SamplerIndex, UV ) TexturePool2D[ Index ].Sample( TexturePoolSamplers[SamplerIndex], UV )
#define TEXTURE_SAMPLECUBE( Index, SamplerIndex, UVW ) TexturePoolCube[ Index ].Sample( TexturePoolSamplers[SamplerIndex], UVW )

#define SAMPLER_LINEAR_CLAMP 0
#define SAMPLER_ANISO_WRAP 1
#define SAMPLER_ANISO_CLAMP 2
#endif