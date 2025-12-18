#include "Platform/Windows/WindowGLFW.h"
#include "Volk/volk.h"
#include <GLFW/glfw3.h>

#include <glm/gtc/matrix_transform.hpp>

namespace imp
{
    bool WindowGLFW::Initialize(const WindowInitParams& params)
    {
        if (glfwInit() != GLFW_TRUE)
            return false;

        glfwInitVulkanLoader(vkGetInstanceProcAddr);

        if (glfwVulkanSupported() != GLFW_TRUE)
            return false;

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        m_Window = glfwCreateWindow(params.width, params.height, "Imperial Engine 3", NULL, NULL);

        return true;
    }
    bool WindowGLFW::Shutdown(VkInstance instance, VkDevice device)
    {
        return true;
    }

    VkResult WindowGLFW::CreateWindowSurface(VkInstance instance)
    {
        return glfwCreateWindowSurface(instance, m_Window, NULL, &m_Surface);;
    }
    
    void WindowGLFW::UpdateInfo(double frameTimeMs)
    {
        glfwPollEvents();
        char titleBuffer[256];
        snprintf(titleBuffer, sizeof(titleBuffer), "Imperial Engine 3 - %.2f ms (%.0f FPS)", 
                 frameTimeMs, 1000.0 / frameTimeMs);
        glfwSetWindowTitle(m_Window, titleBuffer);
    }
    
    void WindowGLFW::MoveCamera(glm::mat4& transform, float delta)
    {
        static constexpr float translationSpeed = 10.0f;
        
        // Forward
        int wKeyState = glfwGetKey(m_Window, GLFW_KEY_W);
        if (wKeyState == GLFW_PRESS)
        {
            glm::vec3 forward(0.0f, 0.0f, -1.0f);
            glm::vec3 translation = forward * delta * translationSpeed;
            transform = glm::translate(transform, translation);
        }

        // Back
        int sKeyState = glfwGetKey(m_Window, GLFW_KEY_S);
        if (sKeyState == GLFW_PRESS)
        {
            glm::vec3 backward(0.0f, 0.0f, 1.0f);
            glm::vec3 translation = backward * delta * translationSpeed;
            transform = glm::translate(transform, translation);
        }

        // LEft
        int aKeyState = glfwGetKey(m_Window, GLFW_KEY_A);
        if (aKeyState == GLFW_PRESS)
        {
            glm::vec3 left(1.0f, 0.0f, 0.0f);
            glm::vec3 translation = left * delta * translationSpeed;
            transform = glm::translate(transform, translation);
        }

        // Right
        int dKeyState = glfwGetKey(m_Window, GLFW_KEY_D);
        if (dKeyState == GLFW_PRESS)
        {
            glm::vec3 right(-1.0f, 0.0f, 0.0f);
            glm::vec3 translation = right * delta * translationSpeed;
            transform = glm::translate(transform, translation);
        }


        // Up
        int spaceKeyState = glfwGetKey(m_Window, GLFW_KEY_SPACE);
        if (spaceKeyState == GLFW_PRESS)
        {
            glm::vec3 up(0.0f, 1.0f, 0.0f);
            glm::vec3 translation = up * delta * translationSpeed;
            transform = glm::translate(transform, translation);
        }

        // Down
        int shiftKeyState = glfwGetKey(m_Window, GLFW_KEY_LEFT_SHIFT);
        if (shiftKeyState == GLFW_PRESS)
        {
            glm::vec3 up(0.0f, -1.0f, 0.0f);
            glm::vec3 translation = up * delta * translationSpeed;
            transform = glm::translate(transform, translation);
        }

        // Rotate left
        int leftKeyState = glfwGetKey(m_Window, GLFW_KEY_LEFT);
        if (leftKeyState == GLFW_PRESS)
        {
            transform = glm::rotate(transform, glm::radians(-90.0f * delta), glm::vec3(0.0f, 1.0f, 0.0f));
        }

        // Rotate right
        int rightKeyState = glfwGetKey(m_Window, GLFW_KEY_RIGHT);
        if (rightKeyState == GLFW_PRESS)
        {
            transform = glm::rotate(transform, glm::radians(90.0f * delta), glm::vec3(0.0f, 1.0f, 0.0f));
        }

        // Rotate up
        int upKeyState = glfwGetKey(m_Window, GLFW_KEY_UP);
        if (upKeyState == GLFW_PRESS)
        {
            transform = glm::rotate(transform, glm::radians(-90.0f * delta), glm::vec3(1.0f, 0.0f, 0.0f));
        }

        // Rotate down
        int downKeyState = glfwGetKey(m_Window, GLFW_KEY_DOWN);
        if (downKeyState == GLFW_PRESS)
        {
            transform = glm::rotate(transform, glm::radians(90.0f * delta), glm::vec3(1.0f, 0.0f, 0.0f));
        }

        // Reset Camera Rotation
        int rKeyState = glfwGetKey(m_Window, GLFW_KEY_R);
        if (rKeyState == GLFW_PRESS)
        {
            transform[0] = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
            transform[1] = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
            transform[2] = glm::vec4(0.0f, 0.0f, 1.0f, 0.0f);
        }
    }
        

    bool WindowGLFW::ShouldClose() const
    {
        return glfwWindowShouldClose(m_Window);
    }

    uint32_t WindowGLFW::GetWidth() const
    {
        int width = 0;
        glfwGetWindowSize(m_Window, &width, nullptr);
        return static_cast<uint32_t>(width);
    }

    uint32_t WindowGLFW::GetHeight() const
    {
        int height = 0;
        glfwGetWindowSize(m_Window, nullptr, &height);
        return static_cast<uint32_t>(height);
    }

    const char* const* WindowGLFW::GetRequiredInstanceExtensions(uint32_t* count) const
    {
        return glfwGetRequiredInstanceExtensions(count);
    }
    const char* const* WindowGLFW::GetRequiredDeviceExtensions(uint32_t* count) const
    {
        *count = kRequiredDeviceExtensions.size();
        return kRequiredDeviceExtensions.data();
    }
}