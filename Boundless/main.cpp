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
	s_Engine->Create();

	static float oldTime = static_cast<float>( glfwGetTime() );

	while ( !s_Engine->ShouldExit() ) {
		float dt = static_cast<float>( glfwGetTime() ) - oldTime;
		oldTime = static_cast<float>( glfwGetTime() );

		s_Engine->Tick(dt);

		glfwPollEvents();
	}

	s_Engine->Destroy();
}