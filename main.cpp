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


const unsigned int SCREEN_WIDTH = 2048;
const unsigned int SCREEN_HEIGHT = 1024;

const unsigned short OPENGL_MAJOR_VERSION = 4;
const unsigned short OPENGL_MINOR_VERSION = 6;

bool vSync = true;



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
	GLfloat x, y, z,w;
};
struct alignas(16) Sphere{
	Vector3 position;
	GLfloat radius;
	Vector4 color;
	Vector3 velocity;
	float density;
	
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

	// GPU storage
	GLuint screenTex;       // render target texture
	GLuint spheresBuffer;   // SSBO for sphere data

	// Shaders
	Shader screenShader;    // final blit shader
	Shader renderShader;    // compute shader for rendering
	Shader physicsShader;   // compute shader for physics

	RenderResources(const char* vertPath,	const char* fragPath,	const char* renderComputePath,	const char* physicsComputePath)
		: screenShader(vertPath, fragPath),	renderShader(renderComputePath), physicsShader(physicsComputePath),	spheresBuffer(0), screenTex(0), VAO(0), VBO(0), EBO(0) {
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
}

RenderResources InitRenderResources(std::vector<Sphere>& spheresArray) {
	RenderResources res("default.vert", "default.frag", "computeShader.compute","physicsHandler.compute");

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

	glCreateBuffers(1, &res.spheresBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, res.spheresBuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER,
		spheresArray.size() * sizeof(Sphere),
		spheresArray.data(),
		GL_DYNAMIC_COPY);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, res.spheresBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

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
				float radius = dimension*2;

				float colorR = static_cast<float>(rand()) / RAND_MAX;
				float colorG = static_cast<float>(rand()) / RAND_MAX;
				float colorB = static_cast<float>(rand()) / RAND_MAX;
				float colorA = 1.f;

				Sphere newSphere(new float[]{
					positionX, positionY, positionZ, radius,
						colorR, colorG, colorB, colorA, 0, 0, 0, 0
					});
				spheres.push_back(newSphere);
			}
		}
	}
	return spheres;
}
void UpdateFrame(RenderResources& res, std::vector<Sphere>& spheresArray, float bounds[]) {
	// Physics update
	res.physicsShader.Activate();
	glUniform1f(glGetUniformLocation(res.physicsShader.ID, "gravity"), 0.1f);
	glUniform1ui(glGetUniformLocation(res.physicsShader.ID, "objectAmount"), spheresArray.size());
	glUniform3f(glGetUniformLocation(res.physicsShader.ID, "bounds"), bounds[0], bounds[1], bounds[2]);
	glDispatchCompute(GLuint(floor( spheresArray.size() / 32)+1), 1, 1);
	
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

	res.renderShader.Activate();
	GLuint halffov = glGetUniformLocation(res.renderShader.ID, "halffov");
	GLuint objectAmountRender = glGetUniformLocation(res.renderShader.ID, "objectAmount");
	glUniform1f(halffov, 45.0f);
	glUniform1ui(objectAmountRender, spheresArray.size());
	glDispatchCompute((SCREEN_WIDTH + 31) / 32,	(SCREEN_HEIGHT + 31) / 32,	1);

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
	float bounds[3] = { 5.f, 3.f, 3.f };
	float center[3] = { 0.f, 0.f, 0.f };
	int amount = 2000;
	std::vector<Sphere> spheres = CreateSphereArray(center, bounds, amount);

	//create simulation bounds
	float simulationBounds[4] = { 10.f, 5.f, 5.f };
	RenderResources res = InitRenderResources(spheres);

	FrameTimer timer;
	timer.Start();

	const double targetDelta = 1.0 / 60.0;

	while (!glfwWindowShouldClose(window)) {
		double frameStart = timer.Tick();

		// Physics + render combined
		UpdateFrame(res, spheres, simulationBounds);

		// Swap buffers and poll events
		glfwSwapBuffers(window);
		glfwPollEvents();

		// Busy-wait or sleep until 16.67 ms has passed
		double frameEnd = timer.GetDelta();
		double elapsed = frameEnd;

		if (elapsed < targetDelta) {
			double sleepTime = targetDelta - elapsed;
			std::this_thread::sleep_for(
				std::chrono::duration<double>(sleepTime)
			);
		}
		float frametime = timer.GetDelta() * 1000.0f; // Convert to milliseconds
		float fps = timer.GetFPS();
		std::cout << "FPS: " << fps << "Frame Time:"<<frametime<<"\n";
		}
	// Cleanup GPU resources
	CleanupRenderResources(res);

	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}

