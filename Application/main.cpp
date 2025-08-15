#include <iostream>
#include <chrono>

#include "application.h"

using namespace Byte;

extern "C" {
	__declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

int main() {
	Application app;
	app.initialize(1280, 720, "Byte Engine");

	app.run();

	return 0;
}