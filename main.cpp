#include<iostream>

#include"glad/glad.h"
#include"GLFW/glfw3.h"
#include"shaderClass.h"
#include"frameTimer.h"
#include <string>
#include <vector>
#include <cmath>
#include <thread>
#include <chrono>
#include <algorithm>


const unsigned int SCREEN_WIDTH = 2048;
const unsigned int SCREEN_HEIGHT = 1024;

const unsigned short OPENGL_MAJOR_VERSION = 4;
const unsigned short OPENGL_MINOR_VERSION = 6;

bool vSync = true;

const float FOV = 90.0f;
const unsigned int threadCount = 32;




GLfloat vertices[] =
{
	-1.0f, -1.0f , 0.0f, 0.0f, 0.0f,
	-1.0f,  1.0f , 0.0f, 0.0f, 1.0f,
	 1.0f,  1.0f , 0.0f, 1.0f, 1.0f,
	 1.0f, -1.0f , 0.0f, 1.0f, 0.0f,  
};

GLuint indices[] =
{
	0, 2, 1,
	0, 3, 2
};

struct Vector3
{
	GLfloat x, y, z;
}; 
struct Vector4
{
	GLfloat x, y, z, w;
};
struct Sphere{
	Vector3 position;
	float radius;
	Vector4 color;
	Vector3 velocity;
	float density;
	Vector3 predictedPosition;
	float nearDensity;
	Sphere(float input[]) {
		position.x = input[0];
		position.y = input[1];
		position.z = input[2];
		radius = input[3];
		color.x = input[4];
		color.y = input[5];
		color.z = input[6];
		color.w = input[7];
		velocity = { input[8], input[9], input[10] };
		density = input[11];
		predictedPosition.x = input[0];
		predictedPosition.y = input[1];
		predictedPosition.z = input[2];
		nearDensity = 0;
	}
};
struct SphereIndex {
	unsigned int sphereIndex, hash, cellKey, debug;
	SphereIndex() {
		sphereIndex = 0;
		hash = 0;
		cellKey = 0;
		debug = 0;
	}
	SphereIndex(unsigned int inSphereIndex, unsigned int inCellKey) {
		sphereIndex = inSphereIndex;
		cellKey = inCellKey;
		hash = 0;
		debug = 0;
	}
};

void PrintSpecs() {
	int work_grp_cnt[3];
	glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &work_grp_cnt[0]);
	glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 1, &work_grp_cnt[1]);
	glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 2, &work_grp_cnt[2]);

	int work_grp_size[3];
	glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0, &work_grp_size[0]);
	glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 1, &work_grp_size[1]);
	glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 2, &work_grp_size[2]);

	int work_grp_inv;
	glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, &work_grp_inv);
	std::cout << "Max work groups per compute shader" <<
		" x:" << work_grp_cnt[0] <<
		" y:" << work_grp_cnt[1] <<
		" z:" << work_grp_cnt[2] << "\n";
	std::cout << "Max work group sizes" <<
		" x:" << work_grp_size[0] <<
		" y:" << work_grp_size[1] <<
		" z:" << work_grp_size[2] << "\n";

	std::cout << "Max invocations count per work group: " << work_grp_inv << "\n";
}
struct RenderResources {
	// Geometry
	GLuint VAO, VBO, EBO;

	// GPU pointers
	GLuint screenTex;
	GLuint spheresBuffer;
	GLuint spatialBuffer;
	GLuint spatialIndexBuffer;

	GLuint physicsObjectAmount;
	GLuint physicsGravity;
	GLuint physicsBounds;
	GLuint physicsBoundsPosition;
	GLuint renderObjectAmount;
	GLuint renderBounds;
	GLuint renderBoundsPosition;
	GLuint renderHalfFov;
	GLuint sortObjectAmount;
	GLuint sortGroupWidth;
	GLuint sortGroupHeight;
	GLuint sortStepIndex;
	// Shaders
	Shader screenShader;
	Shader renderShader;
	Shader physicsShader;
	Shader sortShader; 
	RenderResources(const char* vertPath,	const char* fragPath,	const char* renderComputePath,	const char* physicsComputePath, const char* sortComputePath)
		: screenShader(vertPath, fragPath),	renderShader(renderComputePath), physicsShader(physicsComputePath), sortShader(sortComputePath),
		spheresBuffer(0), spatialBuffer(0), spatialIndexBuffer(0),
		physicsObjectAmount(0), renderObjectAmount(0), sortObjectAmount(0), physicsGravity(0), physicsBounds(0), renderHalfFov(0), sortGroupWidth(0), sortGroupHeight(0), sortStepIndex(0), physicsBoundsPosition(0), renderBounds(0), renderBoundsPosition(0),
		screenTex(0), VAO(0), VBO(0), EBO(0) {
	}
};

void CleanupRenderResources(RenderResources& res) {
	glDeleteVertexArrays(1, &res.VAO);
	glDeleteBuffers(1, &res.VBO);
	glDeleteBuffers(1, &res.EBO);
	glDeleteTextures(1, &res.screenTex);
	glDeleteBuffers(1, &res.spheresBuffer);

	res.renderShader.Delete();
	res.physicsShader.Delete();
	res.screenShader.Delete();
	res.sortShader.Delete();
}

RenderResources InitRenderResources(std::vector<Sphere>& spheresArray, std::vector<SphereIndex>& spatialArray,std::vector<unsigned int>& spatialIndexArray) {
	RenderResources res("default.vert", "default.frag", "computeShader.compute","physicsHandler.compute", "GPUSort.compute");

	glCreateVertexArrays(1, &res.VAO);
	glCreateBuffers(1, &res.VBO);
	glCreateBuffers(1, &res.EBO);

	glNamedBufferData(res.VBO, sizeof(vertices), vertices, GL_STATIC_DRAW);
	glNamedBufferData(res.EBO, sizeof(indices), indices, GL_STATIC_DRAW);

	glEnableVertexArrayAttrib(res.VAO, 0);
	glVertexArrayAttribBinding(res.VAO, 0, 0);
	glVertexArrayAttribFormat(res.VAO, 0, 3, GL_FLOAT, GL_FALSE, 0);

	glEnableVertexArrayAttrib(res.VAO, 1);
	glVertexArrayAttribBinding(res.VAO, 1, 0);
	glVertexArrayAttribFormat(res.VAO, 1, 2, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat));

	glVertexArrayVertexBuffer(res.VAO, 0, res.VBO, 0, 5 * sizeof(GLfloat));
	glVertexArrayElementBuffer(res.VAO, res.EBO);

	// Screen texture
	glCreateTextures(GL_TEXTURE_2D, 1, &res.screenTex);
	glTextureParameteri(res.screenTex, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTextureParameteri(res.screenTex, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTextureParameteri(res.screenTex, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTextureParameteri(res.screenTex, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTextureStorage2D(res.screenTex, 1, GL_RGBA32F, SCREEN_WIDTH, SCREEN_HEIGHT);
	glBindImageTexture(0, res.screenTex, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);

	//init buffer
	unsigned int amount = spheresArray.size();

	for (unsigned int i = 0; i < amount; i++) {
		SphereIndex sphereIndex(i,0);
		spatialArray.push_back(sphereIndex);
		spatialIndexArray.push_back(amount+1);
	}
	glCreateBuffers(1, &res.spheresBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, res.spheresBuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER,
		amount * sizeof(Sphere),
		spheresArray.data(),
		GL_DYNAMIC_COPY);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, res.spheresBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	glCreateBuffers(1, &res.spatialBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, res.spatialBuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER,
		amount * sizeof(SphereIndex),
		spatialArray.data(),
		GL_DYNAMIC_COPY);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, res.spatialBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	glCreateBuffers(1, &res.spatialIndexBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, res.spatialIndexBuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER,
		amount * sizeof(unsigned int),
		spatialIndexArray.data(),
		GL_DYNAMIC_COPY);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, res.spatialIndexBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	//get uniform location
	res.physicsObjectAmount=glGetUniformLocation(res.physicsShader.ID, "objectAmount");
	res.renderObjectAmount = glGetUniformLocation(res.renderShader.ID, "objectAmount");
	res.sortObjectAmount = glGetUniformLocation(res.sortShader.ID, "objectAmount");
	res.physicsGravity = glGetUniformLocation(res.physicsShader.ID, "gravity");
	res.physicsBounds = glGetUniformLocation(res.physicsShader.ID, "bounds");
	res.physicsBoundsPosition = glGetUniformLocation(res.physicsShader.ID, "boundsPosition");
	res.renderHalfFov = glGetUniformLocation(res.renderShader.ID, "halffov");
	res.renderBounds = glGetUniformLocation(res.renderShader.ID, "bounds");
	res.renderBoundsPosition = glGetUniformLocation(res.renderShader.ID, "boundsPosition");
	res.sortGroupWidth = glGetUniformLocation(res.sortShader.ID, "groupWidth");
	res.sortGroupHeight = glGetUniformLocation(res.sortShader.ID, "groupHeight");
	res.sortStepIndex = glGetUniformLocation(res.sortShader.ID, "stepIndex");
	return res;
}

static std::vector<Sphere> CreateSphereArray(float center[], float bound[], int amount) {
	float volume = bound[0] * bound[1] * bound[2] / amount;
	float dimension = cbrt(volume);
	float inverseDimension = 1 / dimension;

	std::vector<Sphere> spheres;
	int dimensions[3] = {
		floor(bound[0] * inverseDimension),
		floor(bound[1] * inverseDimension),
		floor(bound[2] * inverseDimension)
	};

	for (int i = 0; i < dimensions[0]; i++) {
		for (int j = 0; j < dimensions[1]; j++) {
			for (int k = 0; k < dimensions[2]; k++) {
				float positionX = center[0]+(i - dimensions[0] / 2) * dimension;
				float positionY = center[1] + (j - dimensions[1] / 2) * dimension;
				float positionZ = center[2] + (k - dimensions[2] / 2) * dimension;
				float radius = 0.75;

				float colorR = static_cast<float>(rand()) / RAND_MAX;
				float colorG = static_cast<float>(rand()) / RAND_MAX;
				float colorB = static_cast<float>(rand()) / RAND_MAX;
				float colorA = 1.f;

				Sphere newSphere(new float[]{
					positionX, positionY, positionZ, radius,
						colorR, colorG, colorB, colorA, 0, 0, 0, 1
					});
				spheres.push_back(newSphere);
			}
		}
	}
	return spheres;
}


unsigned int BitAmount(unsigned int input) {
	input--;       
	input |= input >> 1;  
	input |= input >> 2;
	input |= input >> 4;
	input |= input >> 8;
	input |= input >> 16;
	input |= input >> 32;
	input++;
	return input;
}
void Sort(RenderResources& res, unsigned int objectAmount) {
	res.sortShader.Activate();
	glUniform1ui(res.sortObjectAmount, objectAmount);

	
	unsigned int stages= log2(BitAmount(objectAmount));
	for (int stageIndex = 0; stageIndex < stages; stageIndex++)
	{
		for (int stepIndex = 0; stepIndex < stageIndex + 1; stepIndex++)
		{
			// Calculate patterns
			int groupWidth = 1 << (stageIndex - stepIndex);
			int groupHeight = 2 * groupWidth - 1;
			glUniform1ui(res.sortGroupWidth, groupWidth);
			glUniform1ui(res.sortGroupHeight, groupHeight);
			glUniform1ui(res.sortStepIndex, stepIndex);

			glDispatchCompute(BitAmount(objectAmount) / (2*threadCount), 1, 1);
			GLenum err = glGetError();
			if (err != GL_NO_ERROR) {
				std::cerr << "Sort dispatch failed with error code: " << err << std::endl;
			}
			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
		}
	}
	glUniform1ui(res.sortStepIndex, objectAmount + 1);
	glDispatchCompute(GLuint(objectAmount / threadCount + 1), 1, 1);
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void UpdateFrame(RenderResources& res, std::vector<Sphere>& spheresArray, float bounds[], float boundsPosition[]) {
	//Sort
	unsigned int objectAmount=spheresArray.size();
	// Physics update
	res.physicsShader.Activate();
	glUniform1f(res.physicsGravity, 10.0f);
	glUniform1ui(res.physicsObjectAmount, objectAmount);
	glUniform3f(res.physicsBounds, bounds[0], bounds[1], bounds[2]);
	glUniform3f(res.physicsBoundsPosition, boundsPosition[0], boundsPosition[1], boundsPosition[2]);
	glDispatchCompute(GLuint(objectAmount / threadCount+1), 1, 1);
	GLenum err = glGetError();
	if (err != GL_NO_ERROR) {
		std::cerr << "Physics dispatch failed with error code: " << err << std::endl;
	}
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

	Sort(res, objectAmount);

	res.renderShader.Activate();
	glUniform1f(res.renderHalfFov, FOV/2);
	glUniform1ui(res.renderObjectAmount, objectAmount);
	glUniform3f(res.renderBounds, bounds[0], bounds[1], bounds[2]);
	glUniform3f(res.renderBoundsPosition, boundsPosition[0], boundsPosition[1], boundsPosition[2]);
	glDispatchCompute((SCREEN_WIDTH + 31) / 32,	(SCREEN_HEIGHT + 31) / 32,	1);

	err = glGetError();
	if (err != GL_NO_ERROR) {
		std::cerr << "Render dispatch failed with error code: " << err << std::endl;
	}
	//Draw on screen
	res.screenShader.Activate();
	glBindTextureUnit(0, res.screenTex);
	const char* texture = "screen";
	res.screenShader.GetTexture(texture);
	glBindVertexArray(res.VAO);
	glDrawElements(GL_TRIANGLES,
		sizeof(indices) / sizeof(indices[0]),
		GL_UNSIGNED_INT,
		0);
}

int main() {

	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, OPENGL_MAJOR_VERSION);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, OPENGL_MINOR_VERSION);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	GLFWwindow* window = glfwCreateWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Temmie", NULL, NULL);
	glfwMakeContextCurrent(window);
	glfwSwapInterval(vSync);
	gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
	//init spheres
	float bounds[3] = { 10.f, 7.f, 7.0f };
	float center[3] = { 0.f, 7.f, 0.f };
	int amount =5000;
	std::vector<Sphere> spheres = CreateSphereArray(center, bounds, amount);
	std::vector<SphereIndex> spheresIndices;
	std::vector<unsigned int> spatialIndices;
	//create simulation bounds
	float simulationBounds[3] = { 15.f, 7.f, 10.0f };
	float simulationBoundsPosition[3] = { 0, simulationBounds[1], 0 };
	RenderResources res = InitRenderResources(spheres, spheresIndices,spatialIndices);

	FrameTimer timer;
	timer.Start();

	const double targetDelta = 1.0 / 144.0;

	while (!glfwWindowShouldClose(window)) {
		double frameStart = timer.Tick();

		//UpdateFrame
		UpdateFrame(res, spheres, simulationBounds, simulationBoundsPosition);

		glfwSwapBuffers(window);
		glfwPollEvents();

		//const time
		double frameEnd = timer.GetDelta();
		double elapsed = frameEnd;

		if (elapsed < targetDelta) {
			double sleepTime = targetDelta - elapsed;
			std::this_thread::sleep_for(
				std::chrono::duration<double>(sleepTime)
			);
		}
		float frametime = timer.GetDelta() * 1000.0f;
		float fps = timer.GetFPS();
		std::cout << "FPS: " << fps << "Frame Time:"<<frametime<<"\n";
		}

	CleanupRenderResources(res);

	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}

