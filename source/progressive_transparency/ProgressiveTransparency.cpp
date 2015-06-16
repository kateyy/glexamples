#include "ProgressiveTransparency.h"

#include <iostream>
#include <chrono>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>

#include <glbinding/gl/boolean.h>
#include <glbinding/gl/enum.h>
#include <glbinding/gl/bitfield.h>

#include <globjects/globjects.h>
#include <globjects/logging.h>
#include <globjects/Framebuffer.h>
#include <globjects/DebugMessage.h>
#include <globjects/Program.h>
#include <globjects/Texture.h>

#include <gloperate/base/RenderTargetType.h>
#include <gloperate/base/make_unique.hpp>
#include <gloperate/resources/ResourceManager.h>
#include <gloperate/painter/TargetFramebufferCapability.h>
#include <gloperate/painter/ViewportCapability.h>
#include <gloperate/painter/PerspectiveProjectionCapability.h>
#include <gloperate/painter/CameraCapability.h>
#include <gloperate/primitives/AdaptiveGrid.h>
#include <gloperate/primitives/ScreenAlignedQuad.h>
#include <gloperate/primitives/Scene.h>

#include <gloperate/primitives/PolygonalDrawable.h>
#include <gloperate/primitives/PolygonalGeometry.h>

#include <reflectionzeug/PropertyGroup.h>
#include <widgetzeug/make_unique.hpp>

#include "MasksTableGenerator.h"
#include "ProgressiveTransparencyOptions.h"


using namespace gl;
using namespace glm;
using namespace globjects;

using widgetzeug::make_unique;

ProgressiveTransparency::ProgressiveTransparency(gloperate::ResourceManager & resourceManager)
:   Painter(resourceManager)
,   m_targetFramebufferCapability(addCapability(new gloperate::TargetFramebufferCapability()))
,   m_viewportCapability(addCapability(new gloperate::ViewportCapability()))
,   m_projectionCapability(addCapability(new gloperate::PerspectiveProjectionCapability(m_viewportCapability)))
,   m_cameraCapability(addCapability(new gloperate::CameraCapability()))
,   m_options(new ProgressiveTransparencyOptions(*this))
,   m_frame(0)
{
}

ProgressiveTransparency::~ProgressiveTransparency() = default;

void ProgressiveTransparency::onInitialize()
{
    globjects::init();
    globjects::DebugMessage::enable();

#ifdef __APPLE__
    Shader::clearGlobalReplacements();
    Shader::globalReplace("#version 140", "#version 150");

    debug() << "Using global OS X shader replacement '#version 140' -> '#version 150'" << std::endl;
#endif
    
    m_grid = make_ref<gloperate::AdaptiveGrid>();
    m_grid->setColor({0.6f, 0.6f, 0.6f});
    
    setupPrograms();
    setupProjection();
    setupFramebuffer();
    setupDrawable();
}

void ProgressiveTransparency::onPaint()
{
    if (m_viewportCapability->hasChanged())
    {
        m_frame = 0;

        glViewport(
            m_viewportCapability->x(),
            m_viewportCapability->y(),
            m_viewportCapability->width(),
            m_viewportCapability->height());

        m_viewportCapability->setChanged(false);
        
        const auto viewport = glm::vec2{m_viewportCapability->width(), m_viewportCapability->height()};
        m_alphaToCoverageProgram->setUniform("viewport", viewport);
        
        updateFramebuffer();
    }

    if (m_cameraCapability->hasChanged())
    {
        m_frame = 0;
    }

    auto now = std::chrono::steady_clock::now();
    auto now2 = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();

    m_alphaToCoverageProgram->setUniform("time", static_cast<GLuint>(now2));

    
    clearBuffers();
    updateUniforms();
    
    if (m_options->backFaceCulling())
        glEnable(GL_CULL_FACE);
        
    renderAlphaToCoverage(GL_COLOR_ATTACHMENT0);
        
    blit();
    
    Framebuffer::unbind(GL_FRAMEBUFFER);

    ++m_frame;
}

void ProgressiveTransparency::setupFramebuffer()
{
    m_colorAttachment = make_ref<Texture>(GL_TEXTURE_2D);
    m_depthAttachment = make_ref<Texture>(GL_TEXTURE_2D);
    
    updateFramebuffer();
    
    m_fbo = make_ref<Framebuffer>();
    
    m_fbo->attachTexture(GL_COLOR_ATTACHMENT0, m_colorAttachment);
    m_fbo->attachTexture(GL_DEPTH_ATTACHMENT, m_depthAttachment);

    m_fbo->printStatus(true);
}

