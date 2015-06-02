#pragma once

#include <memory>

#include <vector>

#include <glm/mat4x4.hpp>

#include <glbinding/gl/types.h>

#include <globjects/base/ref_ptr.h>

#include <gloperate/painter/Painter.h>


namespace globjects
{
    class Framebuffer;
    class Program;
    class Texture;
}

namespace gloperate
{
    class AdaptiveGrid;
    class ResourceManager;
    class AbstractTargetFramebufferCapability;
    class AbstractViewportCapability;
    class AbstractPerspectiveProjectionCapability;
    class AbstractCameraCapability;
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
    
    bool multisampling() const;
    void setMultisampling(bool b);
    
    float transparency() const;
    void setTransparency(float transparency);

    float subpixelShift() const;
    void setSubpixelShift(float shift);
    
protected:
    virtual void onInitialize() override;
    virtual void onPaint() override;

protected:
    void setupFramebuffer();
    void setupProjection();
    void setupDrawable();
    void setupProgram();
    void updateFramebuffer();

protected:
    /* capabilities */
    gloperate::AbstractTargetFramebufferCapability * m_targetFramebufferCapability;
    gloperate::AbstractViewportCapability * m_viewportCapability;
    gloperate::AbstractPerspectiveProjectionCapability * m_projectionCapability;
    gloperate::AbstractCameraCapability * m_cameraCapability;

    /* members */
    globjects::ref_ptr<globjects::Framebuffer> m_fbo;
    globjects::ref_ptr<globjects::Texture> m_colorAttachment;
    globjects::ref_ptr<globjects::Texture> m_depthAttachment;

    globjects::ref_ptr<globjects::Framebuffer> m_ppfbo;
    globjects::ref_ptr<globjects::Texture> m_ppTexture;

    
    globjects::ref_ptr<gloperate::AdaptiveGrid> m_grid;
    globjects::ref_ptr<gloperate::ScreenAlignedQuad> m_quad;
    globjects::ref_ptr<globjects::Program> m_program;
    gl::GLint m_transformLocation;
    gl::GLint m_transparencyLocation;
    std::vector<std::unique_ptr<gloperate::PolygonalDrawable>> m_drawables;

    int m_frame;
    bool m_multisampling;
    bool m_multisamplingChanged;
    float m_transparency;
    float m_maxSubpixelShift;
    glm::mat4 m_lastTransform;

    float m_maxDofShift;
    float m_focalDepth;
};
