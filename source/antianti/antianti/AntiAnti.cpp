#include "AntiAnti.h"

#include <iostream>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/random.hpp>

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
#include <gloperate/primitives/Scene.h>
#include <gloperate/primitives/PolygonalDrawable.h>
#include <gloperate/primitives/PolygonalGeometry.h>
#include <gloperate/primitives/ScreenAlignedQuad.h>

#include <reflectionzeug/PropertyGroup.h>

#include <widgetzeug/make_unique.hpp>


using namespace gl;
using namespace glm;
using namespace globjects;

using widgetzeug::make_unique;

AntiAnti::AntiAnti(gloperate::ResourceManager & resourceManager)
    : Painter(resourceManager)
    , m_targetFramebufferCapability(addCapability(new gloperate::TargetFramebufferCapability()))
    , m_viewportCapability(addCapability(new gloperate::ViewportCapability()))
    , m_projectionCapability(addCapability(new gloperate::PerspectiveProjectionCapability(m_viewportCapability)))
    , m_cameraCapability(addCapability(new gloperate::CameraCapability()))
    , m_multisampling(false)
    , m_multisamplingChanged(false)
    , m_transparency(0.5)
    , m_maxSubpixelShift(1.0f)
    , m_frame(0)
{    
    setupPropertyGroup();
}

AntiAnti::~AntiAnti() = default;

void AntiAnti::setupPropertyGroup()
{
    addProperty<bool>("multisampling", this,
        &AntiAnti::multisampling, &AntiAnti::setMultisampling);
    
    addProperty<float>("transparency", this,
        &AntiAnti::transparency, &AntiAnti::setTransparency)->setOptions({
        { "minimum", 0.0f },
        { "maximum", 1.0f },
        { "step", 0.1f },
        { "precision", 1u }});

    addProperty<float>("subPixelShift", this,
        &AntiAnti::subpixelShift, &AntiAnti::setSubpixelShift)->setOptions({
            { "minimum", 0.0f },
            { "step", 0.05f },
            { "precision", 2u },
    });
}

bool AntiAnti::multisampling() const
{
    return m_multisampling;
}

void AntiAnti::setMultisampling(bool b)
{
    m_multisamplingChanged = m_multisampling != b;
    m_multisampling = b;
    m_frame = 0;
}

float AntiAnti::transparency() const
{
    return m_transparency;
}

void AntiAnti::setTransparency(float transparency)
{
 
    m_transparency = transparency;
    m_frame = 0;
}
float AntiAnti::subpixelShift() const
{
    return m_maxSubpixelShift;
}

void AntiAnti::setSubpixelShift(float shift)
{
    m_maxSubpixelShift = shift;
    m_frame = 0;
}

void AntiAnti::onInitialize()
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

    setupDrawable();
    setupProgram();
    setupProjection();
    setupFramebuffer();
}

void AntiAnti::onPaint()
{

    if (m_multisamplingChanged)
    {
        m_multisamplingChanged = false;
        setupProgram();
        setupFramebuffer();
    }
    
    if (m_viewportCapability->hasChanged())
    {
        glViewport(
            m_viewportCapability->x(),
            m_viewportCapability->y(),
            m_viewportCapability->width(),
            m_viewportCapability->height());

        m_viewportCapability->setChanged(false);
        
        updateFramebuffer();
    }

    m_fbo->bind(GL_FRAMEBUFFER);
    m_fbo->clearBuffer(GL_COLOR, 0, glm::vec4{0.85f, 0.87f, 0.91f, 1.0f});
    m_fbo->clearBufferfi(GL_DEPTH_STENCIL, 0, 1.0f, 0);
    
    glEnable(GL_DEPTH_TEST);

    const auto transform = m_projectionCapability->projection() * m_cameraCapability->view();
    const auto eye = m_cameraCapability->eye();

    glm::vec2 shift = glm::vec2(
        glm::linearRand<float>(-m_maxSubpixelShift * 0.5f, m_maxSubpixelShift * 0.5f),
        glm::linearRand<float>(-m_maxSubpixelShift * 0.5f, m_maxSubpixelShift * 0.5f))
        / glm::vec2(m_viewportCapability->width(), m_viewportCapability->height());

    m_grid->update(eye, transform);
    m_grid->draw(shift);
    
    glEnable(GL_SAMPLE_SHADING);
    glMinSampleShading(1.0);
    
    m_program->use();

    m_program->setUniform("subpixelShift", shift);
    m_program->setUniform(m_transformLocation, transform);
    
    for (auto i = 0u; i < m_drawables.size(); ++i)
    {
        m_program->setUniform(m_transparencyLocation, i % 2 == 0 ? m_transparency : 1.0f);
        m_drawables[i]->draw();
    }
    
    m_program->release();
    
    glDisable(GL_SAMPLE_SHADING);
    glMinSampleShading(0.0);

    Framebuffer::unbind(GL_FRAMEBUFFER);



    glDisable(GL_DEPTH_TEST);

    m_ppfbo->bind(GL_FRAMEBUFFER);
    m_colorAttachment->bindActive(GL_TEXTURE0);
    m_ppTexture->bindActive(GL_TEXTURE1);

    ++m_frame;

    m_quad->program()->setUniform("frame", m_frame);
    m_quad->draw();


    m_ppfbo->unbind(GL_FRAMEBUFFER);


    
    const auto rect = std::array<gl::GLint, 4>{{
        m_viewportCapability->x(),
        m_viewportCapability->y(),
        m_viewportCapability->width(),
        m_viewportCapability->height()}};
    
    auto targetfbo = m_targetFramebufferCapability->framebuffer();
    auto drawBuffer = GL_COLOR_ATTACHMENT0;
    
    if (!targetfbo)
    {
        targetfbo = globjects::Framebuffer::defaultFBO();
        drawBuffer = GL_BACK_LEFT;
    }
    
    m_ppfbo->blit(GL_COLOR_ATTACHMENT0, rect, targetfbo, drawBuffer, rect,
        GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);
}