void ProgressiveTransparency::setupProjection()
{
    static const auto zNear = 0.3f, zFar = 30.f, fovy = 50.f;

    m_projectionCapability->setZNear(zNear);
    m_projectionCapability->setZFar(zFar);
    m_projectionCapability->setFovy(radians(fovy));

    m_grid->setNearFar(zNear, zFar);
}

void ProgressiveTransparency::setupDrawable()
{
    // Load scene
    const auto scene = m_resourceManager.load<gloperate::Scene>("data/transparency/transparency_scene.obj");
    if (!scene)
    {
        std::cout << "Could not load file" << std::endl;
        return;
    }

    // Create a renderable for each mesh
    for (const auto * geometry : scene->meshes()) {
        m_drawables.push_back(gloperate::make_unique<gloperate::PolygonalDrawable>(*geometry));
    }

    // Release scene
    delete scene;
}

void ProgressiveTransparency::setupPrograms()
{
    static const auto alphaToCoverageShaders = "alpha_to_coverage";
    
    const auto initProgram = [] (globjects::ref_ptr<globjects::Program> & program, const char * shaders)
    {
        static const auto shaderPath = std::string{"data/progressive_transparency/"};
        
        program = make_ref<Program>();
        program->attach(
            Shader::fromFile(GL_VERTEX_SHADER, shaderPath + shaders + ".vert"),
            Shader::fromFile(GL_FRAGMENT_SHADER, shaderPath + shaders + ".frag"));
    };
    
    initProgram(m_alphaToCoverageProgram, alphaToCoverageShaders);
    
    glBindAttribLocation(m_alphaToCoverageProgram->id(), 0, "a_vertex");
    glBindAttribLocation(m_alphaToCoverageProgram->id(), 1, "a_normal");
}

void ProgressiveTransparency::updateFramebuffer()
{
    const auto size = glm::ivec2{m_viewportCapability->width(), m_viewportCapability->height()};
    
    m_colorAttachment->image2D(0, GL_RGBA32F, size, 0, GL_RGBA, GL_FLOAT, nullptr);
    m_depthAttachment->image2D(0, GL_DEPTH_COMPONENT, size, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
}

void ProgressiveTransparency::clearBuffers()
{
    m_fbo->setDrawBuffers({ GL_COLOR_ATTACHMENT0 });
    
    m_fbo->clearBuffer(GL_COLOR, 0, glm::vec4(0.85f, 0.87f, 0.91f, 1.0f));
    m_fbo->clearBuffer(GL_COLOR, 1, glm::vec4(0.0f));
    m_fbo->clearBuffer(GL_COLOR, 2, glm::vec4(1.0f));
    m_fbo->clearBufferfi(GL_DEPTH_STENCIL, 0, 1.0f, 0);
}

void ProgressiveTransparency::updateUniforms()
{
    const auto transform = m_projectionCapability->projection() * m_cameraCapability->view();
    const auto eye = m_cameraCapability->eye();
    const auto transparency = m_options->transparency();
    
    m_grid->update(eye, m_cameraCapability->view(), m_projectionCapability->projection());
    
    auto updateProgramUniforms = [&transform, &transparency] (Program * program)
    {
        program->setUniform("transform", transform);
        program->setUniform("transparency", transparency);
    };
    
    updateProgramUniforms(m_alphaToCoverageProgram);
}

void ProgressiveTransparency::renderAlphaToCoverage(gl::GLenum colorAttachment)
{
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);

    m_fbo->bind(GL_FRAMEBUFFER);
    m_fbo->setDrawBuffer(colorAttachment);

    m_alphaToCoverageProgram->use();

    for (auto & drawable : m_drawables)
        drawable->draw();

    m_alphaToCoverageProgram->release();

    m_fbo->unbind();
}

void ProgressiveTransparency::blit()
{
    auto targetfbo = m_targetFramebufferCapability->framebuffer();
    auto drawBuffer = GL_COLOR_ATTACHMENT0;
    
    if (!targetfbo)
    {
        targetfbo = Framebuffer::defaultFBO();
        drawBuffer = GL_BACK_LEFT;
    }
    
    const auto rect = std::array<GLint, 4>{{
        m_viewportCapability->x(),
        m_viewportCapability->y(),
        m_viewportCapability->width(),
        m_viewportCapability->height()
    }};
    
    targetfbo->bind();

    m_fbo->blit(GL_COLOR_ATTACHMENT0, rect, targetfbo, drawBuffer, rect,
        GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);

    targetfbo->unbind();
}
