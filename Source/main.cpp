#include <Windows.h>
#include "Engine.h"

#define BAMBOO_TEST_GRAPHICS_API 1

#if BAMBOO_TEST_GRAPHICS_API

#include "GraphicsAPI.h"
#include "NativeWindow.h"

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	bamboo::win32::NativeWindow win(L"Bamboo", 800, 600);

	bamboo::GraphicsAPI* api = bamboo::InitGraphicsAPI(bamboo::Direct3D11, win.GetHandle());

	if (nullptr == api)
		return -1;

	while (win.ProcessEvent())
	{

	}

	return 0;
}

#elif BAMBOO_TEST_RENDERER

#include "Renderer.h"

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	bamboo::Engine engine;

	engine.Init();

	return engine.Run();
}

#else

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	bamboo::Engine engine;

	engine.Init();

	return engine.Run();
}

#endif