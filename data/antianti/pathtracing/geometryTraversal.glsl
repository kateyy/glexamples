#ifndef GEOMETRYTRAVERSAL
#define GEOMETRYTRAVERSAL

#include </pathtracing/generic/dataTypes.glsl>
#include </pathtracing/generic/intersectionPrimitives.glsl>
#include </pathtracing/generic/ray.glsl>

uniform uint numTriangles;
uniform uint reflectiveObjectIndex = 2;

layout (std430, binding = 6) readonly buffer Indices
{
    uvec4 indices[];
};

layout (std430, binding = 7) readonly buffer Vertices
{
    vec4 vertices[];    // vec3 with padding
};

layout (std430, binding = 8) readonly buffer Normals
{
    vec4 normals[];    // vec3 with padding
};


void checkGeomertryIntersection(in Ray ray, out IntersectionTestResult result)
{
    float t;
    int nearest = -1;
    float nearestT = INFINITY;

    uvec4 ids;
    vec3 tri[3];
    bool hitBackFace, nearestHitBackFace;

    for (int i = 0; i < numTriangles; ++i)
    {
        ids = indices[i];
        tri[0] = vertices[ids[0]].xyz;
        tri[1] = vertices[ids[1]].xyz;
        tri[2] = vertices[ids[2]].xyz;
        if (!intersectionTriangle(tri, ray.origin, ray.direction, INFINITY, t, hitBackFace, true))
            continue;

        if (t < nearestT)
        {
            nearestT = t;
            nearest = i;
            nearestHitBackFace = hitBackFace;
        }
    }

    if (nearest < 0)
    {
        result.hit = false;
        return;
    }

    result.hit = true;
    result.index = nearest;
    result.t = nearestT;
    ids = indices[nearest];
    tri[0] = vertices[ids[0]].xyz;
    tri[1] = vertices[ids[1]].xyz;
    tri[2] = vertices[ids[2]].xyz;
    result.normal = triangleNormal(tri) * (nearestHitBackFace ? -1 : 1);
}

#endif
