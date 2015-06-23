#include "AntiAnti.h"

#include <chrono>
#include <iostream>
#include <random>

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
#include <globjects/base/File.h>

#include <gloperate/base/RenderTargetType.h>
#include <gloperate/base/make_unique.hpp>
#include <gloperate/resources/ResourceManager.h>
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

#include <widgetzeug/make_unique.hpp>


using namespace gl;
using namespace glm;
using namespace globjects;
using namespace gloperate;

using widgetzeug::make_unique;

namespace
{
    GLuint transparencyNoise1DSamples = 1024;

    glm::vec3 lightSource{ 0, 5,  0 };
    GLuint shadowMapSize = 4096u;
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
    , m_transparency(0.5f)
    , m_backFaceCulling(false)
    , m_maxSubpixelShift(1.0f)
    , m_pointOrPlaneDoF(true)
    , m_maxDofShift(0.01f)
    , m_focalDepth(3.0f)
    , m_dofAtCursor(false)
    , m_maxLightSourceShift(0.1f)
{    
    setupPropertyGroup();
}

AntiAnti::~AntiAnti() = default;

void AntiAnti::setupPropertyGroup()
{
    addProperty<float>("transparency", this,
        &AntiAnti::transparency, &AntiAnti::setTransparency)->setOptions({
        { "minimum", 0.0f },
        { "maximum", 1.0f },
        { "step", 0.1f },
        { "precision", 1u } });

    addProperty<bool>("backFaceCulling", this,
        &AntiAnti::backFaceCulling, &AntiAnti::setBackFaceCulling);

    addProperty<float>("subPixelShift", this,
        &AntiAnti::subpixelShift, &AntiAnti::setSubpixelShift)->setOptions({
            { "minimum", 0.0f },
            { "step", 0.05f },
            { "precision", 2u },
    });


    addProperty<bool>("focalPlaneNotPoint",
        [this] () {return m_pointOrPlaneDoF; },
        [this] (bool doPoint) {
        m_pointOrPlaneDoF = doPoint;
        m_frame = 0;
    })->setOption("title", "Use Focal Plane (not Focal Point)");

    addProperty<float>("maxDofShift",
        [this] (){return m_maxDofShift; },
        [this] (float shift) {
        m_maxDofShift = shift;
        m_frame = 0;
    })->setOptions({
            { "minimum", 0.0f },
            { "step", 0.001f },
            { "precision", 3u },
    });

    addProperty<float>("focalDepth",
        [this] (){return m_focalDepth; },
        [this] (float d) {
        m_focalDepth = d;
        m_frame = 0;
    })->setOptions({
        { "minimum", 0.0f },
        { "step", 0.05f },
        { "precision", 2u },
    });

    addProperty<bool>("doofAtCursor",
        [this] () {return m_dofAtCursor; },
        [this] (bool atCursor) {
        m_dofAtCursor = atCursor;
    });

    addProperty<bool>("useSSAO",
        [this]() {return m_postProcessing.useSSAO; },
        [this](bool useSSAO) {
        m_postProcessing.useSSAO = useSSAO;
        m_frame = 0;
    });

    addProperty<float>("SSAORadius",
        [this]() {return m_postProcessing.ssaoRadius; },
        [this](float ssaoRadius) {
        m_postProcessing.ssaoRadius = ssaoRadius;
        m_frame = 0;
    })->setOptions({
        { "minimum", 0.0f },
        { "step", 0.005f },
        { "precision", 3u },
    });

    addProperty<float>("SSAOIntensity",
        [this]() {return m_postProcessing.ssaoIntensity; },
        [this](float ssaoIntensity) {
        m_postProcessing.ssaoIntensity = ssaoIntensity;
        m_frame = 0;
    })->setOptions({
        { "minimum", 0.0f },
        { "step", 0.05f },
        { "precision", 2u },
    });

    addProperty<float>("LightSourceRadius", this,
        &AntiAnti::maxLightSourceShift,
        &AntiAnti::setMaxLightSourceShift)->setOptions({
        { "minimum", 0.0f },
        { "step", 0.05f },
        { "precision", 2u },
        { "title", "Light Radius" }
    });
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

bool AntiAnti::backFaceCulling() const
{
    return m_backFaceCulling;
}

void AntiAnti::setBackFaceCulling(bool backFaceCulling)
{
    m_backFaceCulling = backFaceCulling;
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

float AntiAnti::maxLightSourceShift() const
{
    return m_maxLightSourceShift;
}

void AntiAnti::setMaxLightSourceShift(float shift)
{
    m_maxLightSourceShift = shift;
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

    m_postProcessing.lastFrame = m_ppTexture;
    m_postProcessing.fbo = m_ppfbo;
    m_postProcessing.depthBufferTexture = m_depthAttachment;
    m_postProcessing.normalTexture = m_normalAttachment;
    m_postProcessing.colorTexture = m_colorAttachment;
    m_postProcessing.initialize();

    globjects::NamedString::create("/data/antianti/ssao.glsl", new globjects::File("data/antianti/ssao.glsl"));
}

void AntiAnti::onPaint()
{
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
                property<float>("focalDepth")->setValue(clickZDistance);
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

    if (!m_pointOrPlaneDoF)
    {
        // rotation around focal point
        if (!cameraHasChanged)
        {
            glm::vec3 dofShift{ glm::diskRand(m_maxDofShift), 0.0f };
            glm::vec3 dofShiftWorld = glm::mat3(m_cameraCapability->view()) * dofShift;
            dofShiftedEye = inputEye + dofShiftWorld;
        }
    }

    glm::vec2 shearingFactor = m_pointOrPlaneDoF ? glm::diskRand(m_maxDofShift) : glm::vec2();


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
    

    if (m_backFaceCulling)
    {
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
    }

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
    m_program->setUniform("transparency", m_transparency);
    m_program->setUniform("lightSource", lightSource);

    m_program->setUniform("transparencyNoise1DSamples", transparencyNoise1DSamples);
    m_program->setUniform("transparencyNoise1D", 1);
    m_program->setUniform("shadowMap", 2);

    m_transparencyNoise->bindActive(GL_TEXTURE1);
    m_shadowMap->bindActive(GL_TEXTURE2);

    for (auto i = 0u; i < m_drawables.size(); ++i)
    {
        //m_program->setUniform(m_transparencyLocation, i % 2 == 0 ? m_transparency : 1.0f);
        m_drawables[i]->draw();
    }
    
    m_program->release();
    
    Framebuffer::unbind(GL_FRAMEBUFFER);


    glDisable(GL_CULL_FACE);

    ++m_frame;

    m_postProcessing.camera = camera;
    m_postProcessing.viewport = glm::vec2(m_viewportCapability->x(), m_viewportCapability->y());
    m_postProcessing.frame = m_frame;

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
    m_shadowMap->image2D(0, GL_DEPTH_COMPONENT16, shadowMapSize, shadowMapSize, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);

    m_fboShadowing = make_ref<Framebuffer>();
    m_fboShadowing->attachTexture(GL_DEPTH_ATTACHMENT, m_shadowMap);
    m_fboShadowing->printStatus(true);


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


    std::vector<float> noise(transparencyNoise1DSamples);
    for (GLuint i = 0; i < transparencyNoise1DSamples; ++i)
        noise[i] = float(i) / float(transparencyNoise1DSamples);

    std::random_device rd;
    std::mt19937 g(rd());

    std::shuffle(noise.begin(), noise.end(), g);

    m_transparencyNoise = make_ref<Texture>(GL_TEXTURE_1D);
    m_transparencyNoise->setParameter(gl::GL_TEXTURE_MIN_FILTER, gl::GL_NEAREST);
    m_transparencyNoise->setParameter(gl::GL_TEXTURE_MAG_FILTER, gl::GL_NEAREST);
    m_transparencyNoise->setParameter(gl::GL_TEXTURE_WRAP_S, gl::GL_MIRRORED_REPEAT);
    m_transparencyNoise->image1D(0, GL_R32F, static_cast<GLsizei>(transparencyNoise1DSamples), 0, GL_RED, GL_FLOAT, noise.data());
}

void AntiAnti::setupProgram()
{
    static const auto shaderPath = std::string{"data/antianti/"};
    const auto shaderName = "geometry";
    
    const auto vertexShader = shaderPath + shaderName + ".vert";
    const auto fragmentShader = shaderPath + shaderName + ".frag";
    
    m_program = make_ref<Program>();
    m_program->attach(
        Shader::fromFile(GL_VERTEX_SHADER, vertexShader),
        Shader::fromFile(GL_FRAGMENT_SHADER, fragmentShader));

    glBindAttribLocation(m_program->id(), 0, "a_vertex");
    glBindAttribLocation(m_program->id(), 1, "a_normal");

    m_programShadowing = make_ref<Program>();
    m_programShadowing->attach(
        Shader::fromFile(GL_VERTEX_SHADER, shaderPath + "shadowMap.vert"),
        Shader::fromFile(GL_FRAGMENT_SHADER, shaderPath + "shadowMap.frag"));
}

void AntiAnti::updateFramebuffer()
{
    static const auto numSamples = 4u;
    const auto width = m_viewportCapability->width(), height = m_viewportCapability->height();
    
    m_colorAttachment->image2D(0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
    m_normalAttachment->image2D(0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
    m_depthAttachment->image2D(0, GL_DEPTH_COMPONENT, width, height, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, nullptr);

    m_ppTexture->image2D(0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
}

void AntiAnti::drawShadowMap()
{
    glEnable(GL_DEPTH_TEST);

    m_fboShadowing->bind(GL_FRAMEBUFFER);

    
    glViewport(0, 0, shadowMapSize, shadowMapSize);

    glClearDepth(1.f);
    glClear(GL_DEPTH_BUFFER_BIT);

    m_programShadowing->use();

    auto lightCenter = glm::vec3();
    auto lightViewDir = glm::normalize(lightCenter - lightSource);

    auto lightShift = glm::diskRand(m_maxLightSourceShift);
    // https://stackoverflow.com/questions/10161553/rotate-a-vector-to-reach-another-vector
    auto rndDiscToViewDirAxis = glm::cross(lightViewDir, glm::vec3(0, 0, 1));
    float rndDiscToViewDirAngle = glm::asin(glm::length(rndDiscToViewDirAxis));

    auto orientedLightShift = glm::vec3(glm::rotate(glm::mat4(), rndDiscToViewDirAngle, rndDiscToViewDirAxis) * glm::vec4(lightShift, 0, 1));

    // shift the whole light viewport on its disk (don't rotate it around its center!)
    auto shiftedLightEye = lightSource + orientedLightShift;
    auto shiftedLightCenter = lightCenter + orientedLightShift;
    auto lightUp = glm::cross(-shiftedLightEye, glm::vec3(-1, 1, 0));

    auto transform = 
        glm::perspective(m_projectionCapability->fovy(), 
            1.0f, m_projectionCapability->zNear(), m_projectionCapability->zFar())
        * glm::lookAt(shiftedLightEye, shiftedLightCenter, lightUp);

    m_programShadowing->setUniform("transform", transform);

    // transform depth NDC to texture coordinates for lookup in fragment shader
    auto shadowBias = glm::mat4(
        0.5f, 0.0f, 0.0f, 0.0f
        , 0.0f, 0.5f, 0.0f, 0.0f
        , 0.0f, 0.0f, 0.5f, 0.0f
        , 0.5f, 0.5f, 0.5f, 1.0f);
    m_program->setUniform("biasedDepthTransform", shadowBias * transform);

    for (auto i = 0u; i < m_drawables.size(); ++i)
        m_drawables[i]->draw();

    m_programShadowing->release();

    glDisable(GL_DEPTH_TEST);

    m_fboShadowing->unbind();
}
