#pragma once

#include <windows.h>

class DXSample;

/// <summary>
/// ������ �޼��尡 ��� ����(static)���� ����Ǿ� �ֱ� ������ 
/// �ν��Ͻ��� �������� �ʰ� ������ �� �ִ�.
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

