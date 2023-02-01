#include "App.h"

static int const WIDTH = 10 * 128;
static int const HEIGHT = 6 * 128;

int main(int argc, char** argv) {
	App app(WIDTH, HEIGHT);
	
	int return_code = app.Run();
	return return_code;
}