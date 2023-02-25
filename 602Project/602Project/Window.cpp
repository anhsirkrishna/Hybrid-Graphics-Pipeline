#include "Window.h"

static void glfwErrCallback(int error, const char* desc) {
	fprintf(stderr, "GLFW error %d: %s", error, desc);
}

Window::Window(int _width, int _height, const char* name) noexcept :
        width(_width), height(_height), show_gui(true) {
	glfwSetErrorCallback(glfwErrCallback);

	if ( !glfwInit() ) {
		printf("Could not initialize GLFW.");
		exit(1);
	}

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfw_window = glfwCreateWindow(width, height, name, nullptr, nullptr);

    if (!glfwVulkanSupported()) {
        printf("GLFW: Vulkan Not Supported\n");
        exit(1);
    }

    glfwSetWindowUserPointer(glfw_window, this);
    glfwSetKeyCallback(glfw_window, &KeyboardAction);
    glfwSetCursorPosCallback(glfw_window, &CursorPositionAction);
    glfwSetMouseButtonCallback(glfw_window, &MouseButtonAction);
    glfwSetScrollCallback(glfw_window, &ScrollAction);
    glfwSetFramebufferSizeCallback(glfw_window, &WindowResize);
}

Window::~Window() {
    glfwDestroyWindow(glfw_window);
    glfwTerminate();
}

bool Window::BeginFrame() {
    if (glfwWindowShouldClose(glfw_window))
        return false;

    glfwPollEvents();

    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    if (show_gui) {
        DrawGUI();
    }
        
    return true;
}

GLFWwindow* Window::GetGLFWPointer() {
    return glfw_window;
}

void Window::SetupGraphics() {
    p_gfx = std::make_unique<Graphics>(this, false);
}

void Window::DrawGUI() {
    ImGui::Text("Rate %.3f ms/frame (%.1f FPS)",
        1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
}

void WindowResize(GLFWwindow* window, int w, int h) {

}

void KeyboardAction(GLFWwindow* window, int key, int scancode, int action, int mods) {
}

void MouseButtonAction(GLFWwindow* window, int button, int action, int mods) {
}

void ScrollAction(GLFWwindow* window, double x, double y) {
}

void CursorPositionAction(GLFWwindow* window, double x, double y) {
}
