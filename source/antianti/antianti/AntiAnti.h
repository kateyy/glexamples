#pragma once

#include <memory>

#include <vector>

#include <glm/mat4x4.hpp>

#include <glbinding/gl/types.h>

#include <globjects/base/ref_ptr.h>

#include <gloperate/painter/Painter.h>

#include "PostProcessing.h"


namespace globjects
{
    class Framebuffer;
    class Program;
    class Texture;
}

namespace gloperate
{
    class AdaptiveGrid;
    class CoordinateProvider;
    class ResourceManager;
    class AbstractTargetFramebufferCapability;
    class TypedRenderTargetCapability;
    class AbstractViewportCapability;
    class AbstractPerspectiveProjectionCapability;
    class AbstractCameraCapability;
    class AbstractInputCapability;
    class PolygonalDrawable;
    class ScreenAlignedQuad;
}


class AntiAnti : public gloperate::Painter
{
public:
    AntiAnti(gloperate::ResourceManager & resourceManager);
    virtual ~AntiAnti();
    
public:
    void setupPropertyGroup();
    
    float transparency() const;
    void setTransparency(float transparency);

    bool backFaceCulling() const;
    void setBackFaceCulling(bool backFaceCulling);

    float subpixelShift() const;
    void setSubpixelShift(float shift);

    float maxLightSourceShift() const;
    void setMaxLightSourceShift(float shift);
    
protected:
    virtual void onInitialize() override;
    virtual void onPaint() override;

private:
    void setupFramebuffer();
    void setupProjection();
    void setupTransparencyRandomness();
    void setupDrawable();
    void setupProgram();
    void updateFramebuffer();

    void drawShadowMap();

protected:

    class HackedInputCapability;    // I'm sure it's not meant to be used that way

    /* capabilities */
    gloperate::AbstractTargetFramebufferCapability * m_targetFramebufferCapability;
    gloperate::TypedRenderTargetCapability * m_renderTargetCapability;
    gloperate::AbstractViewportCapability * m_viewportCapability;
    gloperate::AbstractPerspectiveProjectionCapability * m_projectionCapability;
    gloperate::AbstractCameraCapability * m_cameraCapability;
    HackedInputCapability * m_inputCapability;

    /* members */
    globjects::ref_ptr<globjects::Framebuffer> m_fbo;
    globjects::ref_ptr<globjects::Texture> m_colorAttachment;
    globjects::ref_ptr<globjects::Texture> m_normalAttachment;
    globjects::ref_ptr<globjects::Texture> m_depthAttachment;

    globjects::ref_ptr<globjects::Framebuffer> m_fboShadowing;
    globjects::ref_ptr<globjects::Texture> m_shadowMap;
    globjects::ref_ptr<globjects::Program> m_programShadowing;

    globjects::ref_ptr<globjects::Framebuffer> m_ppfbo;
    globjects::ref_ptr<globjects::Texture> m_ppTexture;

    globjects::ref_ptr<globjects::Texture> m_transparencyNoise;
    
    globjects::ref_ptr<gloperate::AdaptiveGrid> m_grid;
    globjects::ref_ptr<globjects::Program> m_program;
    std::vector<std::unique_ptr<gloperate::PolygonalDrawable>> m_drawables;
    std::vector<std::vector<bool>> m_transparencyRandomness;

    std::unique_ptr<gloperate::CoordinateProvider> m_coordProvider;

protected:

    int m_frame;
    float m_maxSubpixelShift;
    glm::mat4 m_lastTransform;

    bool m_backFaceCulling;
    bool m_backFaceCullingShadows;
    float m_transparency;
    bool m_useObjectBasedTransparency;
    int m_numTransparencySamples;

    bool m_pointOrPlaneDoF;
    float m_maxDofShift;
    float m_focalDepth;
    bool m_dofAtCursor;

    glm::vec3 m_lightPosition;
    glm::vec3 m_lightFocus;
    glm::vec2 m_lightZRange;
    float m_maxLightSourceShift;
    bool m_linearizedShadowMap;
    bool m_shadowMapParamsChanged;
    gl::GLenum m_shadowDepthFormat;
    gl::GLint m_shadowMapWidth;

    PostProcessing m_postProcessing;

    gl::GLenum m_accTextureFormat;
};
