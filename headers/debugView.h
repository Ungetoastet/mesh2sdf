namespace DebugView
{
    int debugPlaneHeight = 0;
    int debugPlaneAxis = 0;

    bool flipLastPressed = false;

    /// @brief Returns the debug height in between 0 and 1
    /// @return
    float getRelativeHeight()
    {
        return ((debugPlaneHeight + 0.5f) / (float)sdf_conversion::sdfSize);
    }

    void InputHandler(GLFWwindow *window)
    {
        if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
        {
            debugPlaneHeight = ((debugPlaneHeight - 1) + sdf_conversion::sdfSize) % sdf_conversion::sdfSize;
        }

        if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
        {
            debugPlaneHeight = (debugPlaneHeight + 1) % sdf_conversion::sdfSize;
        }

        if (glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS)
        {
            if (flipLastPressed == false)
            {
                debugPlaneAxis = (debugPlaneAxis + 1) % 3;
            }
            flipLastPressed = true;
        }
        else
        {
            flipLastPressed = false;
        }
    }

    glm::mat4 planeTransform(glm::mat2x3 aabb)
    {
        glm::vec3 min = aabb[0];
        glm::vec3 max = aabb[1];

        float pos = glm::mix(min[debugPlaneAxis], max[debugPlaneAxis], getRelativeHeight());

        glm::vec3 center = (min + max) * 0.5f;
        center[debugPlaneAxis] = pos;

        glm::vec3 scale(1.0f);
        if (debugPlaneAxis == 0)
            scale = glm::vec3(1.0f, max.y - min.y, max.z - min.z);
        if (debugPlaneAxis == 1)
            scale = glm::vec3(max.x - min.x, 1.0f, max.z - min.z);
        if (debugPlaneAxis == 2)
            scale = glm::vec3(max.x - min.x, max.y - min.y, 1.0f);

        glm::mat4 model(1.0f);
        model = glm::translate(model, center);

        model = glm::scale(model, scale);

        model = glm::scale(model, glm::vec3(0.5));

        if (debugPlaneAxis == 0)
            model = glm::rotate(model, glm::half_pi<float>(), glm::vec3(0, 1, 0));
        if (debugPlaneAxis == 1)
            model = glm::rotate(model, -glm::half_pi<float>(), glm::vec3(1, 0, 0));
        if (debugPlaneAxis == 2)
            model = glm::rotate(model, -glm::half_pi<float>(), glm::vec3(0, 0, 1));

        return model;
    }
}