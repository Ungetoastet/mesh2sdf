#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <iomanip>

#include "headers/tinyjson.h"
#include "headers/functions.h"
#include "headers/shader.h"
#include "headers/camera.h"
#include "headers/sdf_cache.h"
#include "headers/conversionmethods.h"
#include "headers/debugView.h"

#include "headers/model.h"
#include "headers/benchmarking.h"

#include "headers/simpletext.h"

Camera camera(glm::vec3(0.0f, 0.0f, 3.0f));

// timing
float deltaTime = 0.0f; // time between current frame and last frame
float lastFrame = 0.0f;

void framebuffer_size_callback(GLFWwindow *window, int width, int height);

#include <iostream>
#include <string>
#include <cstdlib>

int main(int argc, char **argv)
{
	sdf_conversion::sdfSize = 128;

	unsigned int particleCount = 10000000;
	float particleSize = 0.01f;
	glm::vec3 simbox = glm::vec3(2.5);
	glm::vec3 rotationAxis = glm::vec3(1.0, 1.0, 1.0);
	float rotationSpeed = 1.0f;

	std::cout << "Initialising openGL... " << std::flush;

	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_SAMPLES, 8);

	GLFWwindow *window = glfwCreateWindow(1, 1, "Mesh2SDF Playground Particle Demo", NULL, NULL);
	glfwMaximizeWindow(window);

	if (window == NULL)
	{
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);
	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		std::cout << "Failed to initialize GLAD" << std::endl;
		return -1;
	}

	glEnable(GL_MULTISAMPLE);

	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	// glad: load all OpenGL function pointers
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		std::cout << "Failed to initialize GLAD" << std::endl;
		return -1;
	}

	glm::mat4 model(1.0);

	glEnable(GL_DEPTH_TEST);

	// load shaders
	std::cout << "\rLoading draw shaders...                 " << std::flush;
	Shader *shaderRaster = new Shader("./shaders/raster.vs", "./shaders/raster.fs");

	std::vector<string> shaderNames = {
		"RASTERIZE (NO SDF)",
		"NAIVE",
		"JUMP FLOOD",
		"BATTY SWEEP",
		"RAY MAPS",
	};

	std::cout << "\rLoading conversion shaders...                 " << std::flush;
	sdf_conversion::CompileConversionShaders();

	std::map<std::string, Model *> modelMap;
	Model *demomodel = new Model("./Objs/dragon/xyzrgb_dragon.obj", window, shaderNames, "MONKEY", 0.2f, true);
	Model *floormodel = new Model("./Objs/screen/screen.obj", window, shaderNames, "FLATSCREEN");
	std::cout << demomodel->errors << std::endl;

	glEnable(GL_DEBUG_OUTPUT);
	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
	glDebugMessageCallback([](GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const void *userParam)
						   { 
							int s = severity - 0x9146;
							std::vector<std::string> severitystring = {
								"HIGH",
								"MEDIUM",
								"LOW",
								"NOTIFICATION"
							};
							std::cerr << "\nGL DEBUG " << severitystring[s] << " SEVERITY: " << message
									  << std::endl; }, nullptr);

	Shader *particleInit = new Shader("./shaders/particles/init.comp");
	particleInit->use();
	GLuint particleBuffer;
	glGenBuffers(1, &particleBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, particleBuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, 32 * particleCount, nullptr, GL_DYNAMIC_DRAW);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, particleBuffer);
	particleInit->setVec3("spawnCube", simbox);
	particleInit->setUInt("particleCount", particleCount);
	float side = ceil(sqrt((float)particleCount));
	uint groups = (uint)ceil(side / 8.0f);
	glDispatchCompute(groups, groups, groups);
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

	Shader *particleDraw = new Shader("./shaders/particles/p.vs", "./shaders/particles/p.fs");

	GLuint particleVAO;
	glGenVertexArrays(1, &particleVAO);

	Shader *particleUpdate = new Shader("./shaders/particles/update.comp");

	std::string convMethod = "RAY MAPS";

	while (!glfwWindowShouldClose(window))
	{
		if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS)
		{
			particleInit->use();
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, particleBuffer);
			glBufferData(GL_SHADER_STORAGE_BUFFER, 32 * particleCount, nullptr, GL_DYNAMIC_DRAW);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, particleBuffer);
			particleInit->setVec3("spawnCube", simbox);
			particleInit->setUInt("particleCount", particleCount);
			float side = ceil(sqrt((float)particleCount));
			uint groups = (uint)ceil(side / 8.0f);
			glDispatchCompute(groups, groups, groups);
			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
		}

		// per-frame time logic
		glEnable(GL_BLEND);
		float currentFrame = glfwGetTime();
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		int fbWidth, fbHeight;
		glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
		glViewport(0, 0, fbWidth, fbHeight);

		camera.Orbit(window, deltaTime);

		// View and proj matrix
		glm::mat4 view = camera.GetViewMatrix();
		glm::mat4 proj = glm::perspective(glm::radians(45.0f),
										  static_cast<float>(fbWidth) / static_cast<float>(fbHeight),
										  0.1f, 1000.0f);
		glm::mat4 dmodel = glm::rotate(glm::mat4(1.0f), (float)glfwGetTime() * rotationSpeed, rotationAxis);

		shaderRaster->use();
		shaderRaster->setBool("sdfDebug", false);
		shaderRaster->setVec3("camera_pos", camera.Position);
		shaderRaster->setMat("projection", proj);
		shaderRaster->setMat("model", dmodel);
		shaderRaster->setVec2("screen_resolution", fbWidth, fbHeight);
		shaderRaster->setMat("view", view);
		shaderRaster->setFloat("alpha", 1.0);
		glm::mat4 invView = glm::inverse(view);
		shaderRaster->setMat("inverse_view", invView);

		demomodel->Draw(*shaderRaster);

		model = glm::mat4(1.0);
		model = glm::rotate(model, glm::radians(-90.0f), glm::vec3(1.0, 0.0, 0.0));
		model = glm::translate(model, glm::vec3(0.0, 0.0, -simbox.x + 0.01));
		model = glm::scale(model, simbox);
		shaderRaster->setMat("model", model);
		floormodel->Draw(*shaderRaster);
		model = glm::mat4(1.0);
		model = glm::rotate(model, glm::radians(90.0f), glm::vec3(1.0, 0.0, 0.0));
		model = glm::translate(model, glm::vec3(0.0, 0.0, simbox.x));
		model = glm::scale(model, simbox);
		shaderRaster->setMat("model", model);
		floormodel->Draw(*shaderRaster);

		float shaderStart = glfwGetTime();

		// Conversion und Particles
		((sdf_conversion::RaymapConversion *)demomodel->methods[convMethod])->model = dmodel;
		demomodel->methods[convMethod]->PrepareSDF(false);

		particleUpdate->use();
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, particleBuffer);
		glActiveTexture(GL_TEXTURE15);
		GLuint sdfTexID = demomodel->GetSDFByName(convMethod);
		glBindTexture(GL_TEXTURE_3D, sdfTexID);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

		particleUpdate->setInt("sdf", 15);
		particleUpdate->SetMat2x3("boundingBox", demomodel->bounds);
		particleUpdate->setUInt("particleCount", particleCount);
		particleUpdate->setFloat("deltaTime", deltaTime);
		particleUpdate->setFloat("grav", 0.0f);
		particleUpdate->setFloat("bounce", 0.5f);
		particleUpdate->setFloat("mag", -0.5f);
		particleUpdate->setVec3("simulationBox", simbox);
		uint groups = (particleCount + 1023) / 1024;
		glDispatchCompute(groups, 1, 1);

		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);

		particleDraw->use();
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, particleBuffer);
		particleDraw->setMat("view", view);
		particleDraw->setMat("projection", proj);
		particleDraw->setFloat("size", particleSize);
		glBindVertexArray(particleVAO);
		glDrawArrays(GL_POINTS, 0, particleCount);

		// Sync up with GPU for time measurement
		glFinish();

		float shaderTime = glfwGetTime() - shaderStart;

		// Format performance string
		std::ostringstream ss;
		ss << std::fixed << std::setprecision(1);
		ss << "FRAME TIME: " << deltaTime * 1000 << "MS - SHADER TIME: " << shaderTime * 1000 << "MS (" << 1 / shaderTime << " FPS)";
		std::string perfText = ss.str();

		// Current performance
		SimpleText::drawBox(fbWidth, fbHeight, glm::vec4(0.0, 0.0, 0.5, 0.5), 10, 10, fbWidth - 2 * 10, 10);
		SimpleText::drawText(fbWidth, fbHeight, glm::vec4(1.0), perfText, 10, 10, 10, 0.9);

		// Current model info
		SimpleText::drawText(fbWidth, fbHeight, glm::vec4(1.0, 1.0, 1.0, 0.2), "PARTICLE COUNT: " + readableInt(particleCount), 10, 10, fbHeight - 20, 0.9);
		SimpleText::drawText(fbWidth, fbHeight, glm::vec4(1.0, 1.0, 1.0, 0.2), "SDF SIZE: " + std::to_string(sdf_conversion::sdfSize), 10, 10, fbHeight - 35, 0.9);
		SimpleText::drawText(fbWidth, fbHeight, glm::vec4(1.0, 1.0, 1.0, 0.2), "MODEL FACES: " + readableInt(demomodel->GetModelFaces()), 10, 10, fbHeight - 50, 0.9);

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	std::cout << "\nExit!" << std::endl;
	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}

void framebuffer_size_callback(GLFWwindow *window, int width, int height)
{
	glViewport(0, 0, width, height);
}
