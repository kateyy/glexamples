#version 140

// from https://github.com/lanice/elemate
// from https://github.com/hpicgs/cgsee

float linearize(float depth, vec2 zRange) {
    // d = (2.0 * zfar * znear / (zfar + znear - (zfar - znear) * (2.0 * z- 1.0)));
    // normalized to [0,1]
    // d = (d - znear) / (zfar - znear);

    // simplyfied with wolfram alpha
    float znear = zRange.x;
    float zfar = zRange.y;
    return - znear * depth / (zfar * depth - zfar - znear * depth);
}

float depthNdcToWindow(float ndcDepth)
{   // https://www.opengl.org/wiki/Vertex_Post-Processing#Viewport_transform
    return ((gl_DepthRange.diff * ndcDepth) + gl_DepthRange.near + gl_DepthRange.far) / 2.0;
}
