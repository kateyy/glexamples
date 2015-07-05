#include "SceneLoader.h"

#include <iostream>
#include <unordered_map>

#include <glbinding/gl/enum.h>

#include <globjects/Texture.h>
#include <globjects/globjects.h>

#include <gloperate/resources/RawFile.h>
#include <gloperate/primitives/Scene.h>
#include <gloperate/primitives/PolygonalDrawable.h>
#include <gloperate/primitives/PolygonalGeometry.h>
#include <gloperate-assimp/AssimpSceneLoader.h>

#include <widgetzeug/make_unique.hpp>

#include <assimp/scene.h>
#include <assimp/cimport.h>
#include <assimp/types.h>
#include <assimp/postprocess.h>
#include <assimp/material.h>

#include <QImage>

#include "ImageLoader.h"


using namespace gl;
using namespace globjects;

namespace
{
    std::string base_dir = "data/antianti/meshes/";
    std::unordered_map<SceneLoader::Scene, std::string> directories = {
        { SceneLoader::TRANSPARENCY_TEST, "" },
        { SceneLoader::IMROD, "Imrod/" },
        { SceneLoader::D_SPONZA, "dabrovic-sponza/" },
        {SceneLoader::C_SPONZA, "crytek-sponza/"},
        { SceneLoader::MITSUBA, "mitsuba/" },
        { SceneLoader::MEGACITY_SMALL, "megacity/" },
        { SceneLoader::JAKOBI, "jakobi/" },
    };
    std::unordered_map<SceneLoader::Scene, std::string> filenames = { 
        { SceneLoader::TRANSPARENCY_TEST, "transparency_scene.obj" },
        { SceneLoader::IMROD, "Imrod.obj" },
        { SceneLoader::D_SPONZA, "sponza.obj" },
        {SceneLoader::C_SPONZA, "sponza.obj"},
        { SceneLoader::MITSUBA, "mitsuba.obj" },
        { SceneLoader::MEGACITY_SMALL, "simple2.obj" },
        { SceneLoader::JAKOBI, "jakobikirchplatz4.obj" },
    };

    std::vector<aiTextureType> texTypesToLoad = { aiTextureType_DIFFUSE, aiTextureType_EMISSIVE, aiTextureType_HEIGHT, aiTextureType_NORMALS, aiTextureType_SPECULAR };

    //                                    TRANSPARENCY_TEST  IMROD              D_SPONZA           C_SPONZA           MITSUBA           MEGACITY_SMALL     JAKOBI
    std::vector<glm::vec2> nearFars =     {{0.3, 30.0},      {0.3, 70.0},       {0.3, 50.0},       {5.0, 3000.0},     {0.3, 30.0},      {0.3, 50.0},       {0.05, 8.0},       };
    std::vector<glm::vec3> camPositions = {{0.2, 1.5, -2.8}, {-4.5, 32.0, 8.7}, {15.5, 3.9, 0.1},  {-1300, 250, -23}, {0.25, 4.1, 4.75},{15.5, 3.9, 0.1},  {0.34, 0.39, -0.48},};
    std::vector<glm::vec3> camViews =     {{0.0, -1.5, 2.6}, {0.9, -1.9, -2.1}, {-3.0, -0.9, 0.0}, {3.0, -0.5, 0.0},  {0.0, -1.8, -2.4},{-3.0, -0.9, 0.0}, {-0.24, -0.37, 0.33},};
    std::vector<bool> enableGrid =        {true,             true,              false,             false,             false,            false,             false,             };
    std::vector<glm::vec2> ssaoSettings = {{0.05, 1.0},      {0.8, 1.5},        {0.5, 1.5},        {15.0, 1.5},       {0.5, 1.0},       {0.5, 1.5},        {0.1, 1.5},        };
    std::vector<glm::vec3> lightPositions = {{0, 20, 0},     {0, 54, 0},        {0, 18, 0},        {0, 2000, 0},      {10, 20, 0},      {0, 18, 0},        {-0.4, 1.2, -0.7}, };
    std::vector<float> lightMaxShifts =   {0.1f,             1.0f,              1.0f,              15.0f,             0.7f,             1.0f,              0.05f,             };
}


SceneLoader::SceneLoader()
    : m_aiScene(nullptr)
    , m_currentScene(UNINITIALIZED)
    , m_desiredScene(UNINITIALIZED)
{
}

glm::vec2 SceneLoader::getNearFar()
{
    return nearFars[m_currentScene];
}


glm::vec3 SceneLoader::getCameraPos()
{
    return camPositions[m_currentScene];
}

glm::vec3 SceneLoader::getCameraCenter()
{
    return camPositions[m_currentScene] + camViews[m_currentScene];
}

bool SceneLoader::getEnableGrid()
{
    return enableGrid[m_currentScene];
}

glm::vec2 SceneLoader::getSsaoSettings()
{
    return ssaoSettings[m_currentScene];
}

glm::vec3 SceneLoader::getLightPos()
{
    return lightPositions[m_currentScene];
}

float SceneLoader::getLightMaxShift()
{
    return lightMaxShifts[m_currentScene];
}


ref_ptr<Texture> SceneLoader::getTexture(int meshIndex, aiTextureType type)
{
    unsigned int matIndex = m_aiScene->mMeshes[meshIndex]->mMaterialIndex;
    return m_textures[matIndex][type];
}

bool SceneLoader::update()
{
    if (m_currentScene != m_desiredScene)
    {
        load(m_desiredScene);
        return true;
    }
    return false;
}

ref_ptr<Texture> loadTexture(std::string filepath, aiTextureType type)
{
    auto texture = make_ref<Texture>(GL_TEXTURE_2D);
    texture->setParameter(GL_TEXTURE_WRAP_S, GL_REPEAT);
    texture->setParameter(GL_TEXTURE_WRAP_T, GL_REPEAT);
    texture->setParameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    texture->setParameter(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    texture->setParameter(GL_TEXTURE_MAX_ANISOTROPY_EXT, globjects::getFloat(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT));

    QImage image = loadImage(filepath);

    texture->image2D(0, GL_RGBA8, glm::ivec2(image.width(), image.height()), 0, GL_RGBA, GL_UNSIGNED_BYTE, image.bits());
    texture->generateMipmap();
    return texture;
}

void SceneLoader::load(Scene scene)
{
    std::string directory = base_dir + directories[scene];
    std::string path = directory + filenames[scene];

    aiReleaseImport(m_aiScene);
    m_aiScene = aiImportFile(
        path.c_str(),
        aiProcess_Triangulate |
        aiProcess_JoinIdenticalVertices |
        aiProcess_SortByPType |
        aiProcess_GenNormals);

    if (!m_aiScene)
        std::cout << aiGetErrorString();

    m_textures.clear();
    m_textures.resize(m_aiScene->mNumMaterials);
    if (scene == TRANSPARENCY_TEST)
    {
        m_textures[0].resize(aiTextureType_UNKNOWN + 1);
    }
    else if (scene == IMROD)
    {
        m_textures[0].resize(aiTextureType_UNKNOWN + 1);
        std::string dir = base_dir + directories[scene];
        m_textures[0][aiTextureType_DIFFUSE] = loadTexture(dir + "Imrod_diffuse.png", aiTextureType_DIFFUSE);
        m_textures[0][aiTextureType_EMISSIVE] = loadTexture(dir + "Imrod_emission.png", aiTextureType_EMISSIVE);
        m_textures[0][aiTextureType_NORMALS] = loadTexture(dir + "Imrod_normals.png", aiTextureType_NORMALS);
        m_textures[0][aiTextureType_SPECULAR] = loadTexture(dir + "Imrod_specular.png", aiTextureType_SPECULAR);
    }
    else
    {
        for (unsigned int m = 0; m < m_aiScene->mNumMaterials; m++)
        {
            auto mat = m_aiScene->mMaterials[m];
            m_textures[m].resize(aiTextureType_UNKNOWN+1); // highest texture type

            for (aiTextureType type : texTypesToLoad)
            {
                aiString texPath;
                aiReturn ret = mat->GetTexture(type, 0, &texPath, NULL, NULL, NULL, NULL, NULL);
                if (ret != aiReturn_SUCCESS)
                    continue;
                std::string texPathStd = std::string(texPath.C_Str());
                if (m_textures[m][type])
                    continue; //texture already loaded

                auto texture = loadTexture(directory + texPathStd, type);

                m_textures[m][type] = texture;
            }
        }
    }

    const auto gloperatescene = gloperate_assimp::AssimpSceneLoader().convertScene(m_aiScene);

    m_drawables.clear();
    for (const auto * geometry : gloperatescene->meshes())
    {
        auto * geo = geometry;
        if (scene == MEGACITY_SMALL)
        {
            auto newGeo = new gloperate::PolygonalGeometry();

            std::vector<glm::vec3> newVerts;
            for (auto && v : geometry->vertices())
            {
                newVerts.push_back(v * 10.0f);
            }
            newGeo->setVertices(newVerts);
            newGeo->setIndices(geometry->indices());
            newGeo->setTexCoords(geometry->texCoords());
            newGeo->setNormals(geometry->normals());

            geo = newGeo;
        }

        m_drawables.push_back(widgetzeug::make_unique<gloperate::PolygonalDrawable>(*geo));
    }

    delete gloperatescene;

    m_currentScene = scene;
}