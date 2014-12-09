#pragma once
#ifndef SHADER_HPP
#define SHADER_HPP

#include <stdlib.h>
#include <string>

class Shader
{
protected:

    GLuint shaderID;
    int type;
    bool loaded;

public:

    Shader();
    ~Shader();

    bool loadShader(std::string file_, int type_);
    void deleteShader();

    bool isLoaded();
    GLuint getShaderID();

};


#endif //SHADER_HPP