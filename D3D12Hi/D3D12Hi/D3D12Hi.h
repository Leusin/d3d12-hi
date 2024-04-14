#pragma once

#include <dxgi1_4.h>

#include "DXSample.h"

// ComPtr은 CPU 상의 리소스 수명을 관리하는 데 사용되지만,
// GPU 상의 리소스 수명을 이해하지 못한다. 앱은 GPU에서 참조될 수 있는
// 객체를 아직 파괴해서는 안 되므로 GPU 리소스 수명을 고려해야 한다.
using Microsoft::WRL::ComPtr;

class D3D12Hi : public DXSample
{
public:
	D3D12Hi(UINT width, UINT height, std::wstring name);

	void OnInit() override;
	void OnUpdate() override;
	void OnRender() override;
	void OnDestroy() override;

private:
	static const UINT FrameCount = 2;

	// 파이프라인 오브젝트.
	ComPtr<IDXGISwapChain3> m_swapChain;
	ComPtr<ID3D12Device> m_device;
	ComPtr<ID3D12Resource> m_renderTargets[FrameCount];
	ComPtr<ID3D12CommandAllocator> m_commandAllocator;
	ComPtr<ID3D12CommandQueue> m_commandQueue;
	ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
	ComPtr<ID3D12PipelineState> m_pipelineState;
	ComPtr<ID3D12GraphicsCommandList> m_commandList;
	UINT m_rtvDescriptorSize;

	// 동기화(Synchronization) 오브젝트
	UINT m_frameIndex;
	HANDLE m_fenceEvent;
	ComPtr<ID3D12Fence> m_fence;
	UINT64 m_fenceValue;

	void LoadPipeline();
	void LoadAssets();
	void PopulateCommandList();
	void WaitForPreviousFrame();
};

