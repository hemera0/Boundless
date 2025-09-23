#define M_PI 3.14159265359

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