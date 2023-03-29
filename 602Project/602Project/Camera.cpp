
#include <iostream>
#include "camera.h"
#include "Window.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include "GLFW/glfw3.h"
#include "GLFW/glfw3native.h"

Camera::Camera() : spin(-80.0), tilt(30), ry(0.57), front(0.1), back(1000.0),
eye({ 2.988857, 1.20160866, -1.58941591 }), updated(false) {
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
    if (ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantCaptureMouse)
        return;

    GLFWwindow* GLFW_window = p_control_window->GetGLFWPointer();
    double x, y;
    glfwGetCursorPos(GLFW_window, &x, &y);

    if (glfwGetMouseButton(GLFW_window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
        update_mouse = true;
    else if (glfwGetMouseButton(GLFW_window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_RELEASE)
        update_mouse = false;

    if (update_mouse) {
        mouseMove(x, y);
    }
    else {
        setMousePosition(x, y);
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

void Camera::DrawGUI() {
    ImGui::Text("Camera eye x: %f  y: %f  z: %f", 
        eye.x, eye.y, eye.z);
    ImGui::Text("Camera Spin: %f  Tilt: %f",
        spin, tilt);
}

void Camera::SetEyePos(glm::vec3 _eye_pos) {
    eye = _eye_pos;
}

void Camera::SetSpin(float _spin) {
    spin = _spin;
}

void Camera::SetTilt(float _tilt) {
    tilt = _tilt;
}

bool Camera::WasUpdated() const {
    return updated;
}
