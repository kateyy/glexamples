#include "SceneLoader.h"

#include <iostream>
#include <unordered_map>

#include <glbinding/gl/enum.h>

#include <globjects/Texture.h>

#include <gloperate/resources/RawFile.h>
#include <gloperate/primitives/Scene.h>
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
        { SceneLoader::C_SPONZA, "crytek-sponza/" }
    };
    std::unordered_map<SceneLoader::Scene, std::string> filenames = { 
        { SceneLoader::TRANSPARENCY_TEST, "transparency_scene.obj" },
        { SceneLoader::IMROD, "Imrod.obj" },
        { SceneLoader::D_SPONZA, "sponza.obj" },
        { SceneLoader::C_SPONZA, "sponza.obj" }
    };

    std::vector<aiTextureType> texTypesToLoad = { aiTextureType_DIFFUSE, aiTextureType_EMISSIVE, aiTextureType_HEIGHT, aiTextureType_NORMALS, aiTextureType_SPECULAR };
}


SceneLoader::SceneLoader()
: m_aiScene(nullptr)
{
}

ref_ptr<Texture> SceneLoader::getTexture(int meshIndex, aiTextureType type)
{
    unsigned int matIndex = m_aiScene->mMeshes[meshIndex]->mMaterialIndex;
    return m_textures[matIndex][type];
}

ref_ptr<Texture> loadTexture(std::string filepath, aiTextureType type)
{
    auto texture = make_ref<Texture>(GL_TEXTURE_2D);
    texture->setParameter(GL_TEXTURE_WRAP_S, GL_REPEAT);
    texture->setParameter(GL_TEXTURE_WRAP_T, GL_REPEAT);
    texture->setParameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    texture->setParameter(GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    QImage image = loadImage(filepath);

    texture->image2D(0, GL_RGBA8, glm::ivec2(image.width(), image.height()), 0, GL_RGBA, GL_UNSIGNED_BYTE, image.bits());
    texture->generateMipmap();
    return texture;
}

void SceneLoader::load(Scene scene)
{
    std::string directory = base_dir + directories[scene];
    std::string path = directory + filenames[scene];

    delete m_aiScene;
    m_aiScene = aiImportFile(
        path.c_str(),
        aiProcess_Triangulate |
        aiProcess_JoinIdenticalVertices |
        aiProcess_SortByPType |
        aiProcess_GenNormals);

    if (!m_aiScene)
        std::cout << aiGetErrorString();

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

    for (const auto * geometry : gloperatescene->meshes())
        m_drawables.push_back(widgetzeug::make_unique<gloperate::PolygonalDrawable>(*geometry));

    delete gloperatescene;
}