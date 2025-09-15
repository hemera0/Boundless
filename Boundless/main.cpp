// Small example of fully bindless rendering with glTF files...

#include "Engine.hpp"

std::shared_ptr<Boundless::Engine> s_Engine;

int main() {
	const auto result = volkInitialize();
	if ( result != VK_SUCCESS )
		return -1;

	if ( !glfwInit() )
		return -1;

	s_Engine = std::make_shared<Boundless::Engine>();
	s_Engine->Create();

	while ( !s_Engine->ShouldExit() ) {
		s_Engine->Tick();

		glfwPollEvents();
	}

	s_Engine->Destroy();
}