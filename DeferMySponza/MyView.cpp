#include "MyView.hpp"
#include <SceneModel/SceneModel.hpp>
#include <tygra/FileHelper.hpp>
#include <tsl/primitives.hpp>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <cassert>

#include <map>

glm::vec3 ConvVec3(tsl::Vector3 &vec_);

MyView::
MyView()
{
}

MyView::
~MyView()
{
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
    
    {
        Shader vs, fs;
        vs.loadShader("firstpass_vs.glsl", GL_VERTEX_SHADER);
        fs.loadShader("firstpass_fs.glsl", GL_FRAGMENT_SHADER);

        firstPassProgram.createProgram();
        firstPassProgram.addShaderToProgram(&vs);
        firstPassProgram.addShaderToProgram(&fs);

		glBindFragDataLocation(firstPassProgram.getProgramID(), 0, "position");
		glBindFragDataLocation(firstPassProgram.getProgramID(), 1, "normal");
		glBindFragDataLocation(firstPassProgram.getProgramID(), 2, "material");

        firstPassProgram.linkProgram();

        firstPassProgram.useProgram();


    }

    {
        Shader vs, fs;
        vs.loadShader("light_vs.glsl", GL_VERTEX_SHADER);
        fs.loadShader("light_fs.glsl", GL_FRAGMENT_SHADER);

        lightProgram.createProgram();
        lightProgram.addShaderToProgram(&vs);
        lightProgram.addShaderToProgram(&fs);
        lightProgram.linkProgram();

        lightProgram.useProgram();
    }

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
            instance.materialDataIndex = static_cast<GLint>(mapMaterialIndex[scene_->getInstanceById(ids[j]).getMaterialId()]);
            instanceData[i].push_back(instance);

        }

    }

    // add the initial light instance data so that the buffer is at least initialised
    {
        std::vector<SceneModel::Light> sceneLights = scene_->getAllLights();
        for (unsigned int i = 0; i < scene_->getAllLights().size(); ++i)
        {
            LightData light;
            light.position = sceneLights[i].getPosition();
            light.range = sceneLights[i].getRange();
            lights.push_back(light);
        }
    }

    // setup material SSBO
    glGenBuffers(1, &bufferMaterials);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, bufferMaterials);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(MaterialData)* materials.size(), &materials[0], GL_STREAM_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, bufferMaterials);
    glShaderStorageBlockBinding(
        firstPassProgram.getProgramID(),
        glGetUniformBlockIndex(firstPassProgram.getProgramID(), "BufferMaterials"),
        1);

    //copy lights data
    //UpdateLightData();

    // set up light SSBO
    glGenBuffers(1, &bufferRender);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, bufferRender);
    unsigned int size = sizeof(glm::mat4) + sizeof(glm::vec3); // since our buffer only has a projection matrix, camposition and single light source
    glBufferData(GL_SHADER_STORAGE_BUFFER, size, NULL, GL_STREAM_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    // bind this buffer to both first pass program and light program since both require it
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, bufferRender);
    glShaderStorageBlockBinding(
        firstPassProgram.getProgramID(),
        glGetUniformBlockIndex(firstPassProgram.getProgramID(), "BufferRender"),
        0);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, bufferRender);
    glShaderStorageBlockBinding(
        lightProgram.getProgramID(),
        glGetUniformBlockIndex(lightProgram.getProgramID(), "BufferRender"),
        0);

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

    // set up light mesh
    {
        tsl::IndexedMesh mesh;
        tsl::CreateSphere(1.f, 12, &mesh);
        tsl::ConvertPolygonsToTriangles(&mesh);

        lightMesh.startVerticeIndex = vertices.size();
        lightMesh.startElementIndex = elements.size();

        for (unsigned int j = 0; j < mesh.vertex_array.size(); ++j)
        {
            vertices.push_back(Vertex(
                ConvVec3(mesh.vertex_array[j]),
                ConvVec3(mesh.normal_array[j])
                ));
        }

        // why is the element array a vector<InstanceId>?
        std::vector<int> elementArray = mesh.index_array;
        for (unsigned int j = 0; j < elementArray.size(); ++j)
        {
            elements.push_back(elementArray[j]);
        }

        lightMesh.endVerticeIndex = vertices.size() - 1;
        lightMesh.endElementIndex = elements.size() - 1;
        lightMesh.verticeCount = lightMesh.endVerticeIndex - lightMesh.startVerticeIndex;
        lightMesh.element_count = lightMesh.endElementIndex - lightMesh.startElementIndex + 1;
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

    // set up light vao since it uses a different channel layout
    {
        glGenBuffers(1, &lightMesh.instanceVBO);
        glBindBuffer(GL_ARRAY_BUFFER, lightMesh.instanceVBO);
        glBufferData(GL_ARRAY_BUFFER,
            lights.size() * sizeof(LightData),
            lights.data(),
            GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        unsigned int offset = 0;

        glGenVertexArrays(1, &lightMesh.vao);
        glBindVertexArray(lightMesh.vao);
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
        glBindBuffer(GL_ARRAY_BUFFER, lightMesh.instanceVBO);

        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE,
            sizeof(LightData), TGL_BUFFER_OFFSET(instanceOffset));
        glVertexAttribDivisor(2, 1);
        instanceOffset += sizeof(glm::vec3);

        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE,
            sizeof(LightData), TGL_BUFFER_OFFSET(instanceOffset));
        glVertexAttribDivisor(3, 1);
        instanceOffset += sizeof(float);

        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glBindVertexArray(0);

    }

    glGenFramebuffers(1, &gbufferID);
    glGenRenderbuffers(1, &depthStencilRBOID);
    glGenTextures(3, gbufferTextureBufferIDS);

    glGenFramebuffers(1, &lbufferID);
    glGenRenderbuffers(1, &lbufferColourRBOID);

}

void MyView::
windowViewDidReset(std::shared_ptr<tygra::Window> window,
int width,
int height)
{
    glViewport(0, 0, width, height);
    aspectRatio = static_cast<float>(width) / height;

    {
        // gbuffer position texture
        glBindTexture(GL_TEXTURE_RECTANGLE, gbufferTextureBufferIDS[0]);
        glTexImage2D(
            GL_TEXTURE_RECTANGLE,
            0,
            GL_RGB32F,
            width,
            height,
            0,
            GL_RGB,
            GL_FLOAT,
            NULL
            );
        glBindTexture(GL_TEXTURE_RECTANGLE, 0);

        // gbuffer normal texture
        glBindTexture(GL_TEXTURE_RECTANGLE, gbufferTextureBufferIDS[1]);
        glTexImage2D(
            GL_TEXTURE_RECTANGLE,
            0,
            GL_RGB32F,
            width,
            height,
            0,
            GL_RGB,
            GL_FLOAT,
            NULL
            );
        glBindTexture(GL_TEXTURE_RECTANGLE, 0);

        // gbuffer material texture
        glBindTexture(GL_TEXTURE_RECTANGLE, gbufferTextureBufferIDS[2]);
        glTexImage2D(
            GL_TEXTURE_RECTANGLE,
            0,
            GL_RGB32F,
            width,
            height,
            0,
            GL_RGB,
            GL_FLOAT,
            NULL
            );
        glBindTexture(GL_TEXTURE_RECTANGLE, 0);

        // gbuffer depth stencil buffer
        glBindRenderbuffer(GL_RENDERBUFFER, depthStencilRBOID);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);

        GLenum gbuffer_status = 0;
        glBindFramebuffer(GL_FRAMEBUFFER, gbufferID);

        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depthStencilRBOID); // attach depth stencil buffer
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE, gbufferTextureBufferIDS[0], 0); // attach position buffer
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_RECTANGLE, gbufferTextureBufferIDS[1], 0); // attach normal buffer
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_RECTANGLE, gbufferTextureBufferIDS[2], 0); // attach material buffer

        gbuffer_status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (gbuffer_status != GL_FRAMEBUFFER_COMPLETE)
        {
            tglDebugMessage(GL_DEBUG_SEVERITY_HIGH, "gbuffer not complete");
        }

        GLenum buffers[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
        glDrawBuffers(3, buffers);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

    }

    {
        // lbuffer colour buffer
        glBindRenderbuffer(GL_RENDERBUFFER, lbufferColourRBOID);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA32F, width, height);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);

        GLenum lbuffer_status = 0;
        glBindFramebuffer(GL_FRAMEBUFFER, lbufferID);

        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, lbufferColourRBOID); // attach colour buffer
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depthStencilRBOID); // attach depth stencil buffer

        lbuffer_status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (lbuffer_status != GL_FRAMEBUFFER_COMPLETE)
        {
            tglDebugMessage(GL_DEBUG_SEVERITY_HIGH, "lbuffer not complete");
        }

        GLenum buffers[] = { GL_COLOR_ATTACHMENT0 };
        glDrawBuffers(1, buffers);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
}

void MyView::
windowViewDidStop(std::shared_ptr<tygra::Window> window)
{

    glDeleteFramebuffers(1, &gbufferID);
    glDeleteRenderbuffers(1, &depthStencilRBOID);
    glDeleteTextures(3, gbufferTextureBufferIDS);

    glDeleteFramebuffers(1, &lbufferID);
    glDeleteRenderbuffers(1, &lbufferColourRBOID);

}

void MyView::
windowViewRender(std::shared_ptr<tygra::Window> window)
{
    assert(scene_ != nullptr);
    GLint viewport_size[4];
    glGetIntegerv(GL_VIEWPORT, viewport_size);

    

    glm::mat4 projectionMatrix = glm::perspective(75.f, aspectRatio, 1.f, 1000.f);
    glm::mat4 viewMatrix = glm::lookAt(scene_->getCamera().getPosition(), scene_->getCamera().getDirection() + scene_->getCamera().getPosition(), glm::vec3(0, 1, 0));
    glm::mat4 projectionViewMatrix = projectionMatrix * viewMatrix;

    SetBuffer(projectionViewMatrix, scene_->getCamera().getPosition());

    // set up the depth and stencil buffers, we are not writing to the onscreen framebuffer, we are filling the relevant data for the light render
    {
        firstPassProgram.useProgram();
        glBindFramebuffer(GL_FRAMEBUFFER, gbufferID);

		glClearColor(0.f, 0.f, 0.25f, 0.f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT); // clear all 3 buffers

        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);

        // not using these so disable them
        glDisable(GL_BLEND);

        glEnable(GL_STENCIL_TEST);
        glStencilFunc(GL_ALWAYS, 127, ~0); // we are writing 1 to all pixels that the geometry draws into
        glStencilOp(GL_ZERO, GL_KEEP, GL_REPLACE);

        // the lights are tagged onto the end of the meshes
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

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

	// global lights
	{

	}

    // lets draw the lights
    {
  //      lightProgram.useProgram();
  //      glBindFramebuffer(GL_FRAMEBUFFER, lbufferID);

		//glClearColor(0.f, 0.f, 0.25f, 0.f);
		//glClear(GL_COLOR_BUFFER_BIT); // clear all 3 buffers

  //      glEnable(GL_BLEND);
  //      glBlendEquation(GL_FUNC_ADD);
  //      glBlendFunc(GL_ONE, GL_ONE);

  //      glEnable(GL_DEPTH_TEST);// enable the depth test for use with lights
  //      glDepthMask(GL_FALSE);// disable depth writes since we dont want the lights to mess with the depth buffer
  //      glDepthFunc(GL_GREATER);// set the depth test to check for in front of the back fragments so that we can light correctly

  //      glEnable(GL_CULL_FACE); // enable the culling (not on by default)
  //      glCullFace(GL_FRONT); // set to cull forward facing fragments

  //      //glDisable(GL_STENCIL_TEST);
  //      glEnable(GL_STENCIL_TEST);
  //      glStencilFunc(GL_EQUAL, 127, ~0);
  //      glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

  //      glActiveTexture(GL_TEXTURE0);
  //      glBindTexture(GL_TEXTURE_RECTANGLE, gbufferTextureBufferIDS[0]);
  //      glUniform1i(glGetUniformLocation(lightProgram.getProgramID(), "sampler_world_position"), 0);

  //      glActiveTexture(GL_TEXTURE1);
  //      glBindTexture(GL_TEXTURE_RECTANGLE, gbufferTextureBufferIDS[1]);
  //      glUniform1i(glGetUniformLocation(lightProgram.getProgramID(), "sampler_world_normal"), 1);

  //      glActiveTexture(GL_TEXTURE2);
  //      glBindTexture(GL_TEXTURE_RECTANGLE, gbufferTextureBufferIDS[3]);
  //      glUniform1i(glGetUniformLocation(lightProgram.getProgramID(), "sampler_world_mat"), 2);

  //      // instance draw the lights woop woop
  //      glBindVertexArray(lightMesh.vao);
  //      glDrawElementsInstancedBaseVertex(GL_TRIANGLES,
  //          lightMesh.element_count,
  //          GL_UNSIGNED_INT,
  //          TGL_BUFFER_OFFSET(lightMesh.startElementIndex * sizeof(int)),
  //          lights.size(),
  //          lightMesh.startVerticeIndex);

  //      glDisable(GL_STENCIL_TEST);
  //      glDepthMask(GL_TRUE);
  //      glDepthFunc(GL_LEQUAL);

  //      glDisable(GL_CULL_FACE);
  //      glCullFace(GL_BACK);
    }
    
    glBindFramebuffer(GL_READ_FRAMEBUFFER, gbufferID);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBlitFramebuffer(0, 0, viewport_size[2], viewport_size[3], 0, 0, viewport_size[2], viewport_size[3], GL_COLOR_BUFFER_BIT, GL_NEAREST);

    glBindFramebuffer(GL_FRAMEBUFFER, 0); // unbind the framebuffers

}

void MyView::SetBuffer(glm::mat4 projectMat_, glm::vec3 camPos_)
{
    // so since glMapBufferRange does not work, i am going to create a temporary buffer for the per model data, and then copy the full buffer straight into the shaders buffer
    unsigned int bufferSize = sizeof(projectMat_)+sizeof(camPos_);
    char* buffer = new char[bufferSize];
    unsigned int index = 0;

    //projection matrix first!
    memcpy(buffer + index, glm::value_ptr(projectMat_), sizeof(glm::mat4));
    index += sizeof(projectMat_);

    // camera position next!
    memcpy(buffer + index, glm::value_ptr(camPos_), sizeof(camPos_));

    // update the render buffer, so get the pointer!
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, bufferRender);
    GLvoid* p = glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_WRITE_ONLY);
    memcpy(p, buffer, bufferSize);
    //done
    glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);

    delete[] buffer;
}

glm::vec3 ConvVec3(tsl::Vector3 &vec_)
{
    return glm::vec3(vec_.x, vec_.y, vec_.z);
}