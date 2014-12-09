#include <vector>
#include <tgl/tgl.h>
#include "Shader.hpp"

Shader::Shader() : loaded(0)
{

}

Shader::~Shader()
{

}

bool Shader::loadShader(std::string file_, int type_)
{
#pragma warning(disable:4996)
    FILE* fp = fopen(file_.c_str(), "rt");
#pragma warning(default:4996)
    if (!fp)
    {
        printf("could not open shader source: %s\n", file_.c_str());
        return false;
    }

    // Get all lines from a file
    std::vector<std::string> lines;
    char line[255];
    while (fgets(line, 255, fp))
    {
        lines.push_back(line);
    }
    fclose(fp);

    const char** program = new const char*[lines.size()];
    for (unsigned int i = 0; i < lines.size(); ++i)
    {
#pragma warning(suppress: 6386)
        program[i] = lines[i].c_str();
    }

    shaderID = glCreateShader(type_);

    glShaderSource(shaderID, lines.size(), program, NULL);
    glCompileShader(shaderID);

    delete[] program;

    int compilationStatus;
    glGetShaderiv(shaderID, GL_COMPILE_STATUS, &compilationStatus);

    if (compilationStatus == GL_FALSE)
    {
        printf("shader compile failed: %i\n", compilationStatus);

        GLint blen = 0;
        GLsizei slen = 0;
        glGetShaderiv(shaderID, GL_INFO_LOG_LENGTH, &blen);
        if (blen > 1)
        {
            GLchar* compiler_log = (GLchar*)malloc(blen);
            glGetShaderInfoLog(shaderID, blen, &slen, compiler_log);
            printf("compiler log: %s \n %s", file_.c_str(), compiler_log);
            free(compiler_log);
        }

        return false;
    }

    type = type_;
    loaded = true;
    printf("shader %s compiled successfully\n", file_.c_str());
    return true;
}

void Shader::deleteShader()
{
    if (!isLoaded())
    {
        return;
    }
    loaded = false;
    glDeleteShader(shaderID);
}

bool Shader::isLoaded()
{
    return loaded;
}

GLuint Shader::getShaderID()
{
    return shaderID;
}