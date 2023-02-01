
#include <iostream>
#include "camera.h"
#include "Window.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include "GLFW/glfw3.h"
#include "GLFW/glfw3native.h"

Camera::Camera() : spin(-20.0), tilt(10.66), ry(0.57), front(0.1), back(1000.0),
eye({ 2.279976, 1.677772, 6.640697 }), updated(false) {
}

glm::mat4 Camera::perspective(const float aspect)
{
    glm::mat4 P;

    float rx = ry * aspect;
    P[0][0] = 1.0 / rx;
    P[1][1] = -1.0 / ry; // Because Vulkan does y upside-down.
    P[2][2] = -back / (back - front);  // Becasue Vulkan wants [front,back] mapped to [0,1]
    P[3][2] = -(front * back) / (back - front);
    P[2][3] = -1;
    P[3][3] = 0;
    return P;
}

glm::mat4 Camera::view()
{
    glm::mat4 SPIN = glm::rotate(spin * 3.14159f / 180.0f, glm::vec3(0.0, 1.0, 0.0));
    glm::mat4 TILT = glm::rotate(tilt * 3.14159f / 180.0f, glm::vec3(1.0, 0.0, 0.0));
    glm::mat4 TRAN = glm::translate(-eye);
    return TILT * SPIN * TRAN;
}

void Camera::HandleKeyInputs(float dt) {
    float dist = 0.7 * dt;
    float rad = 3.14159 / 180;

    vec3 oldEye = eye;

    GLFWwindow* GLFW_window = p_control_window->GetGLFWPointer();

    if (glfwGetKey(GLFW_window, GLFW_KEY_W) == GLFW_PRESS)
        eye += dist * glm::vec3(sin(spin * rad), 0.0, -cos(spin * rad));
    if (glfwGetKey(GLFW_window, GLFW_KEY_S) == GLFW_PRESS)
        eye -= dist * glm::vec3(sin(spin * rad), 0.0, -cos(spin * rad));
    if (glfwGetKey(GLFW_window, GLFW_KEY_A) == GLFW_PRESS)
        eye -= dist * glm::vec3(cos(spin * rad), 0.0, sin(spin * rad));
    if (glfwGetKey(GLFW_window, GLFW_KEY_D) == GLFW_PRESS)
        eye += dist * glm::vec3(cos(spin * rad), 0.0, sin(spin * rad));
    if (glfwGetKey(GLFW_window, GLFW_KEY_SPACE) == GLFW_PRESS)
        eye += dist * glm::vec3(0, -1, 0);
    if (glfwGetKey(GLFW_window, GLFW_KEY_C) == GLFW_PRESS)
        eye += dist * glm::vec3(0, 1, 0);

    if (oldEye != eye)
        updated = true;
}

void Camera::HandleMouseInputs(float dt) {
    GLFWwindow* GLFW_window = p_control_window->GetGLFWPointer();

    if (glfwGetMouseButton(GLFW_window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
        double x, y;
        glfwGetCursorPos(GLFW_window, &x, &y);
        mouseMove(x, y);
    }
}

void Camera::mouseMove(const float x, const float y)
{
    float dx = x - posx;
    float dy = y - posy;
    spin += dx / 3;
    tilt += dy / 3;
    posx = x;
    posy = y;
    updated = true;
}

void Camera::setMousePosition(const float x, const float y)
{
    posx = x;
    posy = y;
}

void Camera::wheel(const int dir)
{
    printf("wheel: %d\n", dir);
}

void Camera::Update(float dt) {
    updated = false;

    HandleKeyInputs(dt);
    HandleMouseInputs(dt);
}

void Camera::SetControlWindow(Window* _p_window) {
    p_control_window = _p_window;
}
