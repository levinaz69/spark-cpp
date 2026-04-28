#version 430 core

// Spark Gaussian Splat vertex shader
// Ported from sparkjsdev/spark splatVertex.glsl

// --- Includes (inlined) ---
// splat_defines.glsl is prepended by the shader manager

out vec4 vRgba;
out vec2 vSplatUv;
out vec3 vNdc;
flat out uint vSplatIndex;
flat out float adjustedStdDev;

uniform vec2 renderSize;
uniform vec4 renderToViewQuat;
uniform vec3 renderToViewPos;
uniform float maxStdDev;
uniform float minPixelRadius;
uniform float maxPixelRadius;
uniform float time;
uniform float minAlpha;
uniform float clipXY;
uniform float focalAdjustment;
uniform float blurAmount;
uniform float preBlurAmount;
uniform bool lodInflate;

uniform mat4 projectionMatrix;

uniform usampler2D ordering;
uniform usampler2DArray splatTexture;

// Quad vertex positions (set per-vertex)
layout(location = 0) in vec3 position;

void main() {
    // Default: outside frustum
    gl_Position = vec4(0.0, 0.0, 2.0, 1.0);

    // Look up splat index from ordering texture
    ivec2 orderingCoord = ivec2((gl_InstanceID >> 2) & 4095, gl_InstanceID >> 14);
    uint splatIndex = texelFetch(ordering, orderingCoord, 0)[gl_InstanceID & 3];
    if (splatIndex == 0xffffffffu) return;

    // Read packed splat data
    ivec3 texCoord = splatTexCoord(int(splatIndex));
    uvec4 packed = texelFetch(splatTexture, texCoord, 0);

    vec3 center, scales;
    vec4 quaternion, rgba;
    unpackSplatEncoding(packed, center, scales, quaternion, rgba,
                        vec4(0.0, 1.0, LN_SCALE_MIN, LN_SCALE_MAX));

    if (all(equal(scales, vec3(0.0)))) return;

    // Opacity: packed as 0..0.5 range, multiply by 2
    rgba.a *= 2.0;
    if (rgba.a == 0.0 || rgba.a < minAlpha) return;

    adjustedStdDev = maxStdDev;
    if (rgba.a > 1.0) {
        rgba.a = min(rgba.a * 4.0 - 3.0, 5.0);
        if (lodInflate) {
            float opacity = exp((rgba.a * rgba.a - 1.0) / 2.718281828459045);
            float rescale = pow(opacity, 1.0 / 3.0);
            scales *= rescale;
            rgba.a = 1.0;
        }
        adjustedStdDev = maxStdDev + 0.7 * (rgba.a - 1.0);
    }

    // View-space center
    vec3 viewCenter = quatVec(renderToViewQuat, center) + renderToViewPos;
    if (viewCenter.z >= 0.0) return;

    // Clip-space center
    vec4 clipCenter = projectionMatrix * vec4(viewCenter, 1.0);
    if (abs(clipCenter.z) >= clipCenter.w) return;

    float clip = clipXY * clipCenter.w;
    if (abs(clipCenter.x) > clip || abs(clipCenter.y) > clip) return;

    vRgba = rgba;
    vSplatUv = position.xy * adjustedStdDev;
    vSplatIndex = splatIndex;

    // Compute view-space quaternion
    vec4 viewQuaternion = quatQuat(renderToViewQuat, quaternion);

    // Compute 2D covariance from 3D gaussian projection
    vec3 viewScales = scales;
    mat3 R = mat3(
        quatVec(viewQuaternion, vec3(viewScales.x, 0, 0)),
        quatVec(viewQuaternion, vec3(0, viewScales.y, 0)),
        quatVec(viewQuaternion, vec3(0, 0, viewScales.z))
    );

    float fx = projectionMatrix[0][0];
    float fy = projectionMatrix[1][1];
    float izz = 1.0 / viewCenter.z;

    mat3 J = mat3(
        fx * izz, 0.0, 0.0,
        0.0, fy * izz, 0.0,
        -fx * viewCenter.x * izz * izz, -fy * viewCenter.y * izz * izz, 0.0
    );

    mat3 T = J * R;
    mat3 cov2D_mat = T * transpose(T);

    float a = cov2D_mat[0][0] + preBlurAmount + blurAmount;
    float b = cov2D_mat[0][1];
    float c = cov2D_mat[1][1] + preBlurAmount + blurAmount;

    // Eigenvalue decomposition of 2x2 covariance
    float det = a * c - b * b;
    if (det <= 0.0) return;
    float trace = a + c;
    float disc = sqrt(max(0.25 * trace * trace - det, 0.0));
    float lambda1 = max(0.5 * trace + disc, 0.0);
    float lambda2 = max(0.5 * trace - disc, 0.0);

    float radius1 = adjustedStdDev * sqrt(lambda1) * focalAdjustment;
    float radius2 = adjustedStdDev * sqrt(lambda2) * focalAdjustment;

    // Pixel radii
    float pixelRadius1 = radius1 * renderSize.x * 0.5;
    float pixelRadius2 = radius2 * renderSize.y * 0.5;

    if (max(pixelRadius1, pixelRadius2) < minPixelRadius) return;
    if (min(pixelRadius1, pixelRadius2) > maxPixelRadius) return;

    // Compute eigenvectors for oriented quad
    float angle;
    if (abs(b) > 1e-10) {
        angle = atan(lambda1 - a, b);
    } else {
        angle = (a >= c) ? 0.0 : PI * 0.5;
    }

    float cosA = cos(angle), sinA = sin(angle);
    vec2 axis1 = vec2(cosA, sinA) * radius1;
    vec2 axis2 = vec2(-sinA, cosA) * radius2;

    vec2 ndcCenter = clipCenter.xy / clipCenter.w;
    vec2 offset = position.x * axis1 + position.y * axis2;
    vec2 ndcPos = ndcCenter + offset;

    gl_Position = vec4(ndcPos * clipCenter.w, clipCenter.z, clipCenter.w);
    vNdc = vec3(ndcPos, clipCenter.z / clipCenter.w);
}
