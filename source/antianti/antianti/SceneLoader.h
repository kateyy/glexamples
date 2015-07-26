#pragma once

#include <vector>
#include <memory>

#include <glm/glm.hpp>

#include <globjects/base/ref_ptr.h>
#include <gloperate/primitives/PolygonalDrawable.h>


namespace globjects
{
    class Texture;
    template<typename T>
    class ref_ptr;
}
struct aiScene;
enum aiTextureType;



class SceneLoader
{
public:
    SceneLoader();

public:
    enum Scene {
        UNINITIALIZED = -1,
        TRANSPARENCY_TEST,
        IMROD,
        D_SPONZA,
        C_SPONZA,
        MITSUBA,
        MEGACITY_SMALL,
        JAKOBI,
    };

    Scene m_desiredScene;
    Scene m_currentScene;

    bool update();

    glm::vec2 getNearFar();
    glm::vec3 getCameraPos();
    glm::vec3 getCameraCenter();
    bool getEnableGrid();
    glm::vec2 getSsaoSettings();
    bool getEnableShadows();
    glm::vec3 getLightPos();
    float getLightMaxShift();
    bool getEnableTransparency();

    // a texture of type someType for drawable m_drawables[i] can be retrieved with 
    // getTexture(i, someType). returned texture is null if the drawable has no texture of that type.
    globjects::ref_ptr<globjects::Texture> getTexture(int meshIndex, aiTextureType type);
    std::vector<std::unique_ptr<gloperate::PolygonalDrawable>> m_drawables;

protected:
    void load(std::string path);
    void load(Scene scene);

    std::vector<std::vector<globjects::ref_ptr<globjects::Texture>>> m_textures;
    const aiScene* m_aiScene;
};
