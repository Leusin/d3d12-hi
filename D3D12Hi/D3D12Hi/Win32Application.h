#pragma once

#include <windows.h>

class DXSample;

/// <summary>
/// 변수와 메서드가 모두 정적(static)으로 선언되어 있기 때문에 
/// 인스턴스를 생성하지 않고도 접근할 수 있다.
/// </summary>
class Win32Application
{
public:
	static int Run(DXSample* pSample, HINSTANCE hInstance, int nCmdShow);
	static HWND GetHwnd() { return m_hwnd; }

protected:
	static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

private:
	static HWND m_hwnd;
};

