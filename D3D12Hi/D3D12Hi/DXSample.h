#pragma once

#include <dxgi.h>

#include "DXSampleHelper.h"
#include "Win32Application.h"

class DXSample
{
public:
	DXSample(UINT width, UINT height, std::wstring name);
	virtual ~DXSample();

	virtual void OnInit() abstract;
	virtual void OnUpdate() abstract;
	virtual void OnRender() abstract;
	virtual void OnDestroy() abstract;

	// �̺�Ʈ �ڵ鷯
	virtual void OnKeyDown(UINT8 /*key*/) {}
	virtual void OnKeyUp(UINT8 /*key*/) {}

	// ������
	UINT GetWidth() const { return m_width; }
	UINT GetHeight() const { return m_height; }
	const WCHAR* GetTitle() const { return m_title.c_str(); }

	void ParseCommandLineArgs(_In_reads_(argc) WCHAR* argv[], int argc);

protected:
	std::wstring GetAssetFullPath(LPCWSTR assetName);

	void GetHardwareAdapter(
		_In_ IDXGIFactory1* pFactory,
		_Outptr_result_maybenull_ IDXGIAdapter1** ppAdapter,
		bool requestHighPerformanceAdapter = false);

	void SetCustomWindowText(LPCWSTR text);

	// �� ��Ʈ ����.
	UINT m_width;
	UINT m_height;
	float m_aspectRatio;

	// ��� ����.
	bool m_useWarpDevice;

private:
	// ������ �⺻ ���.
	std::wstring m_assetsPath;

	// ������ ����.
	std::wstring m_title;
};

