#include "Util.hlsli"

float DistributionGGX(float NdotH, float roughness) {
    float alpha   = roughness * roughness;
    float alphaSq = alpha * alpha;
    float denom   = (NdotH * NdotH) * (alphaSq - 1.f) + 1.f;
    return alphaSq / (M_PI * denom * denom) + 0.001f;
}

float3 FresnelSchlick(float3 f0, float f90, float NdotV) {
    return f0 + (f90 - f0) * pow(1.f - NdotV, 5.f);
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
        
        float3 specular = (F * D * G) / 4.f * NdotL * NdotV + 0.001f;
        float3 diffuse  = ((1.f - m_Metallic) * m_Albedo) * (1.f - F);
        return diffuse + specular;
    }

    float3 CalculateDirectionalLight(float3 wi, float3 wo, float3 lightColor) {
        float NdotL = max(0.f, dot(m_Normal, wi)); 
		return EvalBRDF(wi, wo) * NdotL * lightColor;
    }
};