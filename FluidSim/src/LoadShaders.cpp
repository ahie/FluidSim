#include <iostream>
#include <fstream>
#include <vector>
#include <GL/glew.h>

#include "LoadShaders.h"

static const GLchar* ReadShader(const char* filename)
{
	std::streampos len;
	GLchar* source = nullptr;

	std::ifstream file(filename, std::ios::in | std::ios::binary | std::ios::ate);
	if (file.is_open()) 
	{
		len = file.tellg();
		source = new GLchar[(unsigned int)len + 1];
		source[len] = 0;

		file.seekg(0, std::ios::beg);
		file.read(source, len);
		file.close();
	}

	return const_cast<const GLchar*>(source);
}

GLuint LoadShaders(std::initializer_list<ShaderInfo> shaderInfos)
{
	GLuint program = glCreateProgram();

	for (auto shaderInfo : shaderInfos) 
	{
		GLuint shader = glCreateShader(shaderInfo.type);
		const GLchar* source = ReadShader(shaderInfo.filename);

		glShaderSource(shader, 1, &source, NULL);
		glCompileShader(shader);

		delete[] source;

		GLint isCompiled = 0;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);
		if (isCompiled == GL_FALSE)
		{
			GLint maxLength = 0;
			glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);

			std::vector<GLchar> errorLog(maxLength);
			glGetShaderInfoLog(shader, maxLength, &maxLength, &errorLog[0]);

			glDeleteShader(shader);
			
			for (auto i : errorLog)
			{
				std::cerr << i;
			}
		}

		glAttachShader(program, shader);

	}

	glLinkProgram(program);
	return program;
}
