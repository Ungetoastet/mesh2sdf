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

// Used to get the entire model into frame when loaded
float largestBoundDimension = 10;
glm::vec3 boundsCenter = glm::vec3(0.0);

#include <iostream>
#include <string>
#include <cstdlib>

int main(int argc, char **argv)
{
	int benchmark = 0;
	int testSize = 10000;
	int testStart = 0;
	int sdf = 128;

	for (int i = 1; i < argc; ++i)
	{
		std::string arg = argv[i];

		if (arg == "--benchmark")
		{
			benchmark = 1;
		}
		else if (arg == "--thingy10k")
		{
			benchmark = 2;
		}
		else if (arg == "--testsize")
		{
			if (i + 1 >= argc)
			{
				std::cerr << "--testsize requires a value\n";
				return -1;
			}

			testSize = std::stoi(argv[++i]); // consume next arg
			if (testSize > 10000 || testSize < 1)
			{
				std::cout << "--testsize must be in between 1 and 10.000" << std::endl;
				return -2;
			}
		}
		else if (arg == "--sdfsize")
		{
			if (i + 1 >= argc)
			{
				std::cerr << "--sdfsize requires a value\n";
				return -1;
			}

			sdf = std::stoi(argv[++i]); // consume next arg
			if (benchmark > 0)
			{
				std::cout << "Warning: sdfsize is ignored for benchmarking";
			}
			if (sdf > 512 || sdf < 1)
			{
				std::cout << "--sdfsize must be in between 1 and 512" << std::endl;
				return -2;
			}
		}
		else if (arg == "--teststart")
		{
			if (i + 1 >= argc)
			{
				std::cerr << "--teststart requires a value\n";
				return -1;
			}

			testStart = std::stoi(argv[++i]); // consume next arg
			if (testStart > 10000 || testStart < 0)
			{
				std::cout << "--teststart must be in between 0 and 10000" << std::endl;
				return -2;
			}
		}
		else
		{
			std::cerr << "Unknown flag " << arg << "\n";
			return -1;
		}
	}
	sdf_conversion::sdfSize = sdf;

	std::cout << "Initialising openGL... " << std::flush;

	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_SAMPLES, 8);

	GLFWwindow *window = glfwCreateWindow(1, 1, "Mesh2SDF Playground", NULL, NULL);
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

	glm::mat4 model(0);

	glEnable(GL_DEPTH_TEST);

	// load shaders
	std::cout << "\rLoading draw shaders...                 " << std::flush;
	Shader *shaderRaymarchFlat = new Shader("shaders/raymarch.vs", "shaders/raymarch_tex.fs");
	Shader *shaderRaster = new Shader("shaders/raster.vs", "shaders/raster.fs");

	std::vector<string> shaderNames = {
		"RASTERIZE (NO SDF)",
		"NAIVE",
		"JUMP FLOOD",
		"BATTY SWEEP",
		"RAY MAPS",
	};

	string activeShader = shaderNames[0];

	std::cout << "\rLoading conversion shaders...                 " << std::flush;
	sdf_conversion::CompileConversionShaders();

	// False if using cached SDF, True if realtime
	bool realtimeSDF = false;
	bool debugPlane = false;
	GLuint emptySDF;
	glGenTextures(1, &emptySDF);

	std::map<std::string, Model *> modelMap;
	std::vector<std::pair<std::string, std::string>> modelsToLoad = {
		{"CUBE", "./Objs/cube/cube.obj"},
		{"DONUTS", "./Objs/donuts/donut.obj"},
		{"DRAGON", "./Objs/dragon/xyzrgb_dragon.obj"},
		{"LUCY", "./Objs/lucy/lucy.obj"},
		{"MONKEY", "./Objs/monkey/monkey.obj"},
		{"PILLOW", "./Objs/pillow/pillow.obj"},
		{"FLATSCREEN", "./Objs/screen/screen.obj"},
		{"POT", "./Objs/teapot/utah_teapot.obj"},
		{"TORUS", "./Objs/torus/torus.obj"},
	};

	string currentDemoModel = "CUBE";
	string terminalResponse = "";
	float terminalResponseTime = 0;
	unsigned int currentLoadedVerts, currentLoadedFaces;
	if (benchmark != 2)
	{
		int current = 1;
		for (const auto &[name, path] : modelsToLoad)
		{
			std::cout << "\rLoading model " << name << " (" << current++ << "/" << modelsToLoad.size() << ")                   " << std::flush;
			modelMap[name] = new Model(path, window, shaderNames, name);
		}

		std::cout << "\r                                             " << std::flush;

		currentLoadedVerts = modelMap[currentDemoModel]->GetModelVertices();
		currentLoadedFaces = modelMap[currentDemoModel]->GetModelFaces();
	}

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
							std::cerr << "\nGL DEBUG " << severitystring[s] << " SEVERITY: " << message << "\n"
									  << std::endl; }, nullptr);

	while (!glfwWindowShouldClose(window) && !benchmark)
	{
		// per-frame time logic
		glEnable(GL_BLEND);
		float currentFrame = glfwGetTime();
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;
		glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		int fbWidth, fbHeight;
		glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
		glViewport(0, 0, fbWidth, fbHeight);

		// shader switching
		for (unsigned int i = 0; i < shaderNames.size(); i++)
		{
			if (glfwGetKey(window, GLFW_KEY_1 + i) == GLFW_PRESS)
			{
				activeShader = shaderNames[i];
				if (i == 0)
					debugPlane = false;
			}
		}

		// Debug plane is always rasterized
		Shader *renderShader = (activeShader == shaderNames[0] || debugPlane) ? shaderRaster : shaderRaymarchFlat;

		renderShader->use();

		camera.Orbit(window, deltaTime);

		// View and proj matrix
		glm::mat4 view = camera.GetViewMatrix();
		glm::mat4 proj = glm::perspective(glm::radians(45.0f),
										  static_cast<float>(fbWidth) / static_cast<float>(fbHeight),
										  0.1f, 10000.0f);

		renderShader->setBool("sdfDebug", false);
		renderShader->setVec3("camera_pos", camera.Position);
		renderShader->setMat("projection", proj);
		renderShader->setMat("model", glm::mat4(1.0));
		renderShader->setVec2("screen_resolution", fbWidth, fbHeight);
		renderShader->setMat("view", view);
		glm::mat4 invView = glm::inverse(view);
		renderShader->setMat("inverse_view", invView);

		float shaderStart = glfwGetTime();

		// Per shader logic
		if (!debugPlane)
		{
			glEnable(GL_CULL_FACE);
			if (activeShader == shaderNames[0]) // Rasterizer
			{
				renderShader->setInt("sdf", emptySDF);
				renderShader->setFloat("alpha", 1.0f);
				modelMap[currentDemoModel]->Draw(*shaderRaster);
			}
			else
			{
				modelMap[currentDemoModel]->DrawUsingSDF(activeShader, *shaderRaymarchFlat, modelMap["FLATSCREEN"], realtimeSDF);
			}
		}
		else
		{
			// Draw debug plane
			glDisable(GL_CULL_FACE);
			DebugView::InputHandler(window);
			shaderRaster->setMat("model", DebugView::planeTransform(modelMap[currentDemoModel]->bounds));
			shaderRaster->setBool("sdfDebug", true);
			shaderRaster->setInt("textureSize", sdf_conversion::sdfSize);
			shaderRaster->setInt("debugAxis", DebugView::debugPlaneAxis);
			shaderRaster->setFloat("debugHeight", DebugView::getRelativeHeight());
			shaderRaster->setFloat("alpha", 1.0f);
			shaderRaster->setFloat("debugLinesInterval", 50 / largestBoundDimension);

			modelMap[currentDemoModel]->LoadSDFIntoShader(modelMap[currentDemoModel]->methods[activeShader]->sdf, *shaderRaster, true);

			modelMap["FLATSCREEN"]->Draw(*shaderRaster);

			shaderRaster->setMat("model", glm::mat4(1.0));
			shaderRaster->setBool("sdfDebug", false);
			shaderRaster->setFloat("alpha", 0.2f);
			modelMap[currentDemoModel]->Draw(*shaderRaster);
		}

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

		// Current shader display
		SimpleText::drawText(fbWidth, fbHeight, glm::vec4(1.0, 1.0, 1.0, 0.2), activeShader, 20, 10, 30, 0.9);

		// Don't show for rasterizer
		if (activeShader != shaderNames[0])
		{
			if (!debugPlane)
			{
				if (realtimeSDF)
					SimpleText::drawText(fbWidth, fbHeight, glm::vec4(1.0, 1.0, 1.0, 0.2), "REALTIME", 20, fbWidth - 150, 30, 0.9);
				else
					SimpleText::drawText(fbWidth, fbHeight, glm::vec4(1.0, 1.0, 1.0, 0.2), "CACHED", 20, fbWidth - 120, 30, 0.9);
			}
			else
				SimpleText::drawText(fbWidth, fbHeight, glm::vec4(1.0, 1.0, 1.0, 0.2), "DEBUG", 20, fbWidth - 120, 30, 0.9);
		}

		// Current model info
		SimpleText::drawText(fbWidth, fbHeight, glm::vec4(1.0, 1.0, 1.0, 0.2), "MODELNAME: " + currentDemoModel, 10, 10, fbHeight - 20, 0.9);
		SimpleText::drawText(fbWidth, fbHeight, glm::vec4(1.0, 1.0, 1.0, 0.2), "VERTICES: " + readableInt(currentLoadedVerts), 10, 10, fbHeight - 35, 0.9);
		SimpleText::drawText(fbWidth, fbHeight, glm::vec4(1.0, 1.0, 1.0, 0.2), "FACES: " + readableInt(currentLoadedFaces), 10, 10, fbHeight - 50, 0.9);
		SimpleText::drawText(fbWidth, fbHeight, glm::vec4(1.0, 1.0, 1.0, 0.2), "SDF SIZE: " + std::to_string(sdf_conversion::sdfSize), 10, 10, fbHeight - 65, 0.9);

		// Terminal handling
		const int terminalWidth = fbWidth * 0.5;
		const int terminalPadding = fbWidth * 0.25;
		string command = SimpleText::terminal(window, fbWidth, fbHeight, glm::vec4(1.0), glm::vec4(0.0, 0.0, 0.5, 0.5), terminalPadding, fbHeight - 20, terminalWidth, 10);
		if (command == "EXIT")
		{
			glfwSetWindowShouldClose(window, true);
		}
		else if (command.substr(0, 4) == "LOAD")
		{
			if (command.size() > 5)
			{
				string newModel = command.substr(5);
				if (modelMap.find(newModel) != modelMap.end())
				{
					currentDemoModel = newModel;

					float largest = modelMap[newModel]->bounds[1][0] - modelMap[newModel]->bounds[0][0];
					largest = max(modelMap[newModel]->bounds[1][1] - modelMap[newModel]->bounds[0][1], largest);
					largest = max(modelMap[newModel]->bounds[1][2] - modelMap[newModel]->bounds[0][2], largest);
					largestBoundDimension = largest;
					boundsCenter = {
						modelMap[newModel]->bounds[1][0] + modelMap[newModel]->bounds[0][0],
						modelMap[newModel]->bounds[1][1] + modelMap[newModel]->bounds[0][1],
						modelMap[newModel]->bounds[1][2] + modelMap[newModel]->bounds[0][2],
					};
					boundsCenter *= 0.5;
					camera.OrbitDistance = largestBoundDimension * 2;
					camera.YOffset = boundsCenter.y;
					currentLoadedVerts = modelMap[currentDemoModel]->GetModelVertices();
					currentLoadedFaces = modelMap[currentDemoModel]->GetModelFaces();
					terminalResponse = "MODEL LOADED";
				}
				else
				{
					std::cout << "Unknown model name " << newModel << std::endl;
					terminalResponse = "UNKNOWN MODEL NAME";
				}
				terminalResponseTime = currentFrame;
			}
		}
		else if (command == "RECALC")
		{
			if (activeShader == shaderNames[0])
			{
				terminalResponse = "SHADER CANNOT RECALC";
				terminalResponseTime = currentFrame;
			}
			else
			{
				debugPlane = false;
				modelMap[currentDemoModel]->methods[activeShader]->sdf_ready = false;
			}
		}
		else if (command == "REALTIME")
		{
			realtimeSDF = true;
			terminalResponse = "SWITCHED TO REALTIME CALCULATION";
			terminalResponseTime = currentFrame;
			debugPlane = false;
		}
		else if (command == "CACHED")
		{
			realtimeSDF = false;
			terminalResponse = "SWITCHED TO CACHED SDFS";
			terminalResponseTime = currentFrame;
			debugPlane = false;
		}
		else if (command == "DEBUG")
		{
			if (activeShader == shaderNames[0])
			{
				terminalResponse = "NO SDF TO DEBUG";
			}
			else
			{
				realtimeSDF = false;
				terminalResponse = "SWITCHED TO DEBUG VIEW";
				debugPlane = true;
			}
			terminalResponseTime = currentFrame;
		}
		else if (command == "HELP")
		{
			terminalResponse = "AVAILABLE COMMANDS: EXIT LOAD RECALC REALTIME CACHED";
			terminalResponseTime = currentFrame;
		}
		else if (command != "")
		{
			std::cout << "Unknown command " << command << std::endl;
			terminalResponse = "UNKNOWN COMMAND - HELP FOR AVAILABLE COMMANDS";
			terminalResponseTime = currentFrame;
		}

		if (currentFrame - terminalResponseTime < 3.0f)
		{
			SimpleText::drawText(fbWidth, fbHeight, glm::vec4(1.0), terminalResponse, 10, 500, fbHeight - 40, 1);
		}

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	if (benchmark > 0)
	{
		using namespace tinyjson;
		std::cout << "\nBenchmarking mode activated\nCompiling metrics...";
		sdf_conversion::metric::CompileMetricShaders();

		objectnode *root = new objectnode();

		objectnode *sysinfo = new objectnode("system information");
		auto *gpuname = new valuenode<std::string>("gpu name", sysinfo::getGPUName());
		sysinfo->child = gpuname;
		auto *cpuname = new valuenode<std::string>("cpu name", sysinfo::getCPUName());
		gpuname->next = cpuname;
		auto *ramsize = new valuenode<std::string>("ram size", sysinfo::getRAMGB());
		cpuname->next = ramsize;

		objectnode *benchmarkroot = new objectnode("benchmark results");

		root->child = sysinfo;
		sysinfo->next = benchmarkroot;

		double totalTime = 0.0;

		if (benchmark == 1) // Default models
			benchmarking::DefaultIterator(modelMap, shaderNames, benchmarkroot, totalTime);
		else // Thingy10k
		{
			for (auto &[name, model] : modelMap)
			{
				delete model;
			}
			int skipCount = benchmarking::FileIterator(shaderNames, benchmarkroot, window, totalTime, testSize, testStart);
			std::cout << "\nSkipped " << skipCount << " files due to errors." << std::endl;
		}

		std::string output_path = "./output/benchmark_results.json";
		WriteToFile(*root, output_path);
		std::cout << "\nBenchmark results saved to " << output_path << std::endl;
		std::cout << "Finished in " << std::fixed << std::setprecision(0) << totalTime << " seconds" << std::endl;
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
