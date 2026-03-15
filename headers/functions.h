#ifndef FUNCTIONS
#define FUNCTIONS
#define STB_IMAGE_IMPLEMENTATION
#include <glad/glad.h> // include glad to get all the required OpenGL headers
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>
#include "shader.h"
#include "stb_image.h"

float lastX;
float lastY;
bool firstMouse = true;

glm::vec2 mouse_offset(GLFWwindow *window)
{
    double xpos, ypos;
    glfwGetCursorPos(window, &xpos, &ypos);
    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top

    lastX = xpos;
    lastY = ypos;

    return glm::vec2(xoffset, yoffset);
}

std::string readableInt(unsigned int n)
{
    std::string s = std::to_string(n);

    int insertPos = static_cast<int>(s.length()) - 3;
    while (insertPos > 0)
    {
        s.insert(insertPos, ".");
        insertPos -= 3;
    }

    return s;
}

namespace sysinfo
{
    std::string getCPUName()
    {
        std::ifstream file("/proc/cpuinfo");
        std::string line;

        while (std::getline(file, line))
        {
            if (line.rfind("model name", 0) == 0)
            {
                return line.substr(line.find(":") + 2);
            }
        }
        return {};
    }
    std::string getRAMGB()
    {
        std::ifstream file("/proc/meminfo");
        std::string line;

        while (std::getline(file, line))
        {
            if (line.rfind("MemTotal", 0) == 0)
            {
                long kb = std::stol(line.substr(line.find(":") + 1));
                double gb = kb / 1e6;
                std::ostringstream oss;
                oss << std::fixed << std::setprecision(1) << gb << " GB";
                return oss.str();
            }
        }
        return {};
    }
    std::string getGPUName()
    {
        std::string gpu = reinterpret_cast<const char *>(glGetString(GL_RENDERER));
        return gpu.substr(0, gpu.find(" ("));
    }
}

#endif