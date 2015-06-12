#version 140

uniform sampler2D DepthTexture; 
uniform sampler2D ColorTexture;
uniform sampler2D NormalTexture;

uniform mat3 normalMatrix;
//uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;
uniform mat4 projectionInverseMatrix;

//uniform mat4 viewProjectionMatrix;

uniform float nearZ;
uniform float farZ;

uniform vec2 screenSize;


uniform sampler1D ssaoKernelSampler;
uniform sampler2D ssaoNoiseSampler;

uniform vec4 ssaoSamplerSizes; // expected: [kernelSize, 1 / kernelSize, noiseSize, 1 / noiseSize]
uniform float ssaoRadius;
uniform float ssaoIntensity;

vec3 kernel(const in float i)
{
    return texture(ssaoKernelSampler, i * ssaoSamplerSizes[1]).xyz;
}

vec3 normal(const in vec2 uv)
{
    return texture(NormalTexture, uv, 0).xyz;
}

// returns values in [nearZ:farZ]
float linearDepth(const in vec2 uv)
{
    float d = texture(DepthTexture, uv, 0).x;
    return projectionMatrix[3][2] / (d + projectionMatrix[2][2]);
}

mat3 noised(const in vec3 normal, in vec2 uv)
{
    uv *= screenSize * ssaoSamplerSizes[3];

    vec3 random = texture(ssaoNoiseSampler, uv).xyz;

    // orientation matrix
    vec3 t = normalize(random - normal * dot(random, normal));
    vec3 b = cross(normal, t);

    return mat3(t, b, normal);
}

float ssaoKernel(const in vec2 uv, const in vec3 origin)
{
    vec3 screenspaceNormal = normalMatrix * normal(uv);

    // randomized orientation matrix for hemisphere based on face normal
    mat3 m = noised(screenspaceNormal, uv);

    float ao = 0.0;

    for (float i = 0.0; i < ssaoSamplerSizes[0]; ++i)
    {
        vec3 s = m * kernel(i);

        s *= 2.0 * ssaoRadius;
        s += origin;

        vec4 s_offset = projectionMatrix * vec4(s, 1.0);
        s_offset.xy /= s_offset.w;

        s_offset.xy = s_offset.xy * 0.5 + 0.5;

        float sd = -linearDepth(s_offset.xy);

        float rangeCheck = smoothstep(0.0, 1.0, ssaoRadius / abs(-origin.z + sd));
        ao += rangeCheck * float(sd > s.z);
    }

    return pow(1.0 - (ao * ssaoSamplerSizes[1]), ssaoIntensity);
}


vec3 ssao(in vec2 uv, vec3 ssaoColor)
{
    float d = linearDepth(uv);

    if (d > farZ)
        return vec3(1.0f);

    vec4 eye = (projectionInverseMatrix * vec4(2.0*(uv - vec2(0.5)), 1.0, 1.0));
    eye.xyz /= eye.w;
    eye.xyz /= farZ;
    // eye has a z of -1 here

    vec3 origin = eye.xyz * d;

    float v = ssaoKernel(uv, origin);
    return mix(ssaoColor, vec3(1.0f), v);
}
