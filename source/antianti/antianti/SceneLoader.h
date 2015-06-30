#pragma once

#include <vector>
#include <memory>

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
        IMROD,
        D_SPONZA,
        C_SPONZA
    };

    void load(Scene scene);

    // a texture of type someType for drawable m_drawables[i] can be retrieved with 
    // getTexture(i, someType). returned texture is null if the drawable has no texture of that type.
    globjects::ref_ptr<globjects::Texture> getTexture(int meshIndex, aiTextureType type);
    std::vector<std::unique_ptr<gloperate::PolygonalDrawable>> m_drawables;

protected:
    void load(std::string path);

    std::vector<std::vector<globjects::ref_ptr<globjects::Texture>>> m_textures;
    const aiScene* m_aiScene;
};
