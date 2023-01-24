#pragma once
#include "Window.h"

class App
{
public:
	App();
	int Run();
private:
	Window window;
	void Update();
};

