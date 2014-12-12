#pragma once

#include <SceneModel/SceneModel_fwd.hpp>
#include <tygra/WindowViewDelegate.hpp>
#include <tgl/tgl.h>
#include <glm/glm.hpp>
#include <vector>
#include <memory>

#include "ShaderProgram.hpp"

class MyView : public tygra::WindowViewDelegate
{
public:

    MyView();

    ~MyView();

    void
    setScene(std::shared_ptr<const SceneModel::Context> scene);

private:

    void
    windowViewWillStart(std::shared_ptr<tygra::Window> window) override;

    void
    windowViewDidReset(std::shared_ptr<tygra::Window> window,
                       int width,
                       int height) override;

    void
    windowViewDidStop(std::shared_ptr<tygra::Window> window) override;

    void
    windowViewRender(std::shared_ptr<tygra::Window> window) override;

    std::shared_ptr<const SceneModel::Context> scene_;

    float aspectRatio;

    struct Vertex
    {
        Vertex(){};
        Vertex(glm::vec3 pos_, glm::vec3 norm_) : position(pos_), normal(norm_) {}
        glm::vec3 position, normal;
    };
    GLuint vertexVBO; // VertexBufferObject for the vertex positions
    GLuint elementVBO; // VertexBufferObject for the elements (indices)

    struct Mesh
    {
        GLuint vao;// VertexArrayObject for the shape's vertex array settings
        GLuint instanceVBO;
        int startVerticeIndex, endVerticeIndex, verticeCount;
        int startElementIndex, endElementIndex, element_count; // Needed for when we draw using the vertex arrays

        Mesh() : startVerticeIndex(0),
            endVerticeIndex(0),
            verticeCount(0),
            startElementIndex(0),
            endElementIndex(0),
            element_count(0) {}
    };
    std::vector< Mesh > loadedMeshes;

    struct MaterialData
    {
        glm::vec3 colour;
        float shininess;
    };
    std::vector< MaterialData > materials;
    GLuint bufferMaterials;

    struct InstanceData
    {
        glm::mat4x3 positionData;
        GLint materialDataIndex;
    };
    std::vector< std::vector< InstanceData > > instanceData;

    // cant get access to the MyScene::Light since we are only declaring MyScene as a class (no direct reference)
    struct LightData
    {
        glm::vec3 position;
        float range;
    };
    std::vector<LightData> lights;
    GLuint bufferRender;
    Mesh lightMesh, globalLightMesh;

    ShaderProgram lightProgram, firstPassProgram, globalLightProgram, backgroundProgram, postProcessProgram;

    GLuint gbufferFBO;
    GLuint gbufferTO[3];
    GLuint depthStencilRBO;

    GLuint lbufferFBO;
	GLuint lbufferTO;
    GLuint lbufferColourRBO;

	GLuint postProcessFBO;
	GLuint postProcessColourRBO;

    void SetBuffer(glm::mat4 projectMat_, glm::vec3 camPos_);
	void UpdateLights();
};