void AntiAnti::setupFramebuffer()
{
    if (m_multisampling)
    {
        m_colorAttachment = new Texture(GL_TEXTURE_2D_MULTISAMPLE);
        m_colorAttachment->bind(); // workaround
        m_depthAttachment = new Texture(GL_TEXTURE_2D_MULTISAMPLE);
        m_depthAttachment->bind(); // workaround
    }
    else
    {
        m_colorAttachment = Texture::createDefault(GL_TEXTURE_2D);
        m_depthAttachment = Texture::createDefault(GL_TEXTURE_2D);
    }
    
    m_fbo = make_ref<Framebuffer>();

    m_fbo->attachTexture(GL_COLOR_ATTACHMENT0, m_colorAttachment);
    m_fbo->attachTexture(GL_DEPTH_ATTACHMENT, m_depthAttachment);

    m_ppTexture = Texture::createDefault(GL_TEXTURE_2D);

    m_ppfbo = make_ref<Framebuffer>();
    m_ppfbo->attachTexture(GL_COLOR_ATTACHMENT0, m_ppTexture);

    updateFramebuffer();

    m_fbo->printStatus(true);
    m_ppfbo->printStatus(true);
}

void AntiAnti::setupProjection()
{
    static const auto zNear = 0.3f, zFar = 30.f, fovy = 50.f;

    m_projectionCapability->setZNear(zNear);
    m_projectionCapability->setZFar(zFar);
    m_projectionCapability->setFovy(radians(fovy));

    m_grid->setNearFar(zNear, zFar);
}

void AntiAnti::setupDrawable()
{

    m_quad = new gloperate::ScreenAlignedQuad(Shader::fromFile(GL_FRAGMENT_SHADER, "data/antianti/postprocessing.frag"));

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

void AntiAnti::setupProgram()
{
    static const auto shaderPath = std::string{"data/antianti/"};
    const auto shaderName = m_multisampling ? "screendoor_multisample" : "screendoor";
    
    const auto vertexShader = shaderPath + shaderName + ".vert";
    const auto fragmentShader = shaderPath + shaderName + ".frag";
    
    m_program = make_ref<Program>();
    m_program->attach(
        Shader::fromFile(GL_VERTEX_SHADER, vertexShader),
        Shader::fromFile(GL_FRAGMENT_SHADER, fragmentShader));
    
    m_transformLocation = m_program->getUniformLocation("transform");
    m_transparencyLocation = m_program->getUniformLocation("transparency");
}

void AntiAnti::updateFramebuffer()
{
    static const auto numSamples = 4u;
    const auto width = m_viewportCapability->width(), height = m_viewportCapability->height();
    
    if (m_multisampling)
    {
        m_colorAttachment->image2DMultisample(numSamples, GL_RGBA8, width, height, GL_TRUE);
        m_depthAttachment->image2DMultisample(numSamples, GL_DEPTH_COMPONENT, width, height, GL_TRUE);
    }
    else
    {
        m_colorAttachment->image2D(0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
        m_depthAttachment->image2D(0, GL_DEPTH_COMPONENT, width, height, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, nullptr);
    }

    m_ppTexture->image2D(0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
}
