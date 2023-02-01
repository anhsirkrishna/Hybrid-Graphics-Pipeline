#include "App.h"

App::App(int window_width, int window_height) :
	window(window_width, window_height, "Graphics Framework") {
	window.SetupGraphics();
	window.Gfx().SetActiveCamPtr(&cam);
	cam.SetControlWindow(&window);
	timer.Reset();
}

int App::Run() {
	while (window.BeginFrame()) {
		Update();
	}

	window.Gfx().Teardown();
	return 1;
}

void App::Update() {
	const auto dt = timer.Mark() * speed_factor;
	cam.Update(dt);
	window.Gfx().DrawFrame();
}
