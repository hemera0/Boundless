#include "Util.hlsli"

float DistributionGGX(float NdotH, float roughness) {
    float alpha   = roughness * roughness;
    float alphaSq = alpha * alpha;
    float denom   = (NdotH * NdotH) * (alphaSq - 1.f) + 1.f;
    return alphaSq / (M_PI * denom * denom) + 0.001f;
}

float3 FresnelSchlick(float3 F0, float F90, float NdotV) {
    return F0 + (F90 - F0) * pow(1.f - NdotV, 5.f);
}

float3 FresnelSchlickRoughness(float3 F0, float NdotV, float roughness) {
    return F0 + (max(float3(1.0.xxx - roughness), F0) - F0) * pow(clamp(1.0 - NdotV, 0.0, 1.0), 5.0);
}

float GGX_Schlick(float NdotV, float roughness) {
    float r = (roughness + 1.f);
    float k = (r*r) / 8.f;
    float denom = NdotV * (1.f - k) + k;
    return NdotV / (denom + 0.001f);
}

float GeometrySmith(float NdotV, float NdotL, float roughness) {
    float ggx1 = GGX_Schlick(NdotL, roughness);
    float ggx2 = GGX_Schlick(NdotV, roughness);
    return ggx1 * ggx2;
}

struct PBRMaterial {
    float3 m_Albedo;
    float3 m_Normal;
    float m_Metallic;
    float m_Roughness;

    void LoadData(float3 albedo, float3 normal, float metallic, float roughness) {
        m_Albedo    = albedo;
        m_Normal    = normal;
        m_Metallic  = metallic;
        m_Roughness = roughness;
    }

    float3 EvalBRDF(float3 wi, float3 wo) {
        float3 wh   = normalize(wi + wo);
        float NdotL = max(0.f, dot(m_Normal, wi));
        float NdotV = max(0.f, dot(m_Normal, wo));
        float NdotH = max(0.f, dot(m_Normal, wh));
        float VdotH = max(0.f, dot(wo, wh));

        float3 F0 = lerp(0.04f, m_Albedo, m_Metallic);
        float3  F = FresnelSchlick(F0, 1.f, VdotH);
        float   D = DistributionGGX(NdotH, m_Roughness);
        float   G = GeometrySmith(NdotV, NdotL, m_Roughness);
        
        float3 kd = (1.0 - F) * (1.0 - m_Metallic);
        float3 specular = (F * D * G) / 4.f * NdotL * NdotV + 0.001f;
        float3 diffuse = ( kd * m_Albedo );

        return diffuse + specular;
    }

    float3 CalculateDirectionalLight(float3 wi, float3 wo, float3 lightColor, bool rayHit) {
        float NdotL = max(0.f, dot(m_Normal, wi)); 

        NdotL *= rayHit ? 0.1f : 1.f;

		return EvalBRDF(wi, wo) * NdotL * lightColor;
    }

    // TODO: Swap this out for some real GI...
    // Theres a lot of stuff that causes issues with this...
    float3 CalculateIndirectLight(SamplerState iblSampler, SamplerState brdfSampler, TextureCube diffuseMap, TextureCube specularMap, Texture2D<float4> brdfLut, float3 wo) {
        float3 diffuseColor = m_Albedo * (1.0 - 0.04f) * (1.0 - m_Metallic);
        float3 F0 = lerp(0.04f, m_Albedo, m_Metallic);
    
        float NdotV = max(0.f, dot(m_Normal, wo));

        float2 brdf = brdfLut.Sample(brdfSampler, float2(NdotV, m_Roughness)).xy;
        
        uint width, height, specularTextureLevels;
        specularMap.GetDimensions(0, width, height, specularTextureLevels);

        float lod = m_Roughness * ( specularTextureLevels );

        float3 R = normalize( reflect(-wo, m_Normal) );
        float3 radiance = specularMap.SampleLevel(iblSampler, R, lod).xyz;
        float3 irradiance = diffuseMap.SampleLevel(iblSampler, m_Normal, 0).xyz;

        float3 ks = FresnelSchlickRoughness(F0, NdotV, m_Roughness);
        float3 FssEss = ks * brdf.x + brdf.y;

        float Ems = (1.0 - (brdf.x + brdf.y));
        float3 F_avg = F0 + (1.0 - F0) / 21.0;
        float3 FmsEms = Ems * FssEss * F_avg / (1.0 - F_avg * Ems);
        float3 kd = diffuseColor * (1.0 - FssEss - FmsEms);
        
        return FssEss * radiance + (FmsEms + kd) * irradiance;
    }
};