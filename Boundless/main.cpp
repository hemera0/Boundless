#include "Pch.hpp"
#include "Engine.hpp"

#include "Input.hpp"

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

std::shared_ptr<Boundless::Engine> s_Engine;

static void MouseCallback( GLFWwindow* window, double xpos, double ypos ) {
	Boundless::g_Input->SetMousePos( glm::vec2{ xpos, ypos } );
}

static void KeyCallback( GLFWwindow* window, int key, int scancode, int action, int mods ) {
	Boundless::g_Input->SetKeyState( key, action );
}

int main() {
	VULKAN_HPP_DEFAULT_DISPATCHER.init( );

	// TODO: Move all this to application class.
	if ( !glfwInit() )
		return -1;

	glfwWindowHint( GLFW_CLIENT_API, GLFW_NO_API );

	GLFWwindow* window = glfwCreateWindow( 1280, 720, "Boundless", nullptr, nullptr );
	if ( !window ) {
		printf( "Failed to create engine window\n" );
		return -1;
	}

	glfwSetInputMode( window, GLFW_CURSOR, GLFW_CURSOR_DISABLED );
	glfwSetCursorPosCallback( window, &MouseCallback );
	glfwSetKeyCallback( window, &KeyCallback );

	s_Engine = std::make_shared<Boundless::Engine>( window );

	static float oldTime = float( glfwGetTime() );

	while ( !s_Engine->ShouldExit() ) {
		float dt = float( glfwGetTime() ) - oldTime;
		oldTime = float( glfwGetTime() );

		s_Engine->Tick(dt);

		glfwPollEvents();
	}
}