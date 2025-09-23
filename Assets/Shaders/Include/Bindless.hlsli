#define PUSH_CONSTANTS(T, N) [[vk::push_constant]] ConstantBuffer<T> N;

#define TEXTURE_POOL() \
    [[vk::binding(0, 0)]] Texture1D<float4> TexturePool1D[]; \
    [[vk::binding(0, 0)]] Texture2D<float4> TexturePool2D[]; \
    [[vk::binding(0, 0)]] TextureCube TexturePoolCube[]; \
    [[vk::binding(0, 0)]] SamplerState TexturePoolSamplers[]; \

#define RTAS_POOL() \
    [[vk::binding(1, 0)]] RaytracingAccelerationStructure AccelerationStructures[];

#define TEXTURE_SAMPLE1D( Index, UV ) TexturePool1D[ Index ].Sample( TexturePoolSamplers[Index], UV )
#define TEXTURE_SAMPLE2D( Index, UV ) TexturePool2D[ Index ].Sample( TexturePoolSamplers[Index], UV )
#define TEXTURE_SAMPLECUBE( Index, UVW ) TexturePoolCube[ Index ].Sample( TexturePoolSamplers[Index], UVW )

#define TEXTURE_GETCUBE( Index ) TexturePoolCube[ Index ]