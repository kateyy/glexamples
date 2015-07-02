#pragma once

#include <vector>

#include <globjects/base/ref_ptr.h>
#include <globjects/Buffer.h>

#include <gloperate/rendering/GenericPathTracingStage.h>


namespace gloperate
{
    class Scene;
}

class DrawablePathTracingStage : public gloperate::GenericPathTracingStage
{
public:
    DrawablePathTracingStage();
    
    void loadScene(const gloperate::Scene & scene);

    void setReflectiveObjectIndex(gl::GLuint index);
    gl::GLuint reflectiveObjectIndex() const;


protected:
    void initialize() override;
    void preRender() override;
    void postRender() override;

private:
    gl::GLuint m_numTriangles;

    globjects::ref_ptr<globjects::Buffer> m_indices;
    globjects::ref_ptr<globjects::Buffer> m_vertices;
    globjects::ref_ptr<globjects::Buffer> m_normals;

    gl::GLuint m_reflectiveObjectIndex;
};
