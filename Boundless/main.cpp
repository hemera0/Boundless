#include "Pch.hpp"
#include "Engine.hpp"

std::shared_ptr<Boundless::Engine> s_Engine;

int main() {
	const auto result = volkInitialize();
	if ( result != VK_SUCCESS )
		return -1;

	if ( !glfwInit() )
		return -1;

	s_Engine = std::make_shared<Boundless::Engine>();

	static float oldTime = float( glfwGetTime() );

	while ( !s_Engine->ShouldExit() ) {
		float dt = float( glfwGetTime() ) - oldTime;
		oldTime = float( glfwGetTime() );

		s_Engine->Tick(dt);

		glfwPollEvents();
	}
}