#pragma once

#include <glm/vec2.hpp>

#include <globjects/base/ref_ptr.h>

#include <globjects/Framebuffer.h>
#include <globjects/Texture.h>

#include <gloperate/primitives/ScreenAlignedQuad.h>
#include <gloperate/pipeline/AbstractStage.h>


#include <gloperate/painter/Camera.h>
#include <gloperate/pipeline/AbstractStage.h>
#include <gloperate/pipeline/InputSlot.h>
#include <gloperate/pipeline/Data.h>

namespace gloperate
{

class AbstractViewportCapability;
class AbstractPerspectiveProjectionCapability;
class AbstractCameraCapability;

}

class  PostProcessing
{
public:
    PostProcessing();

    void initialize();
public:
    enum Output : gl::GLint {
        Source_Final = 0,
        Source_Color = 1,
        Source_Normals = 3,
        Source_Geometry = 4,
        Source_Depth = 5,
        Source_OcclusionMap = 6, 
        Source_ShadowMap = 7,
    };

    glm::vec2 viewport;
    bool useSSAO;
    float ssaoRadius;
    float ssaoIntensity;
    bool useSSAONoise;
    bool duringInterpolation;
    bool inInteraction;

    Output output;

    globjects::ref_ptr<gloperate::Camera> camera;

    globjects::ref_ptr<globjects::Texture> colorTexture;
    globjects::ref_ptr<globjects::Texture> normalTexture;
    globjects::ref_ptr<globjects::Texture> depthBufferTexture;
    globjects::ref_ptr<globjects::Texture> shadowMap;

    globjects::ref_ptr<globjects::Texture> lastFrame;
    globjects::ref_ptr<globjects::Framebuffer> fbo;
    int frame;

    void process();
protected:
    void render();
    globjects::ref_ptr<gloperate::ScreenAlignedQuad> m_screenAlignedQuad;

    globjects::ref_ptr<globjects::Texture> m_ssaoKernel;
    globjects::ref_ptr<globjects::Texture> m_ssaoNoise;
};
