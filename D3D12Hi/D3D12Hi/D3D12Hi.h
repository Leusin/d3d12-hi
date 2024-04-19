#pragma once

#include <dxgi1_6.h>
#include <D3DCompiler.h>
#include <DirectXMath.h>
// �ҷ����� �ʴ´ٸ� �߰� ���� ���͸��� ���� ��� �߰�
// $(SolutionDir)..\Libraries\D3D12\Include
#include "d3dx12.h"

#include "DXSample.h"

using namespace DirectX;

// ComPtr�� CPU ���� ���ҽ� ������ �����ϴ� �� ��������,
// GPU ���� ���ҽ� ������ �������� ���Ѵ�. ���� GPU���� ������ �� �ִ�
// ��ü�� ���� �ı��ؼ��� �� �ǹǷ� GPU ���ҽ� ������ ����ؾ� �Ѵ�.
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
	static const UINT TextureWidth = 256;
	static const UINT TextureHeight = 256;
	static const UINT TexturePixelSize = 4; // �ؽ�ó�� �ȼ��� ���� ����Ʈ ��


	struct Vertex
	{
		XMFLOAT3 position;
		XMFLOAT2 uv; //*XMFLOAT4 color;
	};

	// ���������� ������Ʈ.
	CD3DX12_VIEWPORT m_viewport;
	CD3DX12_RECT m_scissorRect;
	ComPtr<IDXGISwapChain3> m_swapChain;
	ComPtr<ID3D12Device> m_device;
	ComPtr<ID3D12Resource> m_renderTargets[FrameCount];
	ComPtr<ID3D12CommandAllocator> m_commandAllocator;
	ComPtr<ID3D12CommandAllocator> m_bundleAllocator; //*
	ComPtr<ID3D12CommandQueue> m_commandQueue;
	ComPtr<ID3D12RootSignature> m_rootSignature; 
	ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
	ComPtr<ID3D12DescriptorHeap> m_srvHeap;
	ComPtr<ID3D12PipelineState> m_pipelineState;
	ComPtr<ID3D12GraphicsCommandList> m_commandList;
	ComPtr<ID3D12GraphicsCommandList> m_bundle; //*
	UINT m_rtvDescriptorSize;

	// �� ���ҽ�.
	ComPtr<ID3D12Resource> m_vertexBuffer;
	D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
	ComPtr<ID3D12Resource> m_texture;

	// ����ȭ(Synchronization) ������Ʈ
	UINT m_frameIndex;
	HANDLE m_fenceEvent;
	ComPtr<ID3D12Fence> m_fence;
	UINT64 m_fenceValue;

	void LoadPipeline();
	void LoadAssets();
	std::vector<UINT8> GenerateTextureData();
	void PopulateCommandList();
	void WaitForPreviousFrame();
};

