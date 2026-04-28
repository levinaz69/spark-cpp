#version 430 core

// Spark Gaussian Splat fragment shader
// Ported from sparkjsdev/spark splatFragment.glsl

in vec4 vRgba;
in vec2 vSplatUv;
in vec3 vNdc;
flat in uint vSplatIndex;
flat in float adjustedStdDev;

uniform float minAlpha;
uniform bool encodeLinear;
uniform float falloff;
uniform bool premultipliedAlpha;

out vec4 fragColor;

// sRGB -> Linear
vec3 srgbToLinear(vec3 rgb) { return pow(rgb, vec3(2.2)); }

void main() {
    vec4 rgba = vRgba;

    float z2 = dot(vSplatUv, vSplatUv);
    if (z2 > (adjustedStdDev * adjustedStdDev)) {
        discard;
    }

    // Gaussian falloff
    if (rgba.a <= 1.0) {
        rgba.a = mix(rgba.a, rgba.a * exp(-0.5 * z2), falloff);
    } else {
        float a = exp((rgba.a * rgba.a - 1.0) / 2.718281828459045);
        float alpha = 1.0 - pow(1.0 - exp(-0.5 * z2), a);
        rgba.a = mix(1.0, alpha, falloff);
    }

    if (rgba.a < minAlpha) {
        discard;
    }

    if (encodeLinear) {
        rgba.rgb = srgbToLinear(rgba.rgb);
    }

    if (premultipliedAlpha) {
        fragColor = vec4(rgba.rgb * rgba.a, rgba.a);
    } else {
        fragColor = rgba;
    }
}
