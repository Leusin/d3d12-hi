#include <windows.h>
#include "D3D12Hi.h"

_Use_decl_annotations_
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
	D3D12Hi sample(1280, 720, L"D3D12 Hi");
	return Win32Application::Run(&sample, hInstance, nCmdShow);
}
