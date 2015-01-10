#pragma once

#include <GL/gl.h>
#include <initializer_list>

struct ShaderInfo {
    GLenum       type;
    const char*  filename;
};

GLuint LoadShaders(std::initializer_list<ShaderInfo>);
