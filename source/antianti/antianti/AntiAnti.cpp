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

#include <glkernel/sample.h>
#include <glkernel/shuffle.h>

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
        if (key == gloperate::KeyP)
            printCamPos = true;
    }

    void onKeyUp(gloperate::Key key) override
    {
        if (key == gloperate::KeyLeftControl || key == gloperate::KeyRightControl)
            ctrlPressed = false;
    }

    glm::vec2 lastMousePosition;
    bool ctrlPressed = false;
    bool printCamPos = false;
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
    , m_accTextureFormat(GL_RGB32F)
    , m_backgroundColor({ 255, 255, 255 })
    // dof
    , m_dofEnabled(false)
    , m_usePointDoF(false)
    , m_maxDofShift(0.01f)
    , m_focalDepth(3.0f)
    , m_dofSamples(1024)
    , m_dofSortSamples(true)
    , m_dofAtCursor(false)
    // shadows
    , m_shadowsEnabled(true)
    , m_lightPosition({0, 54, 0})
    , m_lightFocus({ 0, 0, 0 })
    , m_maxLightSourceShift(0.1f)
    , m_lightSamples(1024)
    , m_lightSortSamples(false)
    , m_linearizedShadowMap(true)
    , m_shadowMapParamsChanged(true)
    , m_shadowMapFormat(GL_R32F)
    , m_shadowMapWidth(2048)
    , m_lightZRange({0.300, 50.00})
    // transparency
    , m_enableTransparency(false)
    , m_backFaceCulling(false)
    , m_backFaceCullingShadows(false)
    , m_useObjectBasedTransparency(false)
    , m_transparency(0.5f)
    , m_numTransparencySamples(8192)
    , m_aaKernel(128)
{
    m_sceneLoader.m_desiredScene = SceneLoader::IMROD;
    setupPropertyGroup();

    auto num_samples = glkernel::sample::poisson_square(m_aaKernel, 30);
    m_aaKernel = m_aaKernel.trimed(num_samples, 1, 1);

    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(m_aaKernel.m_kernel.begin() + 1, m_aaKernel.m_kernel.end(), g);

    updateDofKernel();
    updateLightKernel();
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

    addProperty<SceneLoader::Scene>("scene",
        [this]() {return m_sceneLoader.m_desiredScene; },
        [this](SceneLoader::Scene scene) {
            m_sceneLoader.m_desiredScene = scene;
    })->setStrings({
        { SceneLoader::Scene::TRANSPARENCY_TEST, "Transparency Test" },
        { SceneLoader::Scene::IMROD, "Imrod" },
        { SceneLoader::Scene::D_SPONZA, "Dabrovic Sponza" },
        { SceneLoader::Scene::C_SPONZA, "Crytek Sponza" },
        { SceneLoader::Scene::MITSUBA, "Mitsuba" },
        { SceneLoader::Scene::MEGACITY_SMALL, "Megacity Small" },
        { SceneLoader::Scene::JAKOBI, "Jakobi" },
    });

    addProperty<glm::vec3>("cameraPosition",
        [this]() {return m_cameraCapability->eye(); },
        [this](glm::vec3 cameraPosition) {
            m_cameraCapability->setEye(cameraPosition);
            m_frame = 0;
    })->setOptions({
        { "precision", 1u },
    });

    addProperty<glm::vec3>("cameraDirection",
        [this]() {return m_cameraCapability->center() - m_cameraCapability->eye(); },
        [this](glm::vec3 cameraDirection) {
        m_cameraCapability->setCenter(m_cameraCapability->eye() + cameraDirection);
        m_frame = 0;
    })->setOptions({
        { "precision", 1u },
    });

    this->addProperty<reflectionzeug::Color>("backgroundColor",
        [this]() {return m_backgroundColor; },
        [this](reflectionzeug::Color color) {
            m_backgroundColor = color;
            m_frame = 0;
    });



    enum CameraPreset {
        NOTHING = -1,
        IMROD_TEST,
        JAKOBI_TEST,
        JAKOBI_SSAO,
        JAKOBI_AA,
        TRANSPARENCY_DOF,
        D_SPONZA_LIGHTING,
        C_SPONZA_AA,
    };

    std::vector<std::pair<glm::vec3, glm::vec3>> cameraPresets = {
        { { -4.42347, 32, 8.55784 }, { 0.900001, -1.9, -2.09999 } },
        { { 0.348432, 0.415102, -0.488417 }, { -0.232397, -0.386651, 0.318336 } },
        {{0.252521, -0.00485591, -0.4329}, {-0.0350579, -0.0328542, 0.0745505}},
        {{0.474203, 0.489727, -0.182752}, {-0.315758, -0.43728, 0.111729}},
        {{1.0575, 0.7301, -1.59997}, {-0.618056, -0.782045, 1.98035}},
        {{8.22877, 2.69668, 0.759895}, {-0.000507355, -2.69668, -0.0926592}},
        {{-804.44, 208.115, -40.5258}, {27849.7, 2296.53, 214.473}},
    };

    addProperty<CameraPreset>("cameraPresets",
        [this]() {return CameraPreset::NOTHING; },
        [this, cameraPresets](CameraPreset preset) {
        if (preset == CameraPreset::NOTHING)
            return;
        m_cameraCapability->setEye(cameraPresets[preset].first);
        m_cameraCapability->setCenter(m_cameraCapability->eye() + cameraPresets[preset].second);
        m_frame = 0;
    })->setStrings({
        { CameraPreset::NOTHING, "Choose..." },
        { CameraPreset::IMROD_TEST, "Transparency Test" },
        { CameraPreset::JAKOBI_TEST, "Jakobi test" },
        {CameraPreset::JAKOBI_SSAO, "JAKOBI_SSAO"},
        {CameraPreset::JAKOBI_AA, "JAKOBI_AA"},
        {CameraPreset::TRANSPARENCY_DOF, "TRANSPARENCY_DOF"},
        {CameraPreset::D_SPONZA_LIGHTING, "D_SPONZA_LIGHTING"},
        {CameraPreset::C_SPONZA_AA, "C_SPONZA_AA"},
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

        dofGroup->addProperty<int>("numSamples",
            [this]() { return (int)m_dofKernel.size(); },
            [this](int numSamples) {
            if (numSamples == m_dofKernel.size())
                return;
            m_dofSamples = numSamples;
            updateDofKernel();
            m_frame = 0;
        });

        dofGroup->addProperty<bool>("sortSamples",
            [this]() { return m_dofSortSamples; },
            [this](bool sortSamples) {
            m_dofSortSamples = sortSamples;
            updateDofKernel();
            m_frame = 0;
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
        ssaoGroup->addProperty<bool>("useSSAONoise",
            [this]() {return m_postProcessing.useSSAONoise; },
            [this](bool useSSAONoise) {
            m_postProcessing.useSSAONoise = useSSAONoise;
            m_frame = 0;
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

        shadows->addProperty<int>("numSamples",
            [this]() { return (int)m_lightKernel.size(); },
            [this](int numSamples) {
            if (numSamples == m_lightKernel.size())
                return;
            m_lightSamples = numSamples;
            updateLightKernel();
            m_frame = 0;
        });

        shadows->addProperty<bool>("sortSamples",
            [this]() { return m_lightSortSamples; },
            [this](bool sortSamples) {
            m_lightSortSamples = sortSamples;
            updateLightKernel();
            m_frame = 0;
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
                m_postProcessing.output = o;
                m_frame = 0;})
            ->setStrings({
                {PostProcessing::Output::Source_Final, "Final" },
                {PostProcessing::Output::Source_Color, "Color" },
                {PostProcessing::Output::Source_Normals, "Normals"},
                {PostProcessing::Output::Source_Geometry, "Geometry"},
                {PostProcessing::Output::Source_Depth, "Depth"},
                {PostProcessing::Output::Source_OcclusionMap, "Occlusion Map"},
                {PostProcessing::Output::Source_ShadowMap, "ShadowMap"}
            });

        ppGroup->addProperty<GLenum>("textureFormat",
            [this] () {return m_accTextureFormat; },
            [this] (GLenum accTextureFormat) {
                m_accTextureFormat = accTextureFormat;
                updateFramebuffer();
                m_frame = 0;
        })->setStrings({
            { GLenum::GL_RGB8, "GL_RGB8" },
            { GLenum::GL_RGB10, "GL_RGB10" },
            { GLenum::GL_RGB12, "GL_RGB12" },
            { GLenum::GL_RGB16, "GL_RGB16" },
            { GLenum::GL_RGB16F, "GL_RGB16F" },
            { GLenum::GL_RGB32F, "GL_RGB32F" },
            { GLenum::GL_R11F_G11F_B10F, "GL_R11F_G11F_B10F" },
        });
    }

    {
        auto ppGroup = addGroup("Transparency");

        ppGroup->addProperty<bool>("enable",
            [this]() {return m_enableTransparency; },
            [this](bool enableTransparency) {
            m_enableTransparency = enableTransparency;
            m_frame = 0;
        });

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

    setupProgram();
    setupFramebuffer();

    globjects::NamedString::create("/data/antianti/ssao.glsl", new globjects::File("data/antianti/ssao.glsl"));
}

glkernel::kernel2 getKernel(int numSamples, bool sort)
{
    glkernel::kernel2 kernel(numSamples);
    auto actual_num_samples = glkernel::sample::poisson_square(kernel, 30);
    kernel = kernel.trimed(numSamples, 1, 1);

    for (int i = 0; i < kernel.size(); i++)
        kernel.m_kernel[i] = kernel.m_kernel[i] * 2.0f - 1.0f;

    if(sort)
    {
        std::sort(kernel.m_kernel.begin(), kernel.m_kernel.end(), [](glm::vec2 a, glm::vec2 b){
            return glm::length(a) < glm::length(b);
        });
    }
    else
    {
        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(kernel.m_kernel.begin() + 1, kernel.m_kernel.end(), g);
    }

    kernel.m_kernel.erase(std::remove_if(kernel.m_kernel.begin(), kernel.m_kernel.end(), [](glm::vec2 a){
        return glm::length(a) > 1.0f;
    }), kernel.m_kernel.end());

    return kernel;
}

void AntiAnti::updateDofKernel()
{
    m_dofKernel = getKernel(m_dofSamples, m_dofSortSamples);
}

void AntiAnti::updateLightKernel()
{
    m_lightKernel = getKernel(m_lightSamples, m_lightSortSamples);
}

void AntiAnti::checkAndBindTexture(int meshID, aiTextureType type, std::string uniformName, GLenum target)
{
    auto texture = m_sceneLoader.getTexture(meshID, type);
    bool uiae = texture.get() != nullptr;
    float hack = uiae ? 1.0f : 0.0f;
    if (uniformName != "")
        m_program->setUniform(uniformName, hack);
    if (texture)
        texture->bindActive(target);
}

void AntiAnti::checkAndUnbindTexture(int meshID, aiTextureType type, GLenum target)
{
    auto texture = m_sceneLoader.getTexture(meshID, type);
    if (texture)
        texture->bindActive(target);
}

void print_vec3(glm::vec3 v)
{
    std::cout << "{" << v.x << ", " << v.y << ", " << v.z << "}";
}

void AntiAnti::onPaint()
{
    bool sceneChanged = m_sceneLoader.update();
    if (sceneChanged)
    {
        setupTransparencyRandomness();

        vec2 nearFar = m_sceneLoader.getNearFar();
        float fovy = 50.0f;
        m_projectionCapability->setZNear(nearFar.x);
        m_projectionCapability->setZFar(nearFar.y);
        m_projectionCapability->setFovy(radians(fovy));

        m_shadowsEnabled = m_sceneLoader.getEnableShadows();
        m_lightZRange = nearFar;
        m_lightPosition = m_sceneLoader.getLightPos();
        m_maxLightSourceShift = m_sceneLoader.getLightMaxShift();

        m_cameraCapability->setEye(m_sceneLoader.getCameraPos());
        m_cameraCapability->setCenter(m_sceneLoader.getCameraCenter());
        m_postProcessing.ssaoRadius = m_sceneLoader.getSsaoSettings().x;
        m_postProcessing.ssaoIntensity = m_sceneLoader.getSsaoSettings().y;
        m_frame = 0;
    }

    if (m_inputCapability->printCamPos)
    {
        std::cout << "{";
        print_vec3(m_cameraCapability->eye());
        std::cout << ", ";
        print_vec3(m_cameraCapability->center() - m_cameraCapability->eye());
        std::cout << "}," << std::endl;
        m_inputCapability->printCamPos = false;
    }

    const auto inputTransform = m_projectionCapability->projection() * m_cameraCapability->view();

    const bool cameraHasChanged = m_lastTransform != inputTransform;

    if (cameraHasChanged)
    {
        m_lastTransform = inputTransform;
        m_frame = 0;
    }

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

    if (m_dofEnabled && (m_dofAtCursor || m_inputCapability->ctrlPressed))
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
            shearingFactor = m_dofKernel[m_frame % m_dofKernel.size()] * m_maxDofShift;
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
    m_fbo->clearBuffer(GL_COLOR, 0, glm::vec4{ m_backgroundColor.red() / 255.0, m_backgroundColor.green() / 255.0, m_backgroundColor.blue() / 255.0, 1.0f });
    m_fbo->clearBuffer(GL_COLOR, 1, glm::vec4{ 0.0f, 0.0f, 0.0f, 0.0f });
    m_fbo->clearBufferfi(GL_DEPTH_STENCIL, 0, 1.0f, 0);
    

    glm::vec2 aaShift = (m_aaKernel[m_frame%m_aaKernel.width()] - 0.5f) * m_maxSubpixelShift;
    aaShift /= glm::vec2(m_viewportCapability->width(), m_viewportCapability->height());

    if (m_sceneLoader.getEnableGrid()) {
        m_grid->setCamera(camera);
        m_grid->draw(aaShift, m_focalDepth, shearingFactor);
    }
    
    gl::glEnable(gl::GL_DEPTH_TEST);
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
    m_program->setUniform("transparency", (m_enableTransparency && !m_useObjectBasedTransparency) ? m_transparency : 0.0f);
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
        if (m_enableTransparency && m_useObjectBasedTransparency && !m_transparencyRandomness[i][m_frame % m_numTransparencySamples])
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
    m_postProcessing.viewport = glm::vec2(m_viewportCapability->width(), m_viewportCapability->height());
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
    m_ppTexture->setParameter(GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    m_ppTexture->setParameter(GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    m_ppfbo = make_ref<Framebuffer>();
    m_ppfbo->attachTexture(GL_COLOR_ATTACHMENT0, m_ppTexture);

    updateFramebuffer();

    m_fbo->printStatus(true);
    m_ppfbo->printStatus(true);


    m_shadowMap = Texture::createDefault(GL_TEXTURE_2D);
    {
        m_shadowMap->setParameter(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        m_shadowMap->setParameter(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        m_shadowMap->bind();
        glm::vec4 color(0.0);
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, (float*)&color);
    }

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
    glBindAttribLocation(m_program->id(), 2, "a_texCoord");

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

    m_ppTexture->image2D(0, m_accTextureFormat, width, height, 0, GL_RGB, GL_FLOAT, nullptr);
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
    lightShift = m_lightKernel[m_frame%m_lightKernel.size()] * m_maxLightSourceShift;
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
    m_programShadowing->setUniform("transparency", (m_enableTransparency && !m_useObjectBasedTransparency) ? m_transparency : 0.0f);
    m_programShadowing->setUniform("transparencyNoise1DSamples", m_numTransparencySamples);
    m_programShadowing->setUniform("transparencyNoise1D", 1);

    m_programShadowing->use();

    m_transparencyNoise->bindActive(GL_TEXTURE1);

    for (auto i = 0u; i < m_sceneLoader.m_drawables.size(); ++i) {
        if (m_enableTransparency && m_useObjectBasedTransparency && !m_transparencyRandomness[i][m_frame % m_numTransparencySamples])
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
