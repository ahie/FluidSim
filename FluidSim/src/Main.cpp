#include <Windows.h>
#include <iostream>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <GLM/glm.hpp>
#include <GLM/gtc/matrix_transform.hpp>
#include <GLM/gtc/random.hpp>

#include "LoadShaders.h"

#ifdef _MSC_VER
	#pragma comment(lib, "glfw3.lib")
	#pragma comment(lib, "glew32.lib")
	#pragma comment(lib, "opengl32.lib")
#endif

// Constants
const int MAX_PARTICLE_COUNT = 131072;
const int MAX_PARTICLE_COUNT_POWER = 17;
const int PARTICLE_COUNT = 131072;
const GLint GRID_CELL_COUNT = 128;
const GLfloat GRID_ORIGIN[] = { -10.0f, -10.0f, -10.0f };
const GLfloat GRID_CELL_SIZE = 0.15625f/2.0f;
const GLfloat PARTICLE_MASS = 1000.f * 8.f * 8.f * 2.f / 131072.f;
const GLfloat SMOOTHING_LENGTH = 0.15625f;
const GLfloat REST_DENSITY = 1000.0f;
const GLfloat GAS_CONSTANT = 400.0f;
const GLfloat VISCOSITY = 402.f;
const GLfloat SURFACE_TENSION_COEFFICIENT = 0.0728f;
const GLfloat SURFACE_TENSION_THRESHOLD = 7.065f;
const GLfloat TIME_STEP = 0.003f;

// Pointer to main window
GLFWwindow* window;

// Shader program IDs
GLuint groupCompute;
GLuint sortCompute;
GLuint densityCompute;
GLuint fluidSimProg;
GLuint renderProg;

// VBO IDs
GLuint position_VBO[2];
GLuint velocity_VBO[2];
GLuint groupID_VBO;
GLuint sorted_groupID_VBO;
GLuint neighbor_VBO;
GLuint offset_buffer;

// TBO IDs
GLuint position_TBO[2];
GLuint velocity_TBO[2];
GLuint groupID_TBO;
GLuint sorted_groupID_TBO;
GLuint offset_texture;

int readonly_index;
int writeonly_index;

void initShaders()
{
	ShaderInfo group[] =
	{
		{ GL_COMPUTE_SHADER, "GroupCompute.glsl" },
		{ GL_NONE, NULL }
	};
	ShaderInfo sort[] =
	{
		{ GL_COMPUTE_SHADER, "SortCompute.glsl" },
		{ GL_NONE, NULL }
	};
	ShaderInfo density[] =
	{
		{ GL_COMPUTE_SHADER, "DensityCompute.glsl" },
		{ GL_NONE, NULL }
	};
	ShaderInfo fluidSim[] =
	{
		{ GL_COMPUTE_SHADER, "FluidSim.glsl" },
		{ GL_NONE, NULL }
	};
	ShaderInfo render[] =
	{
		{ GL_VERTEX_SHADER, "VertexShader.glsl" },
		{ GL_FRAGMENT_SHADER, "FragmentShader.glsl" },
		{ GL_NONE, NULL }
	};
	groupCompute = LoadShaders(group);
	sortCompute = LoadShaders(sort);
	densityCompute = LoadShaders(density);
	fluidSimProg = LoadShaders(fluidSim);
	renderProg = LoadShaders(render);
}

void initBuffers()
{
	// GENERATE VERTEX BUFFERS
	glGenBuffers(2, position_VBO);
	glGenBuffers(2, velocity_VBO);
	glGenBuffers(1, &groupID_VBO);
	glGenBuffers(1, &sorted_groupID_VBO);
	glGenBuffers(1, &offset_buffer);

	// GENERATE TEXTURE BUFFERS
	glGenTextures(2, position_TBO);
	glGenTextures(2, velocity_TBO);
	glGenTextures(1, &groupID_TBO);
	glGenTextures(1, &sorted_groupID_TBO);
	glGenTextures(1, &offset_texture);

	glm::vec4* initial_particle_positions = new glm::vec4[PARTICLE_COUNT];
	int pcount = pow(PARTICLE_COUNT / 4, 1.f / 3.f);
	int index = 0;
	for (size_t i = 0; i < pcount; i++)
	{
		for (size_t j = 0; j < 2 * pcount; j++)
		{
			for (size_t k = 0; k < 2 * pcount; k++)
			{
				*(initial_particle_positions + index) = glm::vec4(
					glm::vec3(k * 8.f / (2.f * (float)pcount),
					j * 8.f / (2.f * (float)pcount),
					i * 2.f / (float)pcount)
					- glm::vec3(8.499, 8.499, 8.499), 0.0);
				index++;
			}
		}
	}

	// position buffers
	glBindBuffer(GL_ARRAY_BUFFER, position_VBO[0]);
	glBufferData(GL_ARRAY_BUFFER, PARTICLE_COUNT * sizeof(glm::vec4), initial_particle_positions, GL_DYNAMIC_COPY);
	glBindTexture(GL_TEXTURE_BUFFER, position_TBO[0]);
	glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, position_VBO[0]);
	glBindBuffer(GL_ARRAY_BUFFER, position_VBO[1]);
	glBufferData(GL_ARRAY_BUFFER, PARTICLE_COUNT * sizeof(glm::vec4), initial_particle_positions, GL_DYNAMIC_COPY);
	glBindTexture(GL_TEXTURE_BUFFER, position_TBO[1]);
	glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, position_VBO[1]);
	delete[] initial_particle_positions;
	
	// offset buffer and texture
	glBindBuffer(GL_ARRAY_BUFFER, offset_buffer);
	glBufferData(GL_ARRAY_BUFFER, 27 * PARTICLE_COUNT * sizeof(GLint) * 2, NULL, GL_DYNAMIC_COPY);
	glBindTexture(GL_TEXTURE_BUFFER, offset_texture);
	glTexBuffer(GL_TEXTURE_BUFFER, GL_RG32I, offset_texture);

	// groupID VBO & TBO
	glBindBuffer(GL_ARRAY_BUFFER, groupID_VBO);
	glBufferData(GL_ARRAY_BUFFER, PARTICLE_COUNT * sizeof(GLint)* 2, NULL, GL_DYNAMIC_COPY);
	glBindTexture(GL_TEXTURE_BUFFER, groupID_TBO);
	glTexBuffer(GL_TEXTURE_BUFFER, GL_RG32I, groupID_VBO);

	// sorted_groupID VBO & TBO
	glBindBuffer(GL_ARRAY_BUFFER, sorted_groupID_VBO);
	glBufferData(GL_ARRAY_BUFFER, MAX_PARTICLE_COUNT * sizeof(GLint)* 2, NULL, GL_DYNAMIC_COPY);
	glBindTexture(GL_TEXTURE_BUFFER, sorted_groupID_TBO);
	glTexBuffer(GL_TEXTURE_BUFFER, GL_RG32I, sorted_groupID_VBO);

	// velocity buffers
	glBindBuffer(GL_ARRAY_BUFFER, velocity_VBO[0]);
	glBufferData(GL_ARRAY_BUFFER, PARTICLE_COUNT * sizeof(glm::vec4), NULL, GL_DYNAMIC_COPY);
	glm::vec4 * vels1 = (glm::vec4 *)
		glMapBufferRange(GL_ARRAY_BUFFER, 0, PARTICLE_COUNT * sizeof(glm::vec4),
		GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
	for (int i = 0; i < PARTICLE_COUNT; i++)
		vels1[i] = glm::vec4(0.0f);
	glUnmapBuffer(GL_ARRAY_BUFFER);
	glBindTexture(GL_TEXTURE_BUFFER, velocity_TBO[0]);
	glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, velocity_VBO[0]);
	glBindBuffer(GL_ARRAY_BUFFER, velocity_VBO[1]);
	glBufferData(GL_ARRAY_BUFFER, PARTICLE_COUNT * sizeof(glm::vec4), NULL, GL_DYNAMIC_COPY);
	glm::vec4 * vels2 = (glm::vec4 *)
		glMapBufferRange(GL_ARRAY_BUFFER, 0, PARTICLE_COUNT * sizeof(glm::vec4),
		GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
	for (int i = 0; i < PARTICLE_COUNT; i++)
		vels2[i] = glm::vec4(0.0f);
	glUnmapBuffer(GL_ARRAY_BUFFER);
	glBindTexture(GL_TEXTURE_BUFFER, velocity_TBO[1]);
	glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, velocity_VBO[1]);

	readonly_index = 1;
	writeonly_index = 0;
}

void demoInit()
{
	// CONTEXT SETTINGS
	glClearColor(.2f, .2f, .2f, 1.f);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// SHADER INIT
	initShaders();

	// BUFFER INIT
	initBuffers();
}

void renderLoop()
{
	int temp = writeonly_index;
	writeonly_index = readonly_index;
	readonly_index = temp;

	// Update groupID_TBO
	glUseProgram(groupCompute);
	glUniform3fv(0, 1, GRID_ORIGIN); 
	glUniform1i(1, GRID_CELL_COUNT);
	glUniform1f(2, GRID_CELL_SIZE);
	glBindImageTexture(0, groupID_TBO, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RG32I);
	glBindImageTexture(1, sorted_groupID_TBO, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RG32I);
	glBindImageTexture(2, position_TBO[readonly_index], 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);
	glDispatchCompute(PARTICLE_COUNT / 256, 1, 1);
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

	// Sort sorted_groupID_TBO
	glUseProgram(sortCompute);
	glBindImageTexture(0, sorted_groupID_TBO, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RG32I);
	for (int i = 1; i <= MAX_PARTICLE_COUNT_POWER; i++)
	{
		glUniform1i(0, i);
		glUniform1i(1, 0);
		glDispatchCompute(MAX_PARTICLE_COUNT / 256, 1, 1);
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
		for (int j = i - 1; j > 0; j--)
		{
			glUniform1i(0, i);
			glUniform1i(1, j);
			glDispatchCompute(MAX_PARTICLE_COUNT / 256, 1, 1);
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
		}
	}

	// Compute density field
	glUseProgram(densityCompute);
	glBindImageTexture(0, groupID_TBO, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RG32I);
	glBindImageTexture(1, sorted_groupID_TBO, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RG32I);
	glBindImageTexture(2, offset_texture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RG32I);
	glBindImageTexture(3, position_TBO[readonly_index], 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
	glUniform1ui(0, GRID_CELL_COUNT);
	glUniform1f(1, SMOOTHING_LENGTH);
	glUniform1f(2, PARTICLE_MASS);
	glUniform1i(3, 0);
	glUniform1i(10, MAX_PARTICLE_COUNT);
	glUniform1i(11, MAX_PARTICLE_COUNT_POWER);
	glDispatchCompute(PARTICLE_COUNT / 128, 1, 1);
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

	// Do fluid simulation
	glUseProgram(fluidSimProg);
	glBindImageTexture(2, offset_texture, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RG32I);
	glBindImageTexture(4, position_TBO[writeonly_index], 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
	glBindImageTexture(5, velocity_TBO[readonly_index], 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
	glBindImageTexture(6, velocity_TBO[writeonly_index], 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
	glUniform1ui(0, GRID_CELL_COUNT);
	glUniform1f(1, SMOOTHING_LENGTH);
	glUniform1f(2, PARTICLE_MASS);
	glUniform1f(3, REST_DENSITY);
	glUniform1f(4, GAS_CONSTANT);
	glUniform1f(5, VISCOSITY);
	glUniform1f(6, TIME_STEP);
	glUniform1i(7, 0);
	glUniform1f(8, SURFACE_TENSION_COEFFICIENT);
	glUniform1f(9, SURFACE_TENSION_THRESHOLD);
	glUniform1i(10, MAX_PARTICLE_COUNT);
	glUniform1i(11, MAX_PARTICLE_COUNT_POWER);
	glDispatchCompute(PARTICLE_COUNT / 128, 1, 1);
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

	// Rendering
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glUseProgram(renderProg);
	glUniform1f(1, REST_DENSITY);
	glBindBuffer(GL_ARRAY_BUFFER, position_VBO[writeonly_index]);
	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(4, GL_FLOAT, 0, 0);

	glViewport(0, 0, 1280 / 2, 720);
	glm::mat4 mvp = glm::perspective(45.0f, 0.888888888f, 0.1f, 1000.0f) *
		glm::translate(glm::mat4(1.0f), glm::vec3(6.5f, 5.5f, -18.0f)) *
		glm::rotate(glm::mat4(1.0f), 15.0f, glm::vec3(1.0f, 0.0f, 0.0f)) *
		glm::rotate(glm::mat4(1.0f), 50.0f, glm::vec3(0.0f, 1.0f, 0.0f));
	glUniformMatrix4fv(0, 1, GL_FALSE, &(mvp)[0][0]);
	glDrawArrays(GL_POINTS, 0, PARTICLE_COUNT);

	glViewport(1280 / 2, 0, 1280 / 2, 720);
	mvp = glm::ortho(-8.5f, -0.5f, -8.5f, -0.5f, 0.f, 10.f) *
			glm::lookAt(glm::vec3(-8.5f,0,0),glm::vec3(0,0,0),glm::vec3(0,1,0));
	glUniformMatrix4fv(0, 1, GL_FALSE, &(mvp)[0][0]);
	glDrawArrays(GL_POINTS, 0, PARTICLE_COUNT);

	glDisableClientState(GL_VERTEX_ARRAY);
	glfwSwapBuffers(window);
}

int main()
{
	if (!glfwInit())
		return -1;
	window = glfwCreateWindow(1280, 720, "FluidSim", NULL, NULL);
	if (!window)
	{
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);
	if (glewInit() != GLEW_OK)
		return -1;
	demoInit();

	float t;
	float deltaTime;
	t = glfwGetTime();

	while (!glfwWindowShouldClose(window))
	{
		deltaTime = glfwGetTime() - t;
		t = glfwGetTime();

		std::cout << deltaTime << std::endl;

		renderLoop();
		glfwPollEvents();
	}
	glfwTerminate();
	return 0;
}