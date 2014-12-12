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

		// set the channels of the output for this one to be sure
		glBindFragDataLocation(firstPassProgram.getProgramID(), 0, "position");
		glBindFragDataLocation(firstPassProgram.getProgramID(), 1, "normal");
		glBindFragDataLocation(firstPassProgram.getProgramID(), 2, "material");

        firstPassProgram.linkProgram();

        firstPassProgram.useProgram();
    }

	{
		Shader vs, fs;
		vs.loadShader("background_vs.glsl", GL_VERTEX_SHADER);
		fs.loadShader("background_fs.glsl", GL_FRAGMENT_SHADER);

		backgroundProgram.createProgram();
		backgroundProgram.addShaderToProgram(&vs);
		backgroundProgram.addShaderToProgram(&fs);

		backgroundProgram.linkProgram();

		backgroundProgram.useProgram();
	}

    {
        Shader vs, fs;
        vs.loadShader("global_light_vs.glsl", GL_VERTEX_SHADER);
        fs.loadShader("global_light_fs.glsl", GL_FRAGMENT_SHADER);

        globalLightProgram.createProgram();
        globalLightProgram.addShaderToProgram(&vs);
        globalLightProgram.addShaderToProgram(&fs);

        globalLightProgram.linkProgram();

        globalLightProgram.useProgram();
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
	
	preparation for future

	*/
	{
		Shader vs, fs;
		vs.loadShader("postprocess_vs.glsl", GL_VERTEX_SHADER);
		fs.loadShader("postprocess_fs.glsl", GL_FRAGMENT_SHADER);

		postProcessProgram.createProgram();
		postProcessProgram.addShaderToProgram(&vs);
		postProcessProgram.addShaderToProgram(&fs);

		postProcessProgram.linkProgram();

		postProcessProgram.useProgram();
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

	// set up fullscreen quad
    {
        std::vector<glm::vec2> vertices(4);
        vertices[0] = glm::vec2(-1, -1);
        vertices[1] = glm::vec2(1, -1);
        vertices[2] = glm::vec2(1, 1);
        vertices[3] = glm::vec2(-1, 1);

        glGenBuffers(1, &globalLightMesh.instanceVBO);
        glBindBuffer(GL_ARRAY_BUFFER, globalLightMesh.instanceVBO);
        glBufferData(GL_ARRAY_BUFFER,
            vertices.size() * sizeof(glm::vec2),
            vertices.data(),
            GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glGenVertexArrays(1, &globalLightMesh.vao);
        glBindVertexArray(globalLightMesh.vao);
        glBindBuffer(GL_ARRAY_BUFFER, globalLightMesh.instanceVBO);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE,
            sizeof(glm::vec2), 0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
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

    glGenFramebuffers(1, &gbufferFBO);
    glGenRenderbuffers(1, &depthStencilRBO);
    glGenTextures(3, gbufferTO);

    glGenFramebuffers(1, &lbufferFBO);
	glGenTextures(1, &lbufferTO);

	glGenFramebuffers(1, &postProcessFBO);
	glGenRenderbuffers(1, &postProcessColourRBO);

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
        glBindTexture(GL_TEXTURE_RECTANGLE, gbufferTO[0]);
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
        glBindTexture(GL_TEXTURE_RECTANGLE, gbufferTO[1]);
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
        glBindTexture(GL_TEXTURE_RECTANGLE, gbufferTO[2]);
        glTexImage2D(
            GL_TEXTURE_RECTANGLE,
            0,
            GL_RGBA32F,
            width,
            height,
            0,
            GL_RGBA,
            GL_FLOAT,
            NULL
            );
        glBindTexture(GL_TEXTURE_RECTANGLE, 0);

        // gbuffer depth stencil buffer
        glBindRenderbuffer(GL_RENDERBUFFER, depthStencilRBO);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);

        GLenum gbuffer_status = 0;
        glBindFramebuffer(GL_FRAMEBUFFER, gbufferFBO);

        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depthStencilRBO); // attach depth stencil buffer
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE, gbufferTO[0], 0); // attach position buffer
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_RECTANGLE, gbufferTO[1], 0); // attach normal buffer
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_RECTANGLE, gbufferTO[2], 0); // attach material buffer

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
		//TODO: need to change to normal texture2D?
		// So that we can do a post process effect, we draw into a texture again
		glBindTexture(GL_TEXTURE_RECTANGLE, lbufferTO);
		glTexImage2D(
			GL_TEXTURE_RECTANGLE,
			0,
			GL_RGBA32F,
			width,
			height,
			0,
			GL_RGBA,
			GL_FLOAT,
			NULL
			);
		glBindTexture(GL_TEXTURE_RECTANGLE, 0);

        GLenum lbuffer_status = 0;
        glBindFramebuffer(GL_FRAMEBUFFER, lbufferFBO);

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE, lbufferTO, 0); // attach position buffer
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depthStencilRBO); // attach depth stencil buffer

        lbuffer_status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (lbuffer_status != GL_FRAMEBUFFER_COMPLETE)
        {
            tglDebugMessage(GL_DEBUG_SEVERITY_HIGH, "lbuffer not complete");
        }

        GLenum buffers[] = { GL_COLOR_ATTACHMENT0 };
        glDrawBuffers(1, buffers);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

	{
		// pbuffer colour buffer
		glBindRenderbuffer(GL_RENDERBUFFER, postProcessColourRBO);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_RGB32F, width, height);
		glBindRenderbuffer(GL_RENDERBUFFER, 0);

		GLenum pbuffer_status = 0;
		glBindFramebuffer(GL_FRAMEBUFFER, postProcessFBO);

		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, postProcessColourRBO); // attach colour buffer

		pbuffer_status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (pbuffer_status != GL_FRAMEBUFFER_COMPLETE)
		{
			tglDebugMessage(GL_DEBUG_SEVERITY_HIGH, "pbuffer not complete");
		}

		GLenum buffers[] = { GL_COLOR_ATTACHMENT0 };
		glDrawBuffers(1, buffers);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}
}

void MyView::
windowViewDidStop(std::shared_ptr<tygra::Window> window)
{

    glDeleteFramebuffers(1, &gbufferFBO);
    glDeleteRenderbuffers(1, &depthStencilRBO);
    glDeleteTextures(3, gbufferTO);

    glDeleteFramebuffers(1, &lbufferFBO);
	glDeleteTextures(1, &lbufferTO);

	glDeleteFramebuffers(1, &postProcessFBO);
	glDeleteRenderbuffers(1, &postProcessColourRBO);

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
        glBindFramebuffer(GL_FRAMEBUFFER, gbufferFBO);

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

	// shade background as scool of computing purple
	{
		backgroundProgram.useProgram();
		glBindFramebuffer(GL_FRAMEBUFFER, lbufferFBO);

		glClearColor(0.f, 0.f, 0.25f, 0.f);
		glClear(GL_COLOR_BUFFER_BIT); // clear all 3 buffers

		glDisable(GL_DEPTH_TEST); // disable depth test snce we are drawing a full screen quad
		glDisable(GL_BLEND);

		glEnable(GL_STENCIL_TEST);
		glStencilFunc(GL_EQUAL, 0, ~0); // equal to background
		glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

		// draw directional light
		glBindVertexArray(globalLightMesh.vao);
		glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	}

	// global lights
	{
        globalLightProgram.useProgram();
        glBindFramebuffer(GL_FRAMEBUFFER, lbufferFBO);

        glDisable(GL_DEPTH_TEST); // disable depth test snce we are drawing a full screen quad
        glDisable(GL_BLEND);

        glEnable(GL_STENCIL_TEST);
        glStencilFunc(GL_NOTEQUAL, 0, ~0);
        glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

		// could remove the glGetUniformLocation, but again, being lazy and fps is still around 100 - 105
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_RECTANGLE, gbufferTO[0]);
        glUniform1i(glGetUniformLocation(globalLightProgram.getProgramID(), "sampler_world_position"), 0);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_RECTANGLE, gbufferTO[1]);
        glUniform1i(glGetUniformLocation(globalLightProgram.getProgramID(), "sampler_world_normal"), 1);

        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_RECTANGLE, gbufferTO[2]);
        glUniform1i(glGetUniformLocation(globalLightProgram.getProgramID(), "sampler_world_mat"), 2);

		// since there are only 2 vecs to pass, im being lazy and doing it this way
        glUniform3fv(glGetUniformLocation(globalLightProgram.getProgramID(), "directional_light"), 1, glm::value_ptr(scene_->getGlobalLightDirection()));
        glUniform3fv(glGetUniformLocation(globalLightProgram.getProgramID(), "light_intensity"), 1, glm::value_ptr(scene_->getGlobalLightIntensity()));

        // draw directional light
        glBindVertexArray(globalLightMesh.vao);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	}

    // lets draw the lights
    {
        lightProgram.useProgram();
        
		// additive blending
		glEnable(GL_BLEND);
        glBlendEquation(GL_FUNC_ADD);
        glBlendFunc(GL_ONE, GL_ONE);

        glEnable(GL_DEPTH_TEST);// enable the depth test for use with lights
        glDepthMask(GL_FALSE);// disable depth writes since we dont want the lights to mess with the depth buffer
        glDepthFunc(GL_GREATER);// set the depth test to check for in front of the back fragments so that we can light correctly

        glEnable(GL_CULL_FACE); // enable the culling (not on by default)
        glCullFace(GL_FRONT); // set to cull forward facing fragments

        glEnable(GL_STENCIL_TEST);
        glStencilFunc(GL_NOTEQUAL, 0, ~0); // background is set to 0, we want the geometry pixels
        glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_RECTANGLE, gbufferTO[0]);
        glUniform1i(glGetUniformLocation(lightProgram.getProgramID(), "sampler_world_position"), 0);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_RECTANGLE, gbufferTO[1]);
        glUniform1i(glGetUniformLocation(lightProgram.getProgramID(), "sampler_world_normal"), 1);

        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_RECTANGLE, gbufferTO[2]);
        glUniform1i(glGetUniformLocation(lightProgram.getProgramID(), "sampler_world_mat"), 2);

		UpdateLights();

        // instance draw the lights woop woop
        glBindVertexArray(lightMesh.vao);
        glDrawElementsInstancedBaseVertex(GL_TRIANGLES,
            lightMesh.element_count,
            GL_UNSIGNED_INT,
            TGL_BUFFER_OFFSET(lightMesh.startElementIndex * sizeof(int)),
            lights.size(),
            lightMesh.startVerticeIndex);

        glDisable(GL_STENCIL_TEST);
        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LEQUAL);

        glDisable(GL_CULL_FACE);
        glCullFace(GL_BACK);
    }

	// post process shenanigans
	{
		postProcessProgram.useProgram();
		glBindFramebuffer(GL_FRAMEBUFFER, postProcessFBO);

		glClearColor(0.f, 0.f, 0.25f, 0.f);
		glClear(GL_COLOR_BUFFER_BIT); // clear all 3 buffers

		glDisable(GL_BLEND); // disable blending

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_RECTANGLE, lbufferTO);
		glUniform1i(glGetUniformLocation(postProcessProgram.getProgramID(), "sampler_world_position"), 0);

		glBindVertexArray(globalLightMesh.vao);
		glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	}
    
    glBindFramebuffer(GL_READ_FRAMEBUFFER, postProcessFBO);
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

void MyView::UpdateLights()
{
	std::vector<SceneModel::Light> sceneLights = scene_->getAllLights();
	lights.resize(sceneLights.size());
	for (unsigned int i = 0; i < scene_->getAllLights().size(); ++i)
	{
		LightData light;
		light.position = sceneLights[i].getPosition();
		light.range = sceneLights[i].getRange();
		lights[i] = light;
	}

	glBindBuffer(GL_ARRAY_BUFFER, lightMesh.instanceVBO);
	glBufferData(GL_ARRAY_BUFFER,
		lights.size() * sizeof(LightData),
		lights.data(),
		GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

// method fixes damn inconsistencies of this so called 'legacy code'
glm::vec3 ConvVec3(tsl::Vector3 &vec_)
{
    return glm::vec3(vec_.x, vec_.y, vec_.z);
}