#include<iostream>

#include"glad/glad.h"
#include"GLFW/glfw3.h"
#include"shaderClass.h"
#include"frameTimer.h"
#include"math.h"
#include <string>
#include <vector>
#include <cmath>
#include <thread>
#include <chrono>
#include <algorithm>


const unsigned int SCREEN_WIDTH = 1244;
const unsigned int SCREEN_HEIGHT = 1024;

const unsigned short OPENGL_MAJOR_VERSION = 4;
const unsigned short OPENGL_MINOR_VERSION = 6;

bool vSync = true;

const float FOV = 90.0f;
const unsigned int threadCount = 32;
const float maxRotationSpeed = 0.05f;
const float maxTranslationSpeed = 1.f;
const float boxGravity = 40.f;

float downwardSpeed = 0.0000f;
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
		nearDensity = input[11];
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
float Clamp(float value, float minVal, float maxVal) {
	if (value < minVal) return minVal;
	if (value > maxVal) return maxVal;
	return value;
}
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
	GLuint densityTex;

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
	GLuint densityObjectAmount;
	GLuint densityBounds;
	GLuint densityBoundsPosition;
	GLuint densityResolution;
	GLuint renderResolution;
	GLuint physicsBoundsMatrix;
	GLuint physicsInverseBoundsMatrix;
	GLuint renderBoundsMatrix;
	GLuint densityBoundsMatrix;
	GLuint renderInverseBoundsMatrix;
	GLuint densityInverseBoundsMatrix;
	// Shaders
	Shader screenShader;
	Shader renderShader;
	Shader physicsShader;
	Shader sortShader; 
	Shader densityShader;
	RenderResources(const char* vertPath,	const char* fragPath,	const char* renderComputePath,	const char* physicsComputePath, const char* sortComputePath, const char* densityComputePath)
		: screenShader(vertPath, fragPath),	renderShader(renderComputePath), physicsShader(physicsComputePath), sortShader(sortComputePath), densityShader(densityComputePath),
		spheresBuffer(0), spatialBuffer(0), spatialIndexBuffer(0),
		physicsObjectAmount(0), renderObjectAmount(0), sortObjectAmount(0), physicsGravity(0), physicsBounds(0), renderHalfFov(0), sortGroupWidth(0), sortGroupHeight(0), sortStepIndex(0), physicsBoundsPosition(0), renderBounds(0), renderBoundsPosition(0), densityBounds(0), densityBoundsPosition(0), densityResolution(0), densityObjectAmount(0), renderResolution(0), densityTex(0), physicsBoundsMatrix(0), physicsInverseBoundsMatrix(0), renderBoundsMatrix(0), densityBoundsMatrix(0), renderInverseBoundsMatrix(0), densityInverseBoundsMatrix(0),
		screenTex(0), VAO(0), VBO(0), EBO(0) {
	}
};

void CleanupRenderResources(RenderResources& res) {
	glDeleteVertexArrays(1, &res.VAO);
	glDeleteBuffers(1, &res.VBO);
	glDeleteBuffers(1, &res.EBO);
	glDeleteTextures(1, &res.screenTex);
	glDeleteBuffers(1, &res.spheresBuffer);
	glDeleteBuffers(1, &res.spatialBuffer);
	glDeleteBuffers(1, &res.spatialIndexBuffer);
	glDeleteTextures(1, &res.densityTex);

	res.renderShader.Delete();
	res.physicsShader.Delete();
	res.screenShader.Delete();
	res.sortShader.Delete();
	res.densityShader.Delete();
}

RenderResources InitRenderResources(std::vector<Sphere>& spheresArray, std::vector<SphereIndex>& spatialArray, std::vector<unsigned int>& spatialIndexArray,unsigned int densityResolution[]) {
	RenderResources res("default.vert", "default.frag", "computeShader.compute","physicsHandler.compute", "GPUSort.compute","DensitySampler.compute");

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


	glCreateTextures(GL_TEXTURE_3D, 1, &res.densityTex);
	glTextureStorage3D(res.densityTex, 1, GL_RG16F, densityResolution[0], densityResolution[1], densityResolution[2]);

	glTextureParameteri(res.densityTex, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTextureParameteri(res.densityTex, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glTextureParameteri(res.densityTex, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTextureParameteri(res.densityTex, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTextureParameteri(res.densityTex, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	glBindImageTexture(4,res.densityTex,0,GL_TRUE,0,GL_WRITE_ONLY, GL_RG16F);

	//init buffer
	unsigned int amount = spheresArray.size();

	for (unsigned int i = 0; i < amount; i++) {
		SphereIndex sphereIndex(i,0);
		spatialArray.push_back(sphereIndex);
		spatialIndexArray.push_back(amount+1);
	}
	double totalDensityResolution = densityResolution[0] * densityResolution[1] * densityResolution[2];
	
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
	res.densityObjectAmount = glGetUniformLocation(res.densityShader.ID, "objectAmount");
	res.densityBounds = glGetUniformLocation(res.densityShader.ID, "bounds");
	res.densityBoundsPosition = glGetUniformLocation(res.densityShader.ID, "boundsPosition");
	res.densityResolution = glGetUniformLocation(res.densityShader.ID, "resolution");
	res.renderResolution = glGetUniformLocation(res.renderShader.ID, "resolution");
	res.physicsBoundsMatrix = glGetUniformLocation(res.physicsShader.ID, "boundsMatrix");
	res.renderBoundsMatrix = glGetUniformLocation(res.renderShader.ID, "boundsMatrix");
	res.densityBoundsMatrix = glGetUniformLocation(res.densityShader.ID, "boundsMatrix");
	res.physicsInverseBoundsMatrix = glGetUniformLocation(res.physicsShader.ID, "inverseBoundsMatrix");
	res.renderInverseBoundsMatrix = glGetUniformLocation(res.renderShader.ID, "inverseBoundsMatrix");
	res.densityInverseBoundsMatrix = glGetUniformLocation(res.densityShader.ID, "inverseBoundsMatrix");
	return res;
}

void CreateSphereArray(static std::vector<Sphere>& spheres, float center[], float bound[], int amount) {
	spheres.reserve(amount);

	for (int i = 0; i < amount; i++) {
		float positionX = center[0] + ((static_cast<float>(rand()) / RAND_MAX) * 2 - 1) * bound[0];
		float positionY = center[1] + ((static_cast<float>(rand()) / RAND_MAX) * 2 - 1) * bound[1];
		float positionZ = center[2] + ((static_cast<float>(rand()) / RAND_MAX) * 2 - 1) * bound[2];
		float radius = 1.25f;

		float colorR = 0;
		float colorG = 0;
		float colorB = 0;
		float colorA = 1.0f;

		Sphere newSphere(new float[] {
			positionX, positionY, positionZ, radius,
				colorR, colorG, colorB, colorA,
				0, 0, 0, 0.1f
			});
		spheres.push_back(newSphere);
	}
	return;
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

void UpdatePhysics(RenderResources& res, std::vector<Sphere>& spheresArray, float bounds[], float boundsPosition[], Mat4& boundsMatrix, Mat4& inverseBoundsMatrix) {
	unsigned int objectAmount = spheresArray.size();
	res.physicsShader.Activate();
	glUniform1f(res.physicsGravity, 20.0f);
	glUniform1ui(res.physicsObjectAmount, objectAmount);
	glUniform3f(res.physicsBounds, bounds[0], bounds[1], bounds[2]);
	glUniform3f(res.physicsBoundsPosition, boundsPosition[0], boundsPosition[1], boundsPosition[2]);
	glUniformMatrix4fv(res.physicsBoundsMatrix, 1, GL_FALSE, &boundsMatrix[0][0]);
	glUniformMatrix4fv(res.physicsInverseBoundsMatrix, 1, GL_FALSE, &inverseBoundsMatrix[0][0]);
	glDispatchCompute(objectAmount / threadCount + 1, 1, 1);
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

	Sort(res, objectAmount); // keep sorting tied to physics
}

void RenderFrame(RenderResources& res, std::vector<Sphere>& spheresArray, float bounds[], float boundsPosition[], unsigned int densityResolution[], Mat4& boundsMatrix, Mat4& inverseBoundsMatrix) {
	unsigned int objectAmount = spheresArray.size();

	res.densityShader.Activate();
	glUniform1ui(res.densityObjectAmount, objectAmount);
	glUniform3f(res.densityBounds, bounds[0], bounds[1], bounds[2]);
	glUniform3f(res.densityBoundsPosition, boundsPosition[0], boundsPosition[1], boundsPosition[2]);
	glUniform3ui(res.densityResolution, densityResolution[0], densityResolution[1], densityResolution[2]);
	glUniformMatrix4fv(res.densityBoundsMatrix, 1, GL_FALSE, &boundsMatrix[0][0]);
	glUniformMatrix4fv(res.densityInverseBoundsMatrix, 1, GL_FALSE, &inverseBoundsMatrix[0][0]);

	glDispatchCompute(densityResolution[0]/8+1, densityResolution[1] / 8 + 1, densityResolution[2] / 8 + 1);

	GLenum err = glGetError();
	if (err != GL_NO_ERROR) {
		std::cerr << "Density dispatch failed, error: " << err << std::endl;
	}

	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

	res.renderShader.Activate();
	glBindTextureUnit(0, res.densityTex);
	glUniform1f(res.renderHalfFov, FOV / 2);
	glUniform1ui(res.renderObjectAmount, objectAmount);
	glUniform3f(res.renderBounds, bounds[0], bounds[1], bounds[2]);
	glUniform3f(res.renderBoundsPosition, boundsPosition[0], boundsPosition[1], boundsPosition[2]);
	glUniform3ui(res.renderResolution, densityResolution[0], densityResolution[1], densityResolution[2]);
	glUniformMatrix4fv(res.renderBoundsMatrix, 1, GL_FALSE, &boundsMatrix[0][0]);
	glUniformMatrix4fv(res.renderInverseBoundsMatrix, 1, GL_FALSE, &inverseBoundsMatrix[0][0]);

	glDispatchCompute((SCREEN_WIDTH + 31) / 32, (SCREEN_HEIGHT + 31) / 32, 1);
	err = glGetError();
	if (err != GL_NO_ERROR) {
		std::cerr << "Render dispatch failed, error: " << err << std::endl;
	}
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

	res.screenShader.Activate();
	glBindTextureUnit(0, res.screenTex);
	res.screenShader.GetTexture("screen");
	glBindVertexArray(res.VAO);
	glDrawElements(GL_TRIANGLES, sizeof(indices) / sizeof(indices[0]), GL_UNSIGNED_INT, 0);
}


void HandleInputs(GLFWwindow* window,
	float boundsPosition[],
	float boundsRotation[],   // rotation state passed in
	float boundsScale[],
	Mat4& boundsMatrix,
	Mat4& inverseBoundsMatrix,
	float deltaTime)
{
	static double lastX = 0, lastY = 0;
	static bool dragging = false;

	if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
		double xpos, ypos;
		glfwGetCursorPos(window, &xpos, &ypos);

		if (!dragging) {
			dragging = true;
			lastX = xpos;
			lastY = ypos;
		}
		else {
			double dx = xpos - lastX;
			double dy = ypos - lastY;

			boundsRotation[1] += Clamp((float)dx * 0.0005f, -maxRotationSpeed,maxRotationSpeed);
			boundsRotation[0] += Clamp((float)dy * 0.0005f, -maxRotationSpeed, maxRotationSpeed);
			lastX = xpos;
			lastY = ypos;
		}
		downwardSpeed = 0;
	}
	else if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS){
		double xpos, ypos;
		glfwGetCursorPos(window, &xpos, &ypos);

		if (!dragging) {
			dragging = true;
			lastX = xpos;
			lastY = ypos;
		}
		else {
			double dx = xpos - lastX;
			double dy = lastY- ypos;

			boundsPosition[0] += Clamp((float)dx * 0.1f, -maxTranslationSpeed, maxTranslationSpeed);
			boundsPosition[1] += Clamp((float)dy * 0.1f, -maxTranslationSpeed, maxTranslationSpeed);
			lastX = xpos;
			lastY = ypos;
		}
		downwardSpeed = 0;
	}
	else {
		dragging = false;
		downwardSpeed += boxGravity*deltaTime;
		boundsPosition[1] -= downwardSpeed * deltaTime; // Apply constant downward movement
	}
	// collision
	float penetrationDepth = 0.0f;

	for (char i = 0; i < 2; i++) {
		for (char j = 0; j < 2; j++) {
			for (char k = 0; k < 2; k++) {
				float local[4] = {
					(i ? 1: -1),
					(j ? 1 : -1) ,
					(k ? 1 : -1),
					1.0f
				};
				float world[4] = { 0,0,0,0 };
				for (int row = 0; row < 4; row++) {
					for (int col = 0; col < 4; col++) {
						world[row] += local[col] * boundsMatrix[col][row];
					}
				}
				if (world[1] < 0.0f) {
					float depth = -world[1];
					if (depth > penetrationDepth) {
						penetrationDepth = depth;
					}
				}
			}
		}
	}

	if (penetrationDepth > 0.0f) {
		boundsPosition[1] += penetrationDepth;
		downwardSpeed = 0;
	}

	boundsMatrix = CalculateModelMatrix(boundsPosition, boundsRotation, boundsScale);
	inverseBoundsMatrix = CalculateInverseModelMatrix(boundsPosition, boundsRotation, boundsScale);
}


int main() {

	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, OPENGL_MAJOR_VERSION);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, OPENGL_MINOR_VERSION);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	GLFWwindow* window = glfwCreateWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Temmie", NULL, NULL);

	glfwMakeContextCurrent(window);
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	glfwSwapInterval(vSync);
	gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
	//init spheres
	float bounds[3] = {15.f, 15.f, 15.f };
	float center1[3] = { -0.f, 40.f, 0.f };
	float center2[3] = { 45.f, 50.f, 0.f };
	int amount =30000;
	float simulationBoundsScale[3] = { 30.f, 30.f, 30.f };
	float simulationBoundsPosition[3] = { 0, simulationBoundsScale[1]+30, 0 };
	float simulationBoundsRotation[3] = { 0, 0.3, 0 };
	unsigned int densityResolution[3] = { simulationBoundsScale[0] * 7,simulationBoundsScale[1] * 7,simulationBoundsScale[2] * 7 };
	std::vector<Sphere> spheres;
	CreateSphereArray(spheres,center1, bounds, amount);
	//CreateSphereArray(spheres, center2, bounds, amount);
	std::vector<SphereIndex> spheresIndices;
	std::vector<unsigned int> spatialIndices;
	RenderResources res = InitRenderResources(spheres, spheresIndices,spatialIndices, densityResolution);


	Mat4 boundsMatrix = CalculateModelMatrix(simulationBoundsPosition, simulationBoundsRotation, simulationBoundsScale);
	Mat4 inverseBoundsMatrix = CalculateInverseModelMatrix(simulationBoundsPosition, simulationBoundsRotation, simulationBoundsScale);
	const double targetDeltaPhysics = 1.0 / 500;
	const double targetDeltaRender = 1.0 / 60.0;
	double physicsTracker = 0;
	double renderTracker = 0;
	double trackedTime = 0;
	unsigned int physicsFrames = 0;
	unsigned int renderFrames = 0;
	FrameTimer timer;
	timer.Start();
	while (!glfwWindowShouldClose(window)) {
		double frameStart = timer.Tick();
		
		//UpdateFrame
		if (physicsTracker>targetDeltaPhysics){
			UpdatePhysics(res, spheres, simulationBoundsScale, simulationBoundsPosition, boundsMatrix, inverseBoundsMatrix);
			physicsTracker = 0;
			physicsFrames++;
		}
		if (renderTracker > targetDeltaRender) {
			RenderFrame(res, spheres, simulationBoundsScale, simulationBoundsPosition,densityResolution, boundsMatrix, inverseBoundsMatrix);
			renderTracker = 0;
			renderFrames++;
			glfwSwapBuffers(window);
		}
		
		double frameEnd = timer.GetDelta();
		double elapsed = frameEnd;

		physicsTracker += elapsed;
		renderTracker += elapsed;
		trackedTime += elapsed;
		if (trackedTime > 1.0) {
			std::cout << "FPS: " << renderFrames << " | Simulations per second: " << physicsFrames << "\n";
			std::cout << "Average frame time: " <<(trackedTime /renderFrames) * 1000 << " ms\n";
			std::cout << "Average simulation time: " << (trackedTime / physicsFrames) * 1000 << " ms\n";
			trackedTime = 0;
			physicsFrames = 0;
			renderFrames = 0;
		}
		glfwPollEvents();

		HandleInputs(window, simulationBoundsPosition, simulationBoundsRotation, simulationBoundsScale, boundsMatrix, inverseBoundsMatrix, elapsed);
		}

	CleanupRenderResources(res);

	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}

