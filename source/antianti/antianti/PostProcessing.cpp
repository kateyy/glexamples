#include "PostProcessing.h"

#include <glm/glm.hpp>
#include <glm/gtc/random.hpp>
#include <glm/gtc/noise.hpp>

#include <glbinding/gl/bitfield.h>
#include <glbinding/gl/enum.h>
#include <glbinding/gl/functions.h>

#include <globjects/Uniform.h>
#include <globjects/Program.h>
#include <globjects/Shader.h>
#include <globjects/Buffer.h>
#include <globjects/Texture.h>
#include <globjects/VertexArray.h>
#include <globjects/Framebuffer.h>
#include <globjects/Texture.h>

#include <gloperate/primitives/ScreenAlignedQuad.h>

#include <gloperate/painter/AbstractViewportCapability.h>
#include <gloperate/painter/AbstractCameraCapability.h>
#include <gloperate/painter/AbstractPerspectiveProjectionCapability.h>

namespace {

std::vector<glm::vec3> ssaoKernel(unsigned int size)
{
    static const float minDistance = 0.1f;
    const float inverseSize = 1.f / static_cast<float>(size);

    std::vector<glm::vec3> kernel;

    if (size == 1)
    {
        kernel.push_back(glm::vec3(0.f));

        return kernel;
    }

    while (kernel.size() < size)
    {
        glm::vec3 v = glm::sphericalRand(1.f);
        v.z = glm::abs(v.z);
        if (v.z < 0.1)
            continue;

        float scale = static_cast<float>(kernel.size()) * inverseSize;
        scale = scale * scale * (1.f - minDistance) + minDistance;

        v *= scale;

        kernel.push_back(v);
    }

    return kernel;
}

std::vector<glm::vec3> ssaoNoise(const unsigned int size, bool useSSAONoise)
{
    std::vector<glm::vec3> kernel;
    if (!useSSAONoise)
    {
        kernel.resize(size*size, glm::vec3(1.0, 0.0, 0.0));
    }

    for(unsigned y = 0; y < size; ++y)
    {
        for(unsigned x = 0; x < size; ++x)
        {
            glm::vec2 c = glm::circularRand(1.f);
            glm::vec3 v = glm::vec3(c.x, c.y, 0.0f);
            //glm::vec3 v = glm::sphericalRand(1.0f);

            kernel.push_back(v);
        }
    }

    return kernel;
}

globjects::Texture* ssaoKernelTexture(unsigned int size)
{
    globjects::Texture* texture = new globjects::Texture(gl::GL_TEXTURE_1D);
    texture->setParameter(gl::GL_TEXTURE_MIN_FILTER, gl::GL_NEAREST);
    texture->setParameter(gl::GL_TEXTURE_MAG_FILTER, gl::GL_NEAREST);
    texture->setParameter(gl::GL_TEXTURE_WRAP_S, gl::GL_MIRRORED_REPEAT);
    return texture;
}

globjects::Texture* ssaoNoiseTexture(unsigned int size)
{
    globjects::Texture* texture = new globjects::Texture(gl::GL_TEXTURE_2D);
    texture->setParameter(gl::GL_TEXTURE_MIN_FILTER, gl::GL_NEAREST);
    texture->setParameter(gl::GL_TEXTURE_MAG_FILTER, gl::GL_NEAREST);
    texture->setParameter(gl::GL_TEXTURE_WRAP_S, gl::GL_MIRRORED_REPEAT);
    texture->setParameter(gl::GL_TEXTURE_WRAP_T, gl::GL_MIRRORED_REPEAT);
    return texture;
}

const unsigned int kernelSize = 8;
const unsigned int noiseSize = 32;

}


PostProcessing::PostProcessing()
: frame(0)
, useSSAO(true)
, ssaoIntensity(1.0f)
, ssaoRadius(0.05f)
, output(Output::Source_Final)
{
}

void PostProcessing::initialize()
{
    m_screenAlignedQuad = new gloperate::ScreenAlignedQuad(globjects::Shader::fromFile(gl::GL_FRAGMENT_SHADER, "data/antianti/postprocessing.frag"));
    m_screenAlignedQuad->program()->attach(globjects::Shader::fromFile(gl::GL_FRAGMENT_SHADER, "data/antianti/ssao.glsl"));
    m_screenAlignedQuad->program()->setName("Screen Aligned Quad Program");
    m_screenAlignedQuad->vertexShader()->setName("Screen Aligned Quad Vertex Shader");
    m_screenAlignedQuad->fragmentShader()->setName("Screen Aligned Quad Fragment Shader");

    m_screenAlignedQuad->program()->setUniform("DepthTexture", 0);
    m_screenAlignedQuad->program()->setUniform("ColorTexture", static_cast<int>(1));
    m_screenAlignedQuad->program()->setUniform("NormalTexture", static_cast<int>(2));
    m_screenAlignedQuad->program()->setUniform("lastFrame", static_cast<int>(3));
    m_screenAlignedQuad->program()->setUniform("shadowMap", static_cast<int>(4));

    m_ssaoKernel = ssaoKernelTexture(kernelSize);
    m_ssaoNoise = ssaoNoiseTexture(noiseSize);

    m_screenAlignedQuad->program()->setUniform("ssaoKernelSampler", static_cast<int>(7));
    m_screenAlignedQuad->program()->setUniform("ssaoNoiseSampler", static_cast<int>(8));

    m_screenAlignedQuad->program()->setUniform("ssaoSamplerSizes", glm::vec4(kernelSize, 1.f/kernelSize, noiseSize, 1.f/noiseSize));


    m_screenAlignedQuad->program()->setUniform("lastFrame", static_cast<int>(5));

    m_screenAlignedQuad->program()->setUniform("useSSAO", false);
}

void PostProcessing::process()
{

    m_screenAlignedQuad->program()->setUniform("screenSize", glm::vec2(viewport.x, viewport.y));

    m_screenAlignedQuad->program()->setUniform("useSSAO", useSSAO);


    m_screenAlignedQuad->program()->setUniform("ssaoRadius", ssaoRadius);
    m_screenAlignedQuad->program()->setUniform("ssaoIntensity", ssaoIntensity);

    m_screenAlignedQuad->program()->setUniform("nearZ", camera->zNear());
    m_screenAlignedQuad->program()->setUniform("farZ", camera->zFar());

    m_screenAlignedQuad->program()->setUniform("projectionMatrix", camera->projection());
    m_screenAlignedQuad->program()->setUniform("viewMatrix", camera->view());
    m_screenAlignedQuad->program()->setUniform("projectionInverseMatrix", camera->projectionInverted());
    m_screenAlignedQuad->program()->setUniform("normalMatrix", camera->normal());

    render();
}

void PostProcessing::render()
{
    fbo->bind(gl::GL_FRAMEBUFFER);

    if (useSSAO)
    {
        m_ssaoKernel->image1D(0, gl::GL_RGBA32F, kernelSize, 0, gl::GL_RGB, gl::GL_FLOAT, ssaoKernel(kernelSize).data());
        m_ssaoNoise->image2D(0, gl::GL_RGBA32F, glm::ivec2(noiseSize), 0, gl::GL_RGB, gl::GL_FLOAT, ssaoNoise(noiseSize, useSSAONoise).data());
    }

    m_screenAlignedQuad->program()->setUniform("Source", static_cast<gl::GLint>(output));
    m_screenAlignedQuad->program()->setUniform("frame", frame);

    depthBufferTexture->bindActive(gl::GL_TEXTURE0);
    colorTexture->bindActive(gl::GL_TEXTURE1);
    normalTexture->bindActive(gl::GL_TEXTURE2);
    shadowMap->bindActive(gl::GL_TEXTURE4);

    m_ssaoKernel->bindActive(gl::GL_TEXTURE7);
    m_ssaoNoise->bindActive(gl::GL_TEXTURE8);

    lastFrame->bindActive(gl::GL_TEXTURE5);

    gl::glDisable(gl::GL_DEPTH_TEST);


    m_screenAlignedQuad->draw();
    gl::glEnable(gl::GL_DEPTH_TEST);

    depthBufferTexture->unbindActive(gl::GL_TEXTURE0);
    colorTexture->unbindActive(gl::GL_TEXTURE1);
    normalTexture->unbindActive(gl::GL_TEXTURE2);
    shadowMap->unbindActive(gl::GL_TEXTURE4);

    m_ssaoKernel->unbindActive(gl::GL_TEXTURE7);
    m_ssaoNoise->unbindActive(gl::GL_TEXTURE8);

    lastFrame->unbindActive(gl::GL_TEXTURE5);

    fbo->unbind(gl::GL_FRAMEBUFFER);
}
