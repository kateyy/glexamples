#pragma once

#include <memory>

#include <vector>

#include <glm/mat4x4.hpp>

#include <glbinding/gl/types.h>

#include <globjects/base/ref_ptr.h>

#include <gloperate/painter/Painter.h>

#include "PostProcessing.h"
#include "SceneLoader.h"


namespace globjects
{
    class Framebuffer;
    class Program;
    class Renderbuffer;
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
    
protected:
    virtual void onInitialize() override;
    void checkAndBindTexture(int meshID, aiTextureType type, std::string uniformName, gl::GLenum target);
    void checkAndUnbindTexture(int meshID, aiTextureType type, gl::GLenum target);
    virtual void onPaint() override;

private:
    struct LightSource
    {
        bool enabled;
        glm::vec3 position;
        glm::vec3 focalPoint;
        float radius;
        float intensity;
        glm::vec2 zRange;
    };

private:
    void setupFramebuffer();
    void setupTransparencyRandomness();
    void setupProgram();
    void updateFramebuffer();

    void drawShadowMaps();
    void drawShadowMap(const LightSource & light, globjects::Framebuffer & fbo, glm::mat4 * shadowTransform = nullptr) const;

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

    std::vector<globjects::ref_ptr<globjects::Framebuffer>> m_fbosShadowing;
    globjects::ref_ptr<globjects::Texture> m_shadowMap;
    globjects::ref_ptr<globjects::Renderbuffer> m_shadowMapRenderbuffer;
    globjects::ref_ptr<globjects::Program> m_programShadowing;

    globjects::ref_ptr<globjects::Framebuffer> m_ppfbo;
    globjects::ref_ptr<globjects::Texture> m_ppTexture;

    globjects::ref_ptr<globjects::Texture> m_transparencyNoise;
    
    globjects::ref_ptr<gloperate::AdaptiveGrid> m_grid;
    globjects::ref_ptr<globjects::Program> m_program;
    std::vector<std::vector<bool>> m_transparencyRandomness;

    std::unique_ptr<gloperate::CoordinateProvider> m_coordProvider;

protected:

    int m_frame;
    float m_maxSubpixelShift;
    glm::mat4 m_lastTransform;
    reflectionzeug::Color m_backgroundColor;

    bool m_backFaceCulling;
    bool m_backFaceCullingShadows;
    float m_transparency;
    bool m_useObjectBasedTransparency;
    int m_numTransparencySamples;

    bool m_dofEnabled;
    bool m_usePointDoF;
    float m_maxDofShift;
    float m_focalDepth;
    bool m_dofAtCursor;

    int m_numFrames;
    
    std::vector<LightSource> m_lights;

    bool m_shadowsEnabled;
    bool m_linearizedShadowMap;
    bool m_shadowMapParamsChanged;
    gl::GLenum m_shadowMapFormat;
    gl::GLint m_shadowMapWidth;

    PostProcessing m_postProcessing;

    gl::GLenum m_accTextureFormat;

    SceneLoader m_sceneLoader;
};
