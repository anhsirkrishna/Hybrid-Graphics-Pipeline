#include "Window.h"

static void glfwErrCallback(int error, const char* desc) {
	fprintf(stderr, "GLFW error %d: %s", error, desc);
}

Window::Window(int _width, int _height, const char* name) noexcept {
	glfwSetErrorCallback(glfwErrCallback);

	if ( !glfwInit() ) {
		printf("Could not initialize GLFW.");
		exit(1);
	}

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFW_window = glfwCreateWindow(WIDTH, HEIGHT, PROJECT.c_str(), nullptr, nullptr);

    if (!glfwVulkanSupported()) {
        printf("GLFW: Vulkan Not Supported\n");
        exit(1);
    }

    glfwSetWindowUserPointer(GLFW_window, this);
    glfwSetKeyCallback(GLFW_window, &KeyboardAction);
    glfwSetCursorPosCallback(GLFW_window, &CursorPositionAction);
    glfwSetMouseButtonCallback(GLFW_window, &mousebutton_cb);
    glfwSetScrollCallback(GLFW_window, &scroll_cb);
    glfwSetFramebufferSizeCallback(GLFW_window, &framebuffersize_cb);

}
