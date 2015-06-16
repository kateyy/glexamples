#pragma once

#include <memory>
#include <vector>

#include <glbinding/gl/types.h>
#include <glbinding/gl/enum.h>

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
    class ScreenAlignedQuad;
    class PolygonalDrawable;
}

class ProgressiveTransparencyOptions;

class ProgressiveTransparency : public gloperate::Painter
{
public:
    ProgressiveTransparency(gloperate::ResourceManager & resourceManager);
    virtual ~ProgressiveTransparency() override;
    
protected:
    virtual void onInitialize() override;
    virtual void onPaint() override;

protected:
    void setupFramebuffer();
    void setupProjection();
    void setupPrograms();
    void setupDrawable();
    void updateFramebuffer();
    
protected:
    void clearBuffers();
    void updateUniforms();
    void renderAlphaToCoverage(gl::GLenum colorAttachment);
    void blit();

private:
    /** \name Capabilities */
    /** \{ */
    
    gloperate::AbstractTargetFramebufferCapability * m_targetFramebufferCapability;
    gloperate::AbstractViewportCapability * m_viewportCapability;
    gloperate::AbstractPerspectiveProjectionCapability * m_projectionCapability;
    gloperate::AbstractCameraCapability * m_cameraCapability;
    
    /** \} */

    /** \name Framebuffers and Textures */
    /** \{ */
    
    globjects::ref_ptr<globjects::Framebuffer> m_fbo;
    globjects::ref_ptr<globjects::Texture> m_colorAttachment;
    globjects::ref_ptr<globjects::Texture> m_depthAttachment;
    
    /** \} */
    
    /** \name Programs */
    /** \{ */
    
    globjects::ref_ptr<globjects::Program> m_alphaToCoverageProgram;
    
    /** \} */
    
    /** \name Geometry */
    /** \{ */
    
    globjects::ref_ptr<gloperate::AdaptiveGrid> m_grid;
    std::vector<std::unique_ptr<gloperate::PolygonalDrawable>> m_drawables;
    
    /** \} */

    /** \name Properties */
    /** \{ */
    
    std::unique_ptr<ProgressiveTransparencyOptions> m_options;
    
    /** \} */

    int m_frame;
};
