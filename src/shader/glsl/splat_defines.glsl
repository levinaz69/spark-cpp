// Spark GLSL defines and utility functions
// Ported from sparkjsdev/spark splatDefines.glsl

const float LN_SCALE_MIN = -12.0;
const float LN_SCALE_MAX = 9.0;

const uint SPLAT_TEX_WIDTH_BITS = 11u;
const uint SPLAT_TEX_HEIGHT_BITS = 11u;
const uint SPLAT_TEX_DEPTH_BITS = 11u;
const uint SPLAT_TEX_LAYER_BITS = SPLAT_TEX_WIDTH_BITS + SPLAT_TEX_HEIGHT_BITS;

const uint SPLAT_TEX_WIDTH = 1u << SPLAT_TEX_WIDTH_BITS;
const uint SPLAT_TEX_HEIGHT = 1u << SPLAT_TEX_HEIGHT_BITS;
const uint SPLAT_TEX_DEPTH = 1u << SPLAT_TEX_DEPTH_BITS;

const uint SPLAT_TEX_WIDTH_MASK = SPLAT_TEX_WIDTH - 1u;
const uint SPLAT_TEX_HEIGHT_MASK = SPLAT_TEX_HEIGHT - 1u;
const uint SPLAT_TEX_DEPTH_MASK = SPLAT_TEX_DEPTH - 1u;

const uint F16_INF = 0x7c00u;
const float PI = 3.1415926535897932384626433832795;

float sqr(float x) { return x * x; }

vec3 srgbToLinear(vec3 rgb) { return pow(rgb, vec3(2.2)); }
vec3 linearToSrgb(vec3 rgb) { return pow(rgb, vec3(1.0 / 2.2)); }

// Index -> 3D texture coordinate
ivec3 splatTexCoord(int index) {
    return ivec3(
        index & int(SPLAT_TEX_WIDTH_MASK),
        (index >> int(SPLAT_TEX_WIDTH_BITS)) & int(SPLAT_TEX_HEIGHT_MASK),
        index >> int(SPLAT_TEX_LAYER_BITS)
    );
}

// Decode quaternion from 24-bit octahedral encoding
vec4 decodeQuatOctXy88R8(uint encoded) {
    uint quantU = encoded & 0xFFu;
    uint quantV = (encoded >> 8u) & 0xFFu;
    uint angleInt = encoded >> 16u;

    float u_f = float(quantU) / 255.0;
    float v_f = float(quantV) / 255.0;
    vec2 f = vec2(u_f * 2.0 - 1.0, v_f * 2.0 - 1.0);

    vec3 axis = vec3(f.xy, 1.0 - abs(f.x) - abs(f.y));
    float t = max(-axis.z, 0.0);
    axis.x += (axis.x >= 0.0) ? -t : t;
    axis.y += (axis.y >= 0.0) ? -t : t;
    axis = normalize(axis);

    float theta = (float(angleInt) / 255.0) * PI;
    float halfTheta = theta * 0.5;
    float s = sin(halfTheta);
    float w = cos(halfTheta);

    return vec4(axis * s, w);
}

// Quaternion multiplication: a * b
vec4 quatQuat(vec4 a, vec4 b) {
    return vec4(
        a.w * b.xyz + b.w * a.xyz + cross(a.xyz, b.xyz),
        a.w * b.w - dot(a.xyz, b.xyz)
    );
}

// Rotate vector by quaternion
vec3 quatVec(vec4 q, vec3 v) {
    vec3 t = 2.0 * cross(q.xyz, v);
    return v + q.w * t + cross(q.xyz, t);
}

// Unpack splat from packed encoding
void unpackSplatEncoding(uvec4 packed, out vec3 center, out vec3 scales,
                          out vec4 quaternion, out vec4 rgba,
                          vec4 encodingParams) {
    float rgbMin = encodingParams.x;
    float rgbMax = encodingParams.y;
    float lnScaleMin = encodingParams.z;
    float lnScaleMax = encodingParams.w;

    // RGBA
    rgba.r = mix(rgbMin, rgbMax, float(packed.x & 0xFFu) / 255.0);
    rgba.g = mix(rgbMin, rgbMax, float((packed.x >> 8u) & 0xFFu) / 255.0);
    rgba.b = mix(rgbMin, rgbMax, float((packed.x >> 16u) & 0xFFu) / 255.0);
    rgba.a = float((packed.x >> 24u) & 0xFFu) / 255.0;

    // Center (float16)
    center.x = unpackHalf2x16(packed.y).x;
    center.y = unpackHalf2x16(packed.y).y;
    center.z = unpackHalf2x16(packed.z).x;

    // Quaternion (octahedral 8+8+8)
    uint quatEncoded = ((packed.z >> 16u) & 0xFFu)
                     | (((packed.z >> 24u) & 0xFFu) << 8u)
                     | (((packed.w >> 24u) & 0xFFu) << 16u);
    quaternion = decodeQuatOctXy88R8(quatEncoded);

    // Scales
    float s0 = float(packed.w & 0xFFu);
    float s1 = float((packed.w >> 8u) & 0xFFu);
    float s2 = float((packed.w >> 16u) & 0xFFu);

    scales.x = (s0 == 0.0) ? 0.0 : exp(mix(lnScaleMin, lnScaleMax, (s0 - 1.0) / 254.0));
    scales.y = (s1 == 0.0) ? 0.0 : exp(mix(lnScaleMin, lnScaleMax, (s1 - 1.0) / 254.0));
    scales.z = (s2 == 0.0) ? 0.0 : exp(mix(lnScaleMin, lnScaleMax, (s2 - 1.0) / 254.0));
}
