#pragma once

#include <dxgi1_4.h>

#include "DXSample.h"

// ComPtr�� CPU ���� ���ҽ� ������ �����ϴ� �� ��������,
// GPU ���� ���ҽ� ������ �������� ���Ѵ�. ���� GPU���� ������ �� �ִ�
// ��ü�� ���� �ı��ؼ��� �� �ǹǷ� GPU ���ҽ� ������ �����ؾ� �Ѵ�.
using Microsoft::WRL::ComPtr;

class D3d12Hi : public DXSample
{
public:
	D3d12Hi(UINT width, UINT height, std::wstring name);

	void OnInit() override;
	void OnUpdate() override;
	void OnRender() override;
	void OnDestroy() override;

private:
	static const UINT FrameCount = 2;

	// ���������� ������Ʈ.
	ComPtr<IDXGISwapChain3> m_swapChain;
	ComPtr<ID3D12Device> m_device;
	ComPtr<ID3D12Resource> m_renderTargets[FrameCount];
	ComPtr<ID3D12CommandAllocator> m_commandAllocator;
	ComPtr<ID3D12CommandQueue> m_commandQueue;
	ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
	ComPtr<ID3D12PipelineState> m_pipelineState;
	ComPtr<ID3D12GraphicsCommandList> m_commandList;
	UINT m_rtvDescriptorSize;

	// ����ȭ(Synchronization) ������Ʈ
	UINT m_frameIndex;
	HANDLE m_fenceEvent;
	ComPtr<ID3D12Fence> m_fence;
	UINT64 m_fenceValue;

	void LoadPipeline();
	void LoadAssets();
	void PopulateCommandList();
	void WaitForPreviousFrame();
};
