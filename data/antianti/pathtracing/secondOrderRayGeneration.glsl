#ifndef SECONDORDERRAYGENERATION
#define SECONDORDERRAYGENERATION

#include </pathtracing/generic/dataTypes.glsl>
#include </pathtracing/generic/random.glsl>

#include </PATHTRACING_EXTENSIONS/geometryTraversal>


void generateSecondOrderRay(in Ray lastRay, inout RayIntersection intersection)
{
    uint objectIndex = indices[intersection.index].w;
    
    vec3 direction;
    if (objectIndex == reflectiveObjectIndex)
    {
        direction = reflect(lastRay.direction, normalize (intersection.normal));
    }
    else
    {
        direction = randomDirectionInHemisphere(intersection.normal);
    } 
    
    intersection.nextRay.origin = intersection.point;
    intersection.nextRay.direction = direction;

    Ray_addEpsilonOffset(intersection.nextRay, intersection.normal);
}


#endif
