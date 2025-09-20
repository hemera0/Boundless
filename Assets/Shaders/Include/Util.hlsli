#define M_PI 3.14159265359
float3 sRGBToLinearFast(float3 c) {
    return float3(pow(c, float3(2.2f, 2.2f, 2.2f)));
}

float3 sRGBToLinear(float3 c) {
    float3 x = c / 12.92f;
    float3 y = pow(max(( c + 0.055f) / 1.055f, 0.0f), 2.4f);

    float3 clr = c;
    clr.r = c.r <= 0.04045f ? x.r : y.r;
    clr.g = c.g <= 0.04045f ? x.g : y.g;
    clr.b = c.b <= 0.04045f ? x.b : y.b;

    return clr;
}

float4 sRGBToLinear(float4 c) {
	return float4(sRGBToLinear(c.rgb), c.a);
}

float3 LinearTosRGBFast(float3 c) {
    const float x = 1.f/2.2f;
	return float3(pow(c, float3(x, x, x)));
}

float3 LinearTosRGB(float3 c) {
    float3 x = c * 12.92f;
    float3 y = 1.055f * pow(saturate(c), 1.0f / 2.4f) - 0.055f;

    float3 clr = c;
    clr.r = c.r < 0.0031308f ? x.r : y.r;
    clr.g = c.g < 0.0031308f ? x.g : y.g;
    clr.b = c.b < 0.0031308f ? x.b : y.b;

    return clr;
}

float4 LinearTosRGB(float4 c) {
	return float4(LinearTosRGB(c.rgb), c.a);
}