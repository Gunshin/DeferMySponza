#include "MyView.hpp"
#include <SceneModel/SceneModel.hpp>
#include <tygra/FileHelper.hpp>
#include <tsl/primitives.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <cassert>

#include <map>

MyView::
MyView()
{
}

MyView::
~MyView() {
}

// school purple = 108 39 135

void MyView::
setScene(std::shared_ptr<const SceneModel::Context> scene)
{
    scene_ = scene;
}

void MyView::
windowViewWillStart(std::shared_ptr<tygra::Window> window)
{
    assert(scene_ != nullptr);

    SceneModel::GeometryBuilder builder = SceneModel::GeometryBuilder();

    Shader vs, fs;
    vs.loadShader("defer_vs.glsl", GL_VERTEX_SHADER);
    fs.loadShader("defer_fs.glsl", GL_FRAGMENT_SHADER);

    shaderProgram.createProgram();
    shaderProgram.addShaderToProgram(&vs);
    shaderProgram.addShaderToProgram(&fs);
    shaderProgram.linkProgram();

    shaderProgram.useProgram();

    /*
    generate a map which contains the MaterialID as the key, which leads to the index inside of my vector that the material is contained
    */
    auto mapMaterialIndex = std::map<SceneModel::MaterialId, unsigned int>();

    for (unsigned int i = 0; i < scene_->getAllMaterials().size(); ++i)
    {
        mapMaterialIndex[scene_->getAllMaterials()[i].getId()] = materials.size();

        MaterialData data;
        data.colour = scene_->getAllMaterials()[i].getColour();
        data.shininess = scene_->getAllMaterials()[i].getShininess();
        materials.push_back(data);
    }

    std::vector<SceneModel::Mesh> meshes = builder.getAllMeshes();
    instanceData.resize(meshes.size());

    for (unsigned int i = 0; i < meshes.size(); ++i)
    {
        
        std::vector<SceneModel::InstanceId> ids = scene_->getInstancesByMeshId(meshes[i].getId());

        for (unsigned int j = 0; j < ids.size(); ++j)
        {
            InstanceData instance;
            instance.positionData = scene_->getInstanceById(ids[j]).getTransformationMatrix();
            instance.materialDataIndex = static_cast< GLint >(mapMaterialIndex[scene_->getInstanceById(ids[j]).getMaterialId()]);
            instanceData[i].push_back(instance);

        }

    }

    // setup material SSBO
    glGenBuffers(1, &bufferMaterials);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, bufferMaterials);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(MaterialData)* materials.size(), &materials[0], GL_STREAM_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, bufferMaterials);
    glShaderStorageBlockBinding(
        shaderProgram.getProgramID(),
        glGetUniformBlockIndex(shaderProgram.getProgramID(), "BufferMaterials"),
        0);

    //copy lights data
    //UpdateLightData();

    // set up light SSBO
    glGenBuffers(1, &bufferRender);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, bufferRender);
    unsigned int size = sizeof(glm::mat4) + sizeof(glm::vec3) + sizeof(LightData)+sizeof(float); // since our buffer only has a projection matrix, camposition and single light source
    glBufferData(GL_SHADER_STORAGE_BUFFER, size, NULL, GL_STREAM_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, bufferRender);
    glShaderStorageBlockBinding(
        shaderProgram.getProgramID(),
        glGetUniformBlockIndex(shaderProgram.getProgramID(), "BufferRender"),
        1);

    //load scene meshes
    std::vector<Vertex> vertices;
    std::vector< unsigned int > elements;
    for (unsigned int i = 0; i < meshes.size(); ++i)
    {
        Mesh mesh;
        mesh.startVerticeIndex = vertices.size();
        mesh.startElementIndex = elements.size();

        // i store these temporarily since getPositionArray() will likely end up copying the whole array rather than passing the original
        std::vector<glm::vec3> positionArray = meshes[i].getPositionArray();
        std::vector<glm::vec3> normalArray = meshes[i].getNormalArray();

        for (unsigned int j = 0; j < positionArray.size(); ++j)
        {
            vertices.push_back(Vertex(positionArray[j],
                normalArray[j]));
        }

        // why is the element array a vector<InstanceId>?
        std::vector<unsigned int> elementArray = meshes[i].getElementArray();
        for (unsigned int j = 0; j < elementArray.size(); ++j)
        {
            elements.push_back(elementArray[j]);
        }

        mesh.endVerticeIndex = vertices.size() - 1;
        mesh.endElementIndex = elements.size() - 1;
        mesh.verticeCount = mesh.endVerticeIndex - mesh.startVerticeIndex;
        mesh.element_count = mesh.endElementIndex - mesh.startElementIndex + 1;
        loadedMeshes.push_back(mesh);
    }

    // set up vao
    glGenBuffers(1, &vertexVBO);
    glBindBuffer(GL_ARRAY_BUFFER, vertexVBO);
    glBufferData(GL_ARRAY_BUFFER,
        vertices.size() * sizeof(Vertex),
        vertices.data(),
        GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glGenBuffers(1, &elementVBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementVBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
        elements.size() * sizeof(unsigned int),
        elements.data(),
        GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    for (unsigned int i = 0; i < meshes.size(); ++i)
    {
        glGenBuffers(1, &loadedMeshes[i].instanceVBO);
        glBindBuffer(GL_ARRAY_BUFFER, loadedMeshes[i].instanceVBO);
        glBufferData(GL_ARRAY_BUFFER,
            instanceData[i].size() * sizeof(InstanceData),
            instanceData[i].data(),
            GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        unsigned int offset = 0;

        glGenVertexArrays(1, &loadedMeshes[i].vao);
        glBindVertexArray(loadedMeshes[i].vao);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementVBO);
        glBindBuffer(GL_ARRAY_BUFFER, vertexVBO);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
            sizeof(Vertex), TGL_BUFFER_OFFSET(offset));
        offset += sizeof(glm::vec3);

        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE,
            sizeof(Vertex), TGL_BUFFER_OFFSET(offset));
        offset += sizeof(glm::vec3);

        unsigned int instanceOffset = 0;
        glBindBuffer(GL_ARRAY_BUFFER, loadedMeshes[i].instanceVBO);

        for (int a = 2; a < 6; ++a)
        {
            glEnableVertexAttribArray(a);
            glVertexAttribPointer(a, 3, GL_FLOAT, GL_FALSE,
                sizeof(InstanceData), TGL_BUFFER_OFFSET(instanceOffset));
            glVertexAttribDivisor(a, 1);
            instanceOffset += sizeof(glm::vec3);
        }

        glEnableVertexAttribArray(6);
        glVertexAttribPointer(6, 1, GL_FLOAT, GL_FALSE,
            sizeof(InstanceData), TGL_BUFFER_OFFSET(instanceOffset));
        glVertexAttribDivisor(6, 1);
        instanceOffset += sizeof(GLint);

        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glBindVertexArray(0);

    }

    
}

void MyView::
windowViewDidReset(std::shared_ptr<tygra::Window> window,
                   int width,
                   int height)
{
    glViewport(0, 0, width, height);
}

void MyView::
windowViewDidStop(std::shared_ptr<tygra::Window> window)
{
}

void MyView::
windowViewRender(std::shared_ptr<tygra::Window> window)
{
    assert(scene_ != nullptr);

    glClearColor(0.f, 0.f, 0.25f, 0.f);
    glClear(GL_COLOR_BUFFER_BIT);

    glm::mat4 projectionMatrix = glm::perspective(75.f, aspectRatio, 1.f, 1000.f);
    glm::mat4 viewMatrix = glm::lookAt(scene_->getCamera().getPosition(), scene_->getCamera().getDirection() + scene_->getCamera().getPosition(), glm::vec3(0, 1, 0));
    glm::mat4 projectionViewMatrix = projectionMatrix * viewMatrix;

    SetBuffer(projectionViewMatrix, scene_->getCamera().getPosition(), LightData());

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glDepthMask(GL_TRUE);

    for (unsigned int i = 0; i < loadedMeshes.size(); ++i)
    {
        glBindVertexArray(loadedMeshes[i].vao);
        glDrawElementsInstancedBaseVertex(GL_TRIANGLES,
            loadedMeshes[i].element_count,
            GL_UNSIGNED_INT,
            TGL_BUFFER_OFFSET(loadedMeshes[i].startElementIndex * sizeof(int)),
            instanceData[i].size(),
            loadedMeshes[i].startVerticeIndex);
    }
}

void MyView::SetBuffer(glm::mat4 projectMat_, glm::vec3 camPos_, LightData light_)
{
    // so since glMapBufferRange does not work, i am going to create a temporary buffer for the per model data, and then copy the full buffer straight into the shaders buffer
    unsigned int bufferSize = sizeof(projectMat_)+sizeof(camPos_)+sizeof(float)/*padding0*/ +sizeof(LightData);
    char* buffer = new char[bufferSize];
    unsigned int index = 0;

    //projection matrix first!
    memcpy(buffer + index, glm::value_ptr(projectMat_), sizeof(glm::mat4));
    index += sizeof(projectMat_);

    // camera position next!
    memcpy(buffer + index, glm::value_ptr(camPos_), sizeof(camPos_));
    index += sizeof(camPos_)+sizeof(float);//padding

    // finally the LIGHTS!
    memcpy(buffer + index, &light_, sizeof(LightData));

    // update the render buffer, so get the pointer!
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, bufferRender);
    GLvoid* p = glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_WRITE_ONLY);
    memcpy(p, buffer, bufferSize);
    //done
    glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);

    delete[] buffer;
}