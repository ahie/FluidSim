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

double grouptime = 0.;
double sorttime = 0.;
double denstime = 0.;
double offsettime = 0.;
double simtime = 0.;
double updatetime = 0.;
double rendertime = 0.;
double swaptime = 0.;

// Constants
const int MAX_PARTICLE_COUNT = 262144;
const int MAX_PARTICLE_COUNT_POWER = 18;
const int PARTICLE_COUNT = 262144;
const GLint GRID_CELL_COUNT = 128;
const GLfloat GRID_ORIGIN[] = { 0.f, 0.f, 0.f };
const GLfloat GRID_CELL_SIZE = 0.3125f;
const GLfloat PARTICLE_MASS = 1000.f * 10.f * 10.f * 2.5f / 262144.f;
const GLfloat SMOOTHING_LENGTH = 0.3125f;
const GLfloat REST_DENSITY = 1000.0f;
const GLfloat GAS_CONSTANT = 800.0f;
const GLfloat VISCOSITY = 1000.5f;
const GLfloat SURFACE_TENSION_COEFFICIENT = 0.0728f;
const GLfloat SURFACE_TENSION_THRESHOLD = 7.065f;
const GLfloat TIME_STEP = 0.005f;

// Pointer to main window
static GLFWwindow* window;

// Shader program IDs
static GLuint groupCompute;
static GLuint sortCompute;
static GLuint offsetCompute;
static GLuint densityCompute;
static GLuint fluidSimProg;
static GLuint updateParticlesProg;
static GLuint renderProg;

// VBO IDs
static GLuint position_VBO;
static GLuint accel_VBO;
static GLuint velocity_VBO[2];
static GLuint groupID_VBO;
static GLuint sorted_groupID_VBO;
static GLuint offset_buffer;

// TBO IDs
static GLuint position_TBO;
static GLuint accel_TBO;
static GLuint velocity_TBO[2];
static GLuint groupID_TBO;
static GLuint sorted_groupID_TBO;
static GLuint offset_texture;

void initShaders()
{
	groupCompute = LoadShaders({ { GL_COMPUTE_SHADER, "GroupCompute.glsl" } });
	sortCompute = LoadShaders({ { GL_COMPUTE_SHADER, "SortCompute.glsl" } });
	offsetCompute = LoadShaders({ { GL_COMPUTE_SHADER, "OffsetCompute.glsl" } });
	densityCompute = LoadShaders({ { GL_COMPUTE_SHADER, "DensityCompute.glsl" } });
	fluidSimProg = LoadShaders({ { GL_COMPUTE_SHADER, "FluidSim.glsl" } });
	updateParticlesProg = LoadShaders({ { GL_COMPUTE_SHADER, "UpdateParticles.glsl" } });
	renderProg = LoadShaders({ { GL_VERTEX_SHADER, "VertexShader.glsl" }, { GL_FRAGMENT_SHADER, "FragmentShader.glsl" } });
}

void initBuffers()
{
	// GENERATE VERTEX BUFFERS
	glGenBuffers(1, &position_VBO);
	glGenBuffers(1, &accel_VBO);
	glGenBuffers(2, velocity_VBO);
	glGenBuffers(1, &groupID_VBO);
	glGenBuffers(1, &sorted_groupID_VBO);
	glGenBuffers(1, &offset_buffer);

	// GENERATE TEXTURE BUFFERS
	glGenTextures(1, &position_TBO);
	glGenTextures(1, &accel_TBO);
	glGenTextures(2, velocity_TBO);
	glGenTextures(1, &groupID_TBO);
	glGenTextures(1, &sorted_groupID_TBO);
	glGenTextures(1, &offset_texture);

	glm::vec4* initial_particle_positions = new glm::vec4[PARTICLE_COUNT];
	int pcount = pow(PARTICLE_COUNT / 4, 1.f / 3.f);
	int index = 0;
	for (size_t i = 1; i < pcount; i++)
	{
		for (size_t j = 1; j < 2 * pcount; j++)
		{
			for (size_t k = 1; k < 2 * pcount; k++)
			{
				*(initial_particle_positions + index) = glm::vec4(
					glm::vec3(k * 10.f / (2.f * (float)pcount),
					j * 10.f / (2.f * (float)pcount),
					i * 2.5f / (float)pcount), 0.0);
				index++;
			}
		}
	}

	// position buffers
	glBindBuffer(GL_ARRAY_BUFFER, position_VBO);
	glBufferData(GL_ARRAY_BUFFER, PARTICLE_COUNT * sizeof(glm::vec4), initial_particle_positions, GL_DYNAMIC_COPY);
	glBindTexture(GL_TEXTURE_BUFFER, position_TBO);
	glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, position_VBO);
	glBindBuffer(GL_ARRAY_BUFFER, accel_VBO);
	glBufferData(GL_ARRAY_BUFFER, PARTICLE_COUNT * sizeof(glm::vec4), initial_particle_positions, GL_DYNAMIC_COPY);
	glBindTexture(GL_TEXTURE_BUFFER, accel_TBO);
	glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, accel_VBO);
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
		vels1[i] = glm::vec4(0.0f,0.f,0.f,-1.f);
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
}

void bindImageTextures()
{
	glBindImageTexture(0, groupID_TBO, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RG32I);
	glBindImageTexture(1, sorted_groupID_TBO, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RG32I);
	glBindImageTexture(2, offset_texture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RG32I);
	glBindImageTexture(3, position_TBO, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
	glBindImageTexture(4, accel_TBO, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
	glBindImageTexture(5, velocity_TBO[0], 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
	glBindImageTexture(6, velocity_TBO[1], 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
}

void demoInit()
{
	// CONTEXT SETTINGS
	glClearColor(0.4f, 0.4f, 0.4f, 1.f);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	initShaders();
	initBuffers();
	bindImageTextures();
}

void renderLoop()
{
	LARGE_INTEGER timer_start, timer_end, frequency;
	QueryPerformanceFrequency(&frequency);

	// Update groupID_TBO
	QueryPerformanceCounter(&timer_start);
	glUseProgram(groupCompute);
	glUniform3fv(0, 1, GRID_ORIGIN); 
	glUniform1i(1, GRID_CELL_COUNT);
	glUniform1f(2, GRID_CELL_SIZE);
	glDispatchCompute(PARTICLE_COUNT / 256, 1, 1);
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
	glFinish();
	QueryPerformanceCounter(&timer_end);
	grouptime += double(timer_end.QuadPart - timer_start.QuadPart) / double(frequency.QuadPart);

	// Sort sorted_groupID_TBO
	QueryPerformanceCounter(&timer_start);
	glUseProgram(sortCompute);
	for (int i = 1; i <= MAX_PARTICLE_COUNT_POWER; i++)
	{
		glUniform1i(0, i);
		glUniform1i(1, 0);
		glDispatchCompute(MAX_PARTICLE_COUNT / 512, 1, 1);
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
		for (int j = i - 1; j > 0; j--)
		{
			glUniform1i(0, i);
			glUniform1i(1, j);
			glDispatchCompute(MAX_PARTICLE_COUNT / 512, 1, 1);
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
		}
	}
	glFinish();
	QueryPerformanceCounter(&timer_end);
	sorttime += double(timer_end.QuadPart - timer_start.QuadPart) / double(frequency.QuadPart);

	// Compute offsets
	QueryPerformanceCounter(&timer_start);
	glUseProgram(offsetCompute);
	glUniform1i(0, MAX_PARTICLE_COUNT);
	glUniform1i(1, MAX_PARTICLE_COUNT_POWER);
	glUniform1ui(2, GRID_CELL_COUNT);
	glDispatchCompute(PARTICLE_COUNT / 128, 1, 1);
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
	glFinish();
	QueryPerformanceCounter(&timer_end);
	offsettime += double(timer_end.QuadPart - timer_start.QuadPart) / double(frequency.QuadPart);

	// Compute density field
	QueryPerformanceCounter(&timer_start);
	glUseProgram(densityCompute);
	glUniform1ui(0, GRID_CELL_COUNT);
	glUniform1f(1, SMOOTHING_LENGTH);
	glUniform1f(2, PARTICLE_MASS);
	glUniform1i(3, MAX_PARTICLE_COUNT);
	glUniform1i(4, MAX_PARTICLE_COUNT_POWER);
	glUniform1i(5, 0);
	glDispatchCompute(PARTICLE_COUNT / 128, 1, 1);
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
	glFinish();
	QueryPerformanceCounter(&timer_end);
	denstime += double(timer_end.QuadPart - timer_start.QuadPart) / double(frequency.QuadPart);

	// Calculate acceleration
	QueryPerformanceCounter(&timer_start);
	glUseProgram(fluidSimProg);
	glUniform1ui(0, GRID_CELL_COUNT);
	glUniform1f(1, SMOOTHING_LENGTH);
	glUniform1f(2, PARTICLE_MASS);
	glUniform1f(3, REST_DENSITY);
	glUniform1f(4, GAS_CONSTANT);
	glUniform1f(5, VISCOSITY);
	glUniform1f(7, SURFACE_TENSION_COEFFICIENT);
	glUniform1f(8, SURFACE_TENSION_THRESHOLD);
	glDispatchCompute(PARTICLE_COUNT / 128, 1, 1);
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
	glFinish();
	QueryPerformanceCounter(&timer_end);
	simtime += double(timer_end.QuadPart - timer_start.QuadPart) / double(frequency.QuadPart);

	// Calculate new positions and velocities
	QueryPerformanceCounter(&timer_start);
	glUseProgram(updateParticlesProg);
	glUniform1f(6, TIME_STEP);
	glDispatchCompute(PARTICLE_COUNT / 256, 1, 1);
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
	glFinish();
	QueryPerformanceCounter(&timer_end);
	updatetime += double(timer_end.QuadPart - timer_start.QuadPart) / double(frequency.QuadPart);

	// Rendering
	QueryPerformanceCounter(&timer_start);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glUseProgram(renderProg);
	glUniform1f(1, REST_DENSITY);
	glBindBuffer(GL_ARRAY_BUFFER, position_VBO);
	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(4, GL_FLOAT, 0, 0);

	glViewport(0, 0, 1280 / 2, 720);
	glm::mat4 mvp = glm::perspective(45.0f, 0.88f, 0.1f, 1000.0f)
					* glm::lookAt(
						glm::vec3(-10.f,10.f,-10.f),
						glm::vec3(5.f,5.f,5.f),
						glm::vec3(0.f,1.f,0.f));

	glUniformMatrix4fv(0, 1, GL_FALSE, &(mvp)[0][0]);
	glDrawArrays(GL_POINTS, 0, PARTICLE_COUNT);

	glViewport(1280 / 2, 0, 1280 / 2, 720);
	mvp = glm::ortho(GRID_ORIGIN[0], 
					GRID_CELL_SIZE * (float)GRID_CELL_COUNT / 2,
					GRID_ORIGIN[2],
					GRID_CELL_SIZE * (float)GRID_CELL_COUNT / 2, 0.f,
					20.f)
		* glm::rotate(glm::mat4(1.f), 90.f, glm::vec3(0.f,1.f,0.f));

	glUniformMatrix4fv(0, 1, GL_FALSE, &(mvp)[0][0]);
	glDrawArrays(GL_POINTS, 0, PARTICLE_COUNT);
	glFinish();
	QueryPerformanceCounter(&timer_end);
	rendertime += double(timer_end.QuadPart - timer_start.QuadPart) / double(frequency.QuadPart);

	QueryPerformanceCounter(&timer_start);
	glDisableClientState(GL_VERTEX_ARRAY);
	glfwSwapBuffers(window);
	glFinish();
	QueryPerformanceCounter(&timer_end);
	swaptime += double(timer_end.QuadPart - timer_start.QuadPart) / double(frequency.QuadPart);
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

	int num_steps = 0;
	int max_steps = 300;
	while (!glfwWindowShouldClose(window) && num_steps < max_steps)
	{
		renderLoop();
		num_steps++;
	}
	std::cout << "grouptime: "     << grouptime / max_steps     << std::endl;
	std::cout << "sorttime: "      << sorttime / max_steps      << std::endl;
	std::cout << "offsettime: "    << offsettime / max_steps    << std::endl;
	std::cout << "denstime: "      << denstime / max_steps      << std::endl;
	std::cout << "accel time: "    << simtime / max_steps       << std::endl;
	std::cout << "update time: "   << rendertime / max_steps    << std::endl;
	std::cout << "rendertime: "    << rendertime / max_steps    << std::endl;
	std::cout << "swaptime: "      << swaptime / max_steps      << std::endl;

	glfwTerminate();
	return 0;
}