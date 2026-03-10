#include "FluidSystem.h"

ssfr::FluidSystem* fluidSystem;

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)						
{
	if (fluidSystem != NULL)																		
	{
		fluidSystem->handleMessages(hWnd, uMsg, wParam, lParam);									
	}
	return (DefWindowProc(hWnd, uMsg, wParam, lParam));
}

int APIENTRY WinMain(_In_ HINSTANCE hInstance, _In_opt_  HINSTANCE hPrevInstance, _In_ LPSTR, _In_ int)
{
	fluidSystem = new ssfr::FluidSystem();
	fluidSystem->initVulkan();
	fluidSystem->setupWindow(hInstance, WndProc);													
	fluidSystem->prepare();
	fluidSystem->renderLoop();
    
	delete fluidSystem;
	return 0;
}