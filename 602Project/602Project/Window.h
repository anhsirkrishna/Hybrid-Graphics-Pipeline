#pragma once

#include <memory>

#define GLFW_EXPOSE_NATIVE_WIN32
#include "GLFW/glfw3.h"
#include "GLFW/glfw3native.h"

#include "Graphics.h"

class Window
{
private:
	int width;
	int height;
	GLFWwindow* glfw_window;
	std::unique_ptr<Graphics> p_gfx;

public:
	Window(int _width, int _height, const char* name) noexcept;
	~Window();
};

//Input handling callbacks
void WindowResize(GLFWwindow* window, int w, int h);
void KeyboardAction(GLFWwindow* window, int key, int scancode, int action, int mods);
void MouseButtonAction(GLFWwindow* window, int button, int action, int mods);
void CursorPositionAction(GLFWwindow* window, double x, double y);
