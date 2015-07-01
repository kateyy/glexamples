#include "AntiAnti.h"

#include <chrono>
#include <iostream>
#include <string>
#include <random>
#include <limits>

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
#include <globjects/NamedString.h>
#include <globjects/Renderbuffer.h>
#include <globjects/base/File.h>

#include <gloperate/base/RenderTargetType.h>
#include <gloperate/base/make_unique.hpp>
#include <gloperate/resources/ResourceManager.h>
#include <gloperate/resources/RawFile.h>
#include <gloperate/painter/TargetFramebufferCapability.h>
#include <gloperate/painter/TypedRenderTargetCapability.h>
#include <gloperate/painter/ViewportCapability.h>
#include <gloperate/painter/PerspectiveProjectionCapability.h>
#include <gloperate/painter/CameraCapability.h>
#include <gloperate/painter/Camera.h>
#include <gloperate/painter/InputCapability.h>
#include <gloperate/primitives/AdaptiveGrid.h>
#include <gloperate/primitives/Scene.h>
#include <gloperate/primitives/PolygonalDrawable.h>
#include <gloperate/primitives/PolygonalGeometry.h>
#include <gloperate/primitives/ScreenAlignedQuad.h>
#include <gloperate/tools/CoordinateProvider.h>
#include <gloperate/tools/DepthExtractor.h>

#include <reflectionzeug/PropertyGroup.h>
#include <reflectionzeug/extensions/GlmProperties.hpp>

#include <widgetzeug/make_unique.hpp>

#include <assimp/material.h>

#include "SceneLoader.h"


using namespace gl;
using namespace glm;
using namespace globjects;
using namespace gloperate;

using widgetzeug::make_unique;

namespace
{
    void glSet(GLenum glenum, bool enable)
    {
        if (enable)
            glEnable(glenum);
        else
            glDisable(glenum);
    }
}

class AntiAnti::HackedInputCapability : public InputCapability
{
public:
    void onMouseMove(int x, int y) override
    {
        lastMousePosition = { x, y };
    }

    void onMousePress(int x, int y, gloperate::MouseButton button) override
    {
        lastMousePosition = { x, y };
    }

    void onKeyDown(gloperate::Key key) override
    {
        if (key == gloperate::KeyLeftControl || key == gloperate::KeyRightControl)
            ctrlPressed = true;
    }

    void onKeyUp(gloperate::Key key) override
    {
        if (key == gloperate::KeyLeftControl || key == gloperate::KeyRightControl)
            ctrlPressed = false;
    }

    glm::vec2 lastMousePosition;
    bool ctrlPressed = false;
};

AntiAnti::AntiAnti(gloperate::ResourceManager & resourceManager)
    : Painter(resourceManager)
    , m_targetFramebufferCapability(addCapability(new gloperate::TargetFramebufferCapability()))
    , m_renderTargetCapability(addCapability(new gloperate::TypedRenderTargetCapability()))
    , m_viewportCapability(addCapability(new gloperate::ViewportCapability()))
    , m_projectionCapability(addCapability(new gloperate::PerspectiveProjectionCapability(m_viewportCapability)))
    , m_cameraCapability(addCapability(new gloperate::CameraCapability()))
    , m_inputCapability(addCapability(new HackedInputCapability()))
    , m_frame(0)
    // misc
    , m_numFrames(10000)
    , m_maxSubpixelShift(1.0f)
    , m_accTextureFormat(GL_RGBA32F)
    // dof
    , m_dofEnabled(false)
    , m_usePointDoF(false)
    , m_maxDofShift(0.01f)
    , m_focalDepth(3.0f)
    , m_dofAtCursor(false)
    // shadows
    , m_shadowsEnabled(true)
    , m_lightPosition({0, 54, 0})
    , m_lightFocus({ 0, 24, 0 })
    , m_maxLightSourceShift(0.1f)
    , m_linearizedShadowMap(false)
    , m_shadowMapParamsChanged(true)
    , m_shadowMapFormat(GL_R32F)
    , m_shadowMapWidth(4096)
    , m_lightZRange({0.300, 50.00})
    // transparency
    , m_backFaceCulling(false)
    , m_backFaceCullingShadows(false)
    , m_useObjectBasedTransparency(true)
    , m_transparency(0.5f)
    , m_numTransparencySamples(1024)
{    
    setupPropertyGroup();
}

AntiAnti::~AntiAnti() = default;

void AntiAnti::setupPropertyGroup()
{
    addProperty<int>("numFrames",
        [this]() {return m_numFrames; },
        [this](int numFrames) {
            if (numFrames < m_numFrames)
                m_frame = 0;
            m_numFrames = numFrames;
    });
    addProperty<float>("subPixelShift",
        [this]() {return m_maxSubpixelShift; },
            [this](float maxShift) {
                m_maxSubpixelShift = maxShift;
                m_frame = 0;
        })->setOptions({
            { "minimum", 0.0f },
            { "step", 0.05f },
            { "precision", 2u },
    });


    {
        auto dofGroup = addGroup("DepthOfField");

        dofGroup->addProperty<bool>("enable",
            [this]() { return m_dofEnabled; },
            [this](bool enable) {
                m_dofEnabled = enable;
                m_frame = 0;
        });

        dofGroup->addProperty<bool>("focalPointNotPlane",
            [this]() {return m_usePointDoF; },
            [this](bool doPoint) {
                m_usePointDoF = doPoint;
                m_frame = 0;
        })->setOption("title", "Use Focal Point (not Focal Plane)");

        dofGroup->addProperty<float>("maxDofShift",
            [this]() {return m_maxDofShift; },
            [this](float shift) {
                m_maxDofShift = shift;
                m_frame = 0;
        })->setOptions({
            { "minimum", 0.0f },
            { "step", 0.001f },
            { "precision", 3u },
        });

        dofGroup->addProperty<float>("focalDepth",
            [this]() {return m_focalDepth; },
            [this](float d) {
                m_focalDepth = d;
                m_frame = 0;
        })->setOptions({
            { "minimum", 0.0f },
            { "step", 0.05f },
            { "precision", 2u },
        });

        dofGroup->addProperty<bool>("doofAtCursor",
            [this]() {return m_dofAtCursor; },
            [this](bool atCursor) {
                m_dofAtCursor = atCursor;
        });
    }

    {
        auto ssaoGroup = addGroup("SSAO");

        ssaoGroup->addProperty<bool>("useSSAO",
            [this]() {return m_postProcessing.useSSAO; },
            [this](bool useSSAO) {
                m_postProcessing.useSSAO = useSSAO;
                m_frame = 0;
        });

        ssaoGroup->addProperty<float>("SSAORadius",
            [this]() {return m_postProcessing.ssaoRadius; },
            [this](float ssaoRadius) {
                m_postProcessing.ssaoRadius = ssaoRadius;
                m_frame = 0;
        })->setOptions({
            { "minimum", 0.0f },
            { "step", 0.005f },
            { "precision", 3u },
        });

        ssaoGroup->addProperty<float>("SSAOIntensity",
            [this]() {return m_postProcessing.ssaoIntensity; },
            [this](float ssaoIntensity) {
                m_postProcessing.ssaoIntensity = ssaoIntensity;
                m_frame = 0;
        })->setOptions({
            { "minimum", 0.0f },
            { "step", 0.05f },
            { "precision", 2u },
        });
    }

    {
        auto shadows = addGroup("Shadows");

        shadows->addProperty<bool>("enable",
            [this]() { return m_shadowsEnabled; },
            [this](bool enable) {
                m_shadowsEnabled = enable;
                m_frame = 0;
        });

        shadows->addProperty<vec3>("LightPosition",
            [this] () { return m_lightPosition; },
            [this] (const vec3 & pos) {
                m_lightPosition = pos;
                m_frame = 0;
        });
        shadows->addProperty<vec3>("LightFocus",
            [this] () { return m_lightFocus; },
            [this] (const vec3 & pos) {
                m_lightFocus = pos;
                m_frame = 0;
        });

        shadows->addProperty<float>("LightSourceRadius",
            [this]() {return m_maxLightSourceShift; },
            [this](float lightSourceShift) {
                m_maxLightSourceShift = lightSourceShift;
                m_frame = 0;
        })->setOptions({
            { "minimum", 0.0f },
            { "step", 0.05f },
            { "precision", 2u },
            { "title", "Light Radius" }
        });

        shadows->addProperty<bool>("linearizedShadowMap",
            [this] () { return m_linearizedShadowMap; },
            [this] (bool l) {
                m_linearizedShadowMap = l;
                m_frame = 0;
        });

        shadows->addProperty<GLenum>("mapFormat",
            [this] () {return m_shadowMapFormat; },
            [this] (GLenum depthFormat) {
                m_shadowMapFormat = depthFormat;
                m_shadowMapParamsChanged = true;
                m_frame = 0;
        })->setStrings({
            { GLenum::GL_R8, "GL_R8" },
            { GLenum::GL_R16, "GL_R16" },
            { GLenum::GL_R32F, "GL_R32F" }
        });

        shadows->addProperty<GLint>("shadowMapWidth",
            [this] () { return m_shadowMapWidth; },
            [this] (GLint width) {
                m_shadowMapWidth = width;
                m_shadowMapParamsChanged = true;
                m_frame = 0;
        })->setOptions({
            {"minimum", 1},
            {"maximum", std::numeric_limits<GLint>::max() } });
            //{"maximum", maxTextureSize } });

        shadows->addProperty<float>("zNear",
            [this] () { return m_lightZRange.x; },
            [this] (float zNear) {
                m_lightZRange.x = zNear;
                m_frame = 0;
        })->setOptions({ { "step", 0.1f } });

        shadows->addProperty<float>("zFar",
            [this] () { return m_lightZRange.y; },
            [this] (float zFar) {
                m_lightZRange.y = zFar;
                m_frame = 0;
        })->setOptions({ { "step", 1.0f } });
    }
    
    {
        auto ppGroup = addGroup("Postprocessing");

        ppGroup->addProperty<PostProcessing::Output>("Output",
            [this] () {return m_postProcessing.output; },
            [this] (PostProcessing::Output o) {
                m_postProcessing.output = o; })
            ->setStrings({
                {PostProcessing::Output::Source_Final, "Final" },
                {PostProcessing::Output::Source_Color, "Color" },
                {PostProcessing::Output::Source_Normals, "Normals"},
                {PostProcessing::Output::Source_Geometry, "Geometry"},
                {PostProcessing::Output::Source_Depth, "Depth"},
                {PostProcessing::Output::Source_ShadowMap, "ShadowMap"}
            });

        ppGroup->addProperty<GLenum>("textureFormat",
            [this] () {return m_accTextureFormat; },
            [this] (GLenum accTextureFormat) {
                m_accTextureFormat = accTextureFormat;
                updateFramebuffer();
                m_frame = 0;
        })->setStrings({
            { GLenum::GL_RGBA8, "GL_RGBA8" },
            { GLenum::GL_RGBA32F, "GL_RGBA32F" },
            { GLenum::GL_RGBA16, "GL_RGBA16" }
        });
    }

    {
        auto ppGroup = addGroup("Transparency");

        ppGroup->addProperty<float>("transparency",
            [this]() { return m_transparency; },
            [this](float transparency) {
                m_transparency = transparency;
                m_frame = 0;
                setupTransparencyRandomness();
        })->setOptions({
                { "minimum", 0.0f },
                { "maximum", 1.0f },
                { "step", 0.1f },
                { "precision", 1u } });

        ppGroup->addProperty<int>("numSamples",
            [this]() {return m_numTransparencySamples; },
            [this](int numTransparencySamples) {
                m_numTransparencySamples = numTransparencySamples;
                m_frame = 0;
                setupTransparencyRandomness();
        });

        ppGroup->addProperty<bool>("ObjectBasedTransparency",
            [this]() {return m_useObjectBasedTransparency; },
            [this](bool useObjectBasedTransparency) {
                m_useObjectBasedTransparency = useObjectBasedTransparency;
                m_frame = 0;
                setupTransparencyRandomness();
        });

        ppGroup->addProperty<bool>("backFaceCulling",
            [this]() {return m_backFaceCulling; },
            [this](bool backFaceCulling) {
                m_backFaceCulling = backFaceCulling;
                m_frame = 0;
        });

        ppGroup->addProperty<bool>("backFaceCullingShadows",
            [this]() {return m_backFaceCullingShadows; },
            [this](bool backFaceCullingShadows) {
                m_backFaceCullingShadows = backFaceCullingShadows;
                m_frame = 0;
        });
    }
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

    GLint maxTextureSize = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
    //m_shadowMapWidth = std::max(maxTextureSize, m_shadowMapWidth);
    globjects::debug() << "GL_MAX_TEXTURE_SIZE: " << std::to_string(maxTextureSize) << std::endl;

    m_grid = make_ref<gloperate::AdaptiveGrid>();
    m_grid->setColor({0.6f, 0.6f, 0.6f});

    setupDrawable();
    setupProgram();
    setupProjection();
    setupFramebuffer();
    setupTransparencyRandomness();

    globjects::NamedString::create("/data/antianti/ssao.glsl", new globjects::File("data/antianti/ssao.glsl"));


}

void AntiAnti::checkAndBindTexture(int meshID, aiTextureType type, std::string uniformName, GLenum target)
{
    auto texture = m_sceneLoader.getTexture(meshID, type);
    bool uiae = texture.get() != nullptr;
    if (uniformName != "")
        m_program->setUniform(uniformName, uiae);
    if (texture)
        texture->bindActive(target);
}

void AntiAnti::onPaint()
{
    if (m_shadowsEnabled)
        drawShadowMap();

    if (m_viewportCapability->hasChanged())
    {
        m_viewportCapability->setChanged(false);
        
        updateFramebuffer();
    }

    glViewport(
        m_viewportCapability->x(),
        m_viewportCapability->y(),
        m_viewportCapability->width(),
        m_viewportCapability->height());

    if (m_dofAtCursor || m_inputCapability->ctrlPressed)
    {
        float depth = m_coordProvider->depthAt(m_inputCapability->lastMousePosition);

        if (depth < 1.0 - std::numeric_limits<float>::epsilon())
        {
            glm::vec3 mouseWorldPos = m_coordProvider->unproject(m_inputCapability->lastMousePosition, depth);

            float clickZDistance = -(m_cameraCapability->view() * glm::vec4(mouseWorldPos, 1.0)).z;

            if (glm::distance(clickZDistance, m_focalDepth) > 0.01f)
                property<float>("DepthOfField/focalDepth")->setValue(clickZDistance);
        }
    }

    const auto inputTransform = m_projectionCapability->projection() * m_cameraCapability->view();

    const bool cameraHasChanged = m_lastTransform != inputTransform;
    if (cameraHasChanged)
    {
        m_lastTransform = inputTransform;
        m_frame = 0;
    }

    glm::vec3 inputEye = m_cameraCapability->eye();
    glm::vec3 dofShiftedEye = inputEye;
    glm::vec3 viewVec = glm::normalize(m_cameraCapability->center() - inputEye) * m_focalDepth;
    glm::vec3 focalPoint = m_cameraCapability->eye() + viewVec;
    glm::vec2 shearingFactor = glm::vec2();

    if (m_dofEnabled && !cameraHasChanged)
    {
        if (m_usePointDoF)
        {
            glm::vec3 dofShift{ glm::diskRand(m_maxDofShift), 0.0f };
            glm::vec3 dofShiftWorld = glm::mat3(m_cameraCapability->view()) * dofShift;
            dofShiftedEye = inputEye + dofShiftWorld;
        }
        else
        {
            shearingFactor = glm::diskRand(m_maxDofShift);
        }
    }


    auto camera = make_ref<gloperate::Camera>(
        dofShiftedEye,
        focalPoint,
        m_cameraCapability->up());
    camera->setZNear(m_projectionCapability->zNear());
    camera->setZFar(m_projectionCapability->zFar());
    camera->setFovy(m_projectionCapability->fovy());
    camera->setAspectRatio(m_projectionCapability->aspectRatio());

    const auto transform = m_projectionCapability->projection() * camera->view();


    m_fbo->bind(GL_FRAMEBUFFER);
    m_fbo->clearBuffer(GL_COLOR, 0, glm::vec4{ 0.85f, 0.87f, 0.91f, 1.0f });
    m_fbo->clearBuffer(GL_COLOR, 1, glm::vec4{ 0.0f, 0.0f, 0.0f, 0.0f });
    m_fbo->clearBufferfi(GL_DEPTH_STENCIL, 0, 1.0f, 0);

    glm::vec2 aaShift = glm::vec2(
        glm::linearRand<float>(-m_maxSubpixelShift * 0.5f, m_maxSubpixelShift * 0.5f),
        glm::linearRand<float>(-m_maxSubpixelShift * 0.5f, m_maxSubpixelShift * 0.5f))
        / glm::vec2(m_viewportCapability->width(), m_viewportCapability->height());

    m_grid->setCamera(camera);
    m_grid->draw(aaShift, m_focalDepth, shearingFactor);
    
    glSet(GL_CULL_FACE, m_backFaceCulling);
    glCullFace(GL_BACK);

    auto time_now = static_cast<GLuint>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
    
    m_program->use();

    m_program->setUniform("subpixelShift", aaShift);
    m_program->setUniform("viewMatrix", camera->view());
    m_program->setUniform("projection", m_projectionCapability->projection());
    //m_program->setUniform(m_transformLocation, transform);
    m_program->setUniform("focalPlane", m_focalDepth);
    m_program->setUniform("shearingFactor", shearingFactor);
    //m_program->setUniform(m_timeLocation, time_now);
    m_program->setUniform("frame", m_frame);
    m_program->setUniform("viewport", glm::vec2{ m_viewportCapability->width(), m_viewportCapability->height() });
    m_program->setUniform("transparency", m_useObjectBasedTransparency ? 0.0f : m_transparency);
    m_program->setUniform("shadowsEnabled", m_shadowsEnabled);
    m_program->setUniform("light", m_lightPosition); // TODO let there be area lights
    m_program->setUniform("camera", camera->eye());
    m_program->setUniform("lightZRange", m_lightZRange);

    m_program->setUniform("transparencyNoise1DSamples", m_numTransparencySamples);
    m_program->setUniform("transparencyNoise1D", 1);
    m_program->setUniform("smap", 2);
    m_program->setUniform("diff", 3);
    m_program->setUniform("norm", 4);
    m_program->setUniform("spec", 5);
    m_program->setUniform("emis", 6);

    m_transparencyNoise->bindActive(GL_TEXTURE1);
    m_shadowMap->bindActive(GL_TEXTURE2);

    for (auto i = 0u; i < m_sceneLoader.m_drawables.size(); ++i)
    {
        if (m_useObjectBasedTransparency && !m_transparencyRandomness[i][m_frame % m_numTransparencySamples])
            continue;

        checkAndBindTexture(i, aiTextureType_DIFFUSE, "hasDiff", GL_TEXTURE3);
        checkAndBindTexture(i, aiTextureType_NORMALS, "hasNorm", GL_TEXTURE4);
        checkAndBindTexture(i, aiTextureType_HEIGHT, "", GL_TEXTURE4);
        checkAndBindTexture(i, aiTextureType_SPECULAR, "hasSpec", GL_TEXTURE5);
        checkAndBindTexture(i, aiTextureType_EMISSIVE, "hasEmis", GL_TEXTURE6);

        m_sceneLoader.m_drawables[i]->draw();
    }
    
    m_program->release();
    
    Framebuffer::unbind(GL_FRAMEBUFFER);


    glDisable(GL_CULL_FACE);

    bool continueRendering = m_frame < m_numFrames;

    if (continueRendering)
        ++m_frame;

    m_postProcessing.camera = camera;
    m_postProcessing.viewport = glm::vec2(m_viewportCapability->x(), m_viewportCapability->y());
    m_postProcessing.frame = continueRendering ? m_frame : std::numeric_limits<int>::max();

    m_postProcessing.process();


    
    const auto rect = std::array<GLint, 4>{{
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
        GL_COLOR_BUFFER_BIT, GL_NEAREST);
    
    m_fbo->blit(GL_COLOR_ATTACHMENT0, rect, targetfbo, drawBuffer, rect,
        GL_DEPTH_BUFFER_BIT, GL_NEAREST);
}

void AntiAnti::setupFramebuffer()
{
    m_colorAttachment = Texture::createDefault(GL_TEXTURE_2D);
    m_normalAttachment = Texture::createDefault(GL_TEXTURE_2D);
    m_depthAttachment = Texture::createDefault(GL_TEXTURE_2D);
    
    m_fbo = make_ref<Framebuffer>();

    m_fbo->attachTexture(GL_COLOR_ATTACHMENT0, m_colorAttachment);
    m_fbo->attachTexture(GL_COLOR_ATTACHMENT1, m_normalAttachment);
    m_fbo->attachTexture(GL_DEPTH_ATTACHMENT, m_depthAttachment);

    m_fbo->setDrawBuffers({ GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 });

    m_ppTexture = Texture::createDefault(GL_TEXTURE_2D);

    m_ppfbo = make_ref<Framebuffer>();
    m_ppfbo->attachTexture(GL_COLOR_ATTACHMENT0, m_ppTexture);

    updateFramebuffer();

    m_fbo->printStatus(true);
    m_ppfbo->printStatus(true);


    m_shadowMap = Texture::createDefault(GL_TEXTURE_2D);

    // trying to work around Intel HD 3000 bugs (always requires a color attachment)
    m_fboShadowing = make_ref<Framebuffer>();
    m_fboShadowing->attachTexture(GL_COLOR_ATTACHMENT0, m_shadowMap);


    m_renderTargetCapability->setRenderTarget(
        RenderTargetType::Depth,
        m_fbo,
        GL_DEPTH_ATTACHMENT,
        GL_DEPTH_COMPONENT);

    m_coordProvider = std::make_unique<CoordinateProvider>(
        m_cameraCapability,
        m_projectionCapability,
        m_viewportCapability, 
        m_renderTargetCapability);

    m_postProcessing.lastFrame = m_ppTexture;
    m_postProcessing.fbo = m_ppfbo;
    m_postProcessing.depthBufferTexture = m_depthAttachment;
    m_postProcessing.normalTexture = m_normalAttachment;
    m_postProcessing.colorTexture = m_colorAttachment;
    m_postProcessing.shadowMap = m_shadowMap;
    m_postProcessing.initialize();
}

void AntiAnti::setupProjection()
{
    static const auto zNear = 0.3f, zFar = 30.f, fovy = 50.f;

    m_projectionCapability->setZNear(zNear);
    m_projectionCapability->setZFar(zFar);
    m_projectionCapability->setFovy(radians(fovy));

    m_grid->setNearFar(zNear, zFar);
}

void AntiAnti::setupTransparencyRandomness()
{
    std::random_device rd;
    std::mt19937 g(rd());

    //object based
    std::vector<bool> randomness(m_numTransparencySamples);
    for (int i = 0; i < m_numTransparencySamples * (1 - m_transparency); i++)
        randomness[i] = true;
    
    m_transparencyRandomness.clear();
    for (auto& drawable : m_sceneLoader.m_drawables) {
        std::shuffle(randomness.begin(), randomness.end(), g);
        randomness[0] = true; // make sure the object is always rendered during camera movement
        m_transparencyRandomness.push_back(randomness);
    }

    //pixel based
    if (!m_transparencyNoise) {
        m_transparencyNoise = make_ref<Texture>(GL_TEXTURE_1D);
        m_transparencyNoise->setParameter(gl::GL_TEXTURE_MIN_FILTER, gl::GL_NEAREST);
        m_transparencyNoise->setParameter(gl::GL_TEXTURE_MAG_FILTER, gl::GL_NEAREST);
        m_transparencyNoise->setParameter(gl::GL_TEXTURE_WRAP_S, gl::GL_MIRRORED_REPEAT);
    }

    std::vector<float> noise(m_numTransparencySamples);
    for (GLint i = 0; i < m_numTransparencySamples; ++i)
        noise[i] = float(i) / float(m_numTransparencySamples);

    std::shuffle(noise.begin(), noise.end(), g);
    m_transparencyNoise->unbindActive(GL_TEXTURE1);
    m_transparencyNoise->image1D(0, GL_R32F, static_cast<GLsizei>(m_numTransparencySamples), 0, GL_RED, GL_FLOAT, noise.data());
}

void AntiAnti::setupDrawable()
{
    m_sceneLoader.load(SceneLoader::TRANSPARENCY_TEST);
}

void AntiAnti::setupProgram()
{
    static const auto shaderPath = std::string{"data/antianti/"};
    const auto shaderName = "geometry";
    
    const auto vertexShader = shaderPath + shaderName + ".vert";
    const auto geometryShader = shaderPath + shaderName + ".geom";
    const auto fragmentShader = shaderPath + shaderName + ".frag";
    auto depthUtilShader = Shader::fromFile(GL_FRAGMENT_SHADER, shaderPath + "depth_util.frag");
    auto transparencyUtilShader = Shader::fromFile(GL_FRAGMENT_SHADER, shaderPath + "transparency_util.frag");
    
    m_program = make_ref<Program>();
    m_program->attach(
        Shader::fromFile(GL_VERTEX_SHADER, vertexShader),
        Shader::fromFile(GL_GEOMETRY_SHADER, geometryShader),
        Shader::fromFile(GL_FRAGMENT_SHADER, fragmentShader),
        depthUtilShader, transparencyUtilShader);

    glBindAttribLocation(m_program->id(), 0, "a_vertex");
    glBindAttribLocation(m_program->id(), 1, "a_normal");

    m_programShadowing = make_ref<Program>();
    m_programShadowing->attach(
        Shader::fromFile(GL_VERTEX_SHADER, shaderPath + "shadowMap.vert"),
        Shader::fromFile(GL_FRAGMENT_SHADER, shaderPath + "shadowMap.frag"),
        depthUtilShader, transparencyUtilShader);
}

void AntiAnti::updateFramebuffer()
{
    static const auto numSamples = 4u;
    const auto width = m_viewportCapability->width(), height = m_viewportCapability->height();
    
    m_colorAttachment->image2D(0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
    m_normalAttachment->image2D(0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
    m_depthAttachment->image2D(0, GL_DEPTH_COMPONENT, width, height, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, nullptr);

    m_ppTexture->image2D(0, m_accTextureFormat, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
}

void AntiAnti::drawShadowMap()
{
    if (m_shadowMapParamsChanged)
    {
        m_shadowMap->image2D(0, m_shadowMapFormat, m_shadowMapWidth, m_shadowMapWidth, 0, GL_RED, GL_FLOAT, 0);

        m_shadowMapRenderbuffer = make_ref<Renderbuffer>();
        m_shadowMapRenderbuffer->storage(GL_DEPTH_COMPONENT24, m_shadowMapWidth, m_shadowMapWidth);
        m_fboShadowing->attachRenderBuffer(GL_DEPTH_ATTACHMENT, m_shadowMapRenderbuffer);

        m_fboShadowing->printStatus(true);
        m_shadowMapParamsChanged = false;
    }

    glEnable(GL_DEPTH_TEST);
    glSet(GL_CULL_FACE, m_backFaceCullingShadows);

    m_fboShadowing->bind(GL_FRAMEBUFFER);
    
    glViewport(0, 0, m_shadowMapWidth, m_shadowMapWidth);

    
    m_fboShadowing->clearBuffer(GL_COLOR, 0, vec4(1));
    m_fboShadowing->clearBuffer(GL_DEPTH, 0, vec4(1));

    auto lightViewDir = glm::normalize(m_lightFocus - m_lightPosition);

    auto lightShift = glm::diskRand(m_maxLightSourceShift);
    // https://stackoverflow.com/questions/10161553/rotate-a-vector-to-reach-another-vector
    auto rndDiscToViewDirAxis = glm::cross(lightViewDir, glm::vec3(0, 0, 1));
    float rndDiscToViewDirAngle = glm::asin(glm::length(rndDiscToViewDirAxis));

    auto orientedLightShift = glm::vec3(glm::rotate(glm::mat4(), rndDiscToViewDirAngle, rndDiscToViewDirAxis) * glm::vec4(lightShift, 0, 1));

    // shift the whole light viewport on its disk (don't rotate it around its center!)
    auto shiftedLightEye = m_lightPosition + orientedLightShift;
    auto shiftedLightCenter = m_lightFocus + orientedLightShift;
    auto lightUp = glm::cross(-shiftedLightEye, glm::vec3(-1, 1, 0));

    auto transform = 
        glm::perspective(m_projectionCapability->fovy(), 
            1.0f, m_lightZRange.x, m_lightZRange.y)
        * glm::lookAt(shiftedLightEye, shiftedLightCenter, lightUp);

    m_programShadowing->setUniform("lightZRange", m_lightZRange);
    m_programShadowing->setUniform("transform", transform);
    m_programShadowing->setUniform("linearizedShadowMap", m_linearizedShadowMap);

    //needed for transparency
    m_programShadowing->setUniform("frame", m_frame);
    m_programShadowing->setUniform("viewport", glm::vec2{ m_shadowMapWidth, m_shadowMapWidth });
    m_programShadowing->setUniform("transparency", m_useObjectBasedTransparency ? 0.0f : m_transparency);
    m_programShadowing->setUniform("transparencyNoise1DSamples", m_numTransparencySamples);
    m_programShadowing->setUniform("transparencyNoise1D", 1);

    m_programShadowing->use();

    m_transparencyNoise->bindActive(GL_TEXTURE1);

    for (auto i = 0u; i < m_sceneLoader.m_drawables.size(); ++i) {
        if (m_useObjectBasedTransparency && !m_transparencyRandomness[i][m_frame % m_numTransparencySamples])
            continue;
        m_sceneLoader.m_drawables[i]->draw();
    }


    m_programShadowing->release();

    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);

    m_fboShadowing->unbind();

    // transform depth NDC to texture coordinates for lookup in fragment shader
    auto shadowBias = glm::mat4(
        0.5f, 0.0f, 0.0f, 0.0f
        , 0.0f, 0.5f, 0.0f, 0.0f
        , 0.0f, 0.0f, 0.5f, 0.0f
        , 0.5f, 0.5f, 0.5f, 1.0f);
    m_program->setUniform("biasedDepthTransform", shadowBias * transform);
    m_program->setUniform("linearizedShadowMap", m_linearizedShadowMap);
}
