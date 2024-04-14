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

	// 이벤트 핸들러
	virtual void OnKeyDown(UINT8 /*key*/) {}
	virtual void OnKeyUp(UINT8 /*key*/) {}

	// 접근자
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

	// 뷰 포트 차원.
	UINT m_width;
	UINT m_height;
	float m_aspectRatio;

	// 어뎁터 정보.
	bool m_useWarpDevice;

private:
	// 에셋의 기본 경로.
	std::wstring m_assetsPath;

	// 윈도우 제목.
	std::wstring m_title;
};

