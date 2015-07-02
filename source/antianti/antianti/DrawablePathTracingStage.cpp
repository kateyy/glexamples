#include "DrawablePathTracingStage.h"

#include <cassert>

#include <glbinding/gl/enum.h>

#include <gloperate/primitives/Scene.h>
#include <gloperate/primitives/PolygonalGeometry.h>

using namespace globjects;
using namespace gloperate;
using namespace gl;


DrawablePathTracingStage::DrawablePathTracingStage()
    : GenericPathTracingStage({
        {"geometryTraversal", "data/antianti/pathtracing/geometryTraversal.glsl"},
        { "materials", "data/antianti/pathtracing/materials.glsl" },
        { "shadowRayCast", "data/antianti/pathtracing/shadowRayCast.glsl" },
        {"secondOrderRayGeneration", "data/antianti/pathtracing/secondOrderRayGeneration.glsl"} })
    , m_numTriangles(0)
    , m_reflectiveObjectIndex(0)
{
}

void DrawablePathTracingStage::loadScene(const gloperate::Scene & scene)
{
    std::vector<glm::uvec4> allIndices;
    std::vector<glm::vec4> allVertices;
    std::vector<glm::vec4> allNormals;

    GLuint indexOffset = 0;

    for (int meshIdx = 0; meshIdx < scene.meshes().size(); ++meshIdx)
    {
        const auto & mesh = scene.meshes()[meshIdx];

        const auto & indices = mesh->indices();
        const auto & vertices = mesh->vertices();
        const auto & normals = mesh->normals();

        assert(indices.size() % 3 == 0);
        for (int i = 0; i < indices.size(); i += 3)
        {
            allIndices.push_back(glm::uvec4(
                indices[i + 0] + indexOffset,
                indices[i + 1] + indexOffset,
                indices[i + 2] + indexOffset,
                static_cast<GLuint>(meshIdx)));
        }

        for (int i = 0; i < vertices.size(); ++i)
        {
            allVertices.push_back(glm::vec4(vertices[i], 0.0f));
        }
        for (int i = 0; i < normals.size(); ++i)
        {
            allNormals.push_back(glm::vec4(normals[i], 0.0f));
        }

        assert(normals.size() == vertices.size());
    
        indexOffset += static_cast<GLuint>(vertices.size());
    }

    m_numTriangles = static_cast<GLuint>(allIndices.size());

    m_indices = new Buffer();
    m_indices->setData(allIndices, GL_STATIC_DRAW);

    m_vertices = new Buffer();
    m_vertices->setData(allVertices, GL_STATIC_DRAW);

    m_normals = new Buffer();
    m_normals->setData(allNormals, GL_STATIC_DRAW);
}

void DrawablePathTracingStage::setReflectiveObjectIndex(GLuint index)
{
    m_reflectiveObjectIndex = index;
}

GLuint DrawablePathTracingStage::reflectiveObjectIndex() const
{
    return m_reflectiveObjectIndex;
}

void DrawablePathTracingStage::initialize()
{
    GenericPathTracingStage::initialize();

    setReflectiveObjectIndex(m_reflectiveObjectIndex);
}

void DrawablePathTracingStage::preRender()
{
    m_indices->bindBase(GL_SHADER_STORAGE_BUFFER, 6);
    m_vertices->bindBase(GL_SHADER_STORAGE_BUFFER, 7);
    m_normals->bindBase(GL_SHADER_STORAGE_BUFFER, 8);

    setUniform("numTriangles", m_numTriangles);
    setUniform<GLuint>("reflectiveObjectIndex", m_reflectiveObjectIndex);
}

void DrawablePathTracingStage::postRender()
{
    Buffer::unbind(GL_SHADER_STORAGE_BUFFER, 6);
    Buffer::unbind(GL_SHADER_STORAGE_BUFFER, 7);
    Buffer::unbind(GL_SHADER_STORAGE_BUFFER, 8);
}
