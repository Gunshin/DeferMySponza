#pragma once
#ifndef SHADER_PROGRAM_HPP
#define SHADER_PROGRAM_HPP

#include <stdlib.h>
#include <string>
#include <vector>

#include "Shader.hpp"


class ShaderProgram
{

protected:

    GLuint programID;
    bool linked;

public:

    ShaderProgram();
    ~ShaderProgram();

    void createProgram();
    void deleteProgram();

    bool addShaderToProgram(Shader* shader_);
    bool linkProgram();

    void useProgram();

    GLuint getProgramID();

};


#endif //SHADER_PROGRAM_HPP