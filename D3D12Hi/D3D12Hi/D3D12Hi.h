#pragma once

#include <dxgi1_6.h>
#include <D3DCompiler.h>
#include <DirectXMath.h>
// 불러오지 않는다면 추가 포함 디렉터리에 다음 경로 추가
// $(SolutionDir)..\Libraries\D3D12\Include
#include "d3dx12.h"

#include "DXSample.h"

using namespace DirectX;

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
	// GPU 큐에 대기할 최대 프레임 수 이면서 DXGI 스왑 체인의 백 버퍼의 수.
	static const UINT FrameCount = 2;

	struct Vertex
	{
		XMFLOAT3 position;
		XMFLOAT4 color;
	};

	struct SceneConstantBuffer
	{
		XMFLOAT4 offset;
		float padding[60]; // 상수 버퍼가 256바이트 정렬되도록 패딩한다.
	};
	static_assert((sizeof(SceneConstantBuffer) % 256) == 0, "상수 버퍼 크기는 256바이트 정렬이어야 합니다.");

	// 파이프라인 오브젝트.
	CD3DX12_VIEWPORT m_viewport;
	CD3DX12_RECT m_scissorRect;
	ComPtr<IDXGISwapChain3> m_swapChain;
	ComPtr<ID3D12Device> m_device;
	ComPtr<ID3D12Resource> m_renderTargets[FrameCount];
	//ComPtr<ID3D12CommandAllocator> m_commandAllocator;
	ComPtr<ID3D12CommandAllocator> m_commandAllocators[FrameCount]; //*
	ComPtr<ID3D12CommandQueue> m_commandQueue;
	ComPtr<ID3D12RootSignature> m_rootSignature; 
	ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
	ComPtr<ID3D12DescriptorHeap> m_cbvHeap; //*
	ComPtr<ID3D12PipelineState> m_pipelineState;
	ComPtr<ID3D12GraphicsCommandList> m_commandList;
	UINT m_rtvDescriptorSize;

	// 앱 리소스.
	ComPtr<ID3D12Resource> m_vertexBuffer;
	D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
	ComPtr<ID3D12Resource> m_constantBuffer; //*
	SceneConstantBuffer m_constantBufferData; //*
	UINT8* m_pCbvDataBegin; //*

	// 동기화(Synchronization) 오브젝트
	UINT m_frameIndex;
	HANDLE m_fenceEvent;
	ComPtr<ID3D12Fence> m_fence;
	//UINT64 m_fenceValue;
	UINT64 m_fenceValues[FrameCount]; //*

	void LoadPipeline();
	void LoadAssets();
	void PopulateCommandList();
	//void WaitForPreviousFrame();
	void MoveToNextFrame(); //*
	void WaitForGpu(); //*
};

