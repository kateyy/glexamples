#ifndef MATERIALS
#define MATERIALS

#include </PATHTRACING_EXTENSIONS/geometryTraversal>

vec4 getBackgroundColor(in vec3 viewDirection)
{
    return vec4(0.85, 0.87, 0.91, 1.0);
}

void getMaterial(in RayIntersection intersection, out PathStackData stackData)
{
    stackData.materialID = intersection.index;
    stackData.BRDF = 1.0;
}

vec4 getColor(in PathStackData stackData)
{
    uint objectIndex = indices[stackData.materialID].w;
    if (objectIndex == reflectiveObjectIndex)
        return vec4(1.0);

    uint normalIndex = indices[stackData.materialID].x;
    vec3 normal = normals[normalIndex].xyz;

    return vec4(normal * 0.5 + 0.5, 1.0);
}

#endif
