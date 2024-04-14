#pragma once


class Win32Application
{
public:
	static HWND GetHwnd() { return m_hwnd; }

private:
	static HWND m_hwnd;
};

