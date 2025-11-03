#define M_PI 3.14159265359

float3 ReconstructWorldPosition(float2 uv, float depth, float4x4 invViewProj) {
    float x = uv.x * 2.0f - 1.0f;
    float y = ( 1.0f - uv.y ) * 2.f - 1.0f; 
    float4 position_s = float4(x, y, depth, 1.0f);
    float4 position_v = mul(invViewProj, position_s);
    return position_v.xyz / position_v.w; 
}

uint Float16x2ToUint( float2 val ) {
    return uint(val.x * 65535) | uint(val.y * 65535) << 16;
}

uint Float4ToUint( float4 val ) {
    uint g = uint(saturate(val.g) * 255.0);
    uint r = uint(saturate(val.r) * 255.0);
    uint b = uint(saturate(val.b) * 255.0);
    uint a = uint(saturate(val.a) * 255.0);
    return (r) | (g << 8) | (b << 16) | (a << 24);    
}

uint Float3ToUint( float3 val ) {
    uint bitmask10 = (1 << 10) - 1;
    uint bitmask11 = (1 << 11) - 1;
    return uint(val.x * bitmask11) | uint(val.y * bitmask11) << 11 | uint(val.z * bitmask10) << 22;
}

float2 UintToFloat16x2(uint val) {
    float2 unpacked = float2(val & 65535, (val >> 16) & 65535);
    return unpacked / 65535;
}

float4 UintToFloat4( uint packed ) {
    float4 unpacked = float4(0.f, 0.f, 0.f, 0.f);
    unpacked.r = ((packed >> 0)  & 0xFF);
    unpacked.g = ((packed >> 8)  & 0xFF);
    unpacked.b = ((packed >> 16) & 0xFF);
    unpacked.a = ((packed >> 24) & 0xFF);
    return unpacked / 255.f;
}

float3 UintToFloat3(uint packed) {
    uint bitmask10 = (1 << 10) - 1;
    uint bitmask11 = (1 << 11) - 1;
    float3 unpacked = float3((packed) & bitmask11, ((packed >> 11) & bitmask11), ((packed >> 22) & bitmask10));
    return unpacked / uint3(bitmask11, bitmask11, bitmask10);
}

void PackAlbedo(float4 albedo, inout uint4 packed) {
    packed.x = Float4ToUint(albedo);
}

void PackNormal(float3 normal, inout uint4 packed) {
    packed.y = Float3ToUint(normalize(normal) * 0.5f + 0.5f);
}

void PackMetallicRoughness(float metallic, float roughness, inout uint4 packed) {
    packed.z = Float16x2ToUint(float2(metallic, roughness));
}

float4 UnpackAlbedo(uint4 packed) {
    return UintToFloat4(packed.x);
}

float3 UnpackNormal(uint4 packed) {
    return UintToFloat3(packed.y) * 2.f - 1.f;
}

float2 UnpackMetallicRoughness(uint4 packed) {
    return UintToFloat16x2(packed.z);
}

float3 ApplyGamma(float3 c) {
	return pow(c, (1.f/2.2f).xxx);
}

float3 ApplyACES(float3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return saturate((x * (a * x + b)) / (x * (c * x + d ) + e));
}