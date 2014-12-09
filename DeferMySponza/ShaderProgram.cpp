#include <stdlib.h>
#include <tgl/tgl.h>
#include "ShaderProgram.hpp"

ShaderProgram::ShaderProgram() : linked(false)
{

}

ShaderProgram::~ShaderProgram()
{

}

void ShaderProgram::createProgram()
{
    programID = glCreateProgram();
}

bool ShaderProgram::addShaderToProgram(Shader* shader_)
{
    if (!shader_->isLoaded())
    {
        printf("shader not loaded\n");
        return false;
    }

    glAttachShader(programID, shader_->getShaderID());

    return true;
}

bool ShaderProgram::linkProgram()
{
    glLinkProgram(programID);
    int linkStatus;
    glGetProgramiv(programID, GL_LINK_STATUS, &linkStatus);

    GLint blen = 0;
    GLsizei slen = 0;

    glGetProgramiv(programID, GL_INFO_LOG_LENGTH, &blen);
    if (blen > 1)
    {
        GLchar* compiler_log = (GLchar*)malloc(blen);
        glGetProgramInfoLog(programID, blen, &slen, compiler_log);
        printf("compiler log:\n%s", compiler_log);
        free(compiler_log);
    }

    linked = linkStatus == GL_TRUE;
    return linked;
}

void ShaderProgram::deleteProgram()
{
    if (!linked)
    {
        return;
    }
    linked = false;
    glDeleteProgram(programID);
}

void ShaderProgram::useProgram()
{
    if (linked)
    {
        glUseProgram(programID);
    }
}

GLuint ShaderProgram::getProgramID()
{
    return programID;
}
