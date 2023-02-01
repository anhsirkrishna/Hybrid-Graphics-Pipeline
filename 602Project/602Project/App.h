#pragma once
#include "Window.h"
#include "Camera.h"
#include "TimerWrap.h"

class App
{
public:
	App(int window_width, int window_height);
	int Run();
private:
	Camera cam;
	TimerWrap timer;
	float speed_factor = 1.0f;
	Window window;
	void Update();
};

