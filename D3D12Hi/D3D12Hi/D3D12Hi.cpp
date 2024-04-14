#include "D3D12Hi.h"

// �ҷ����� �ʴ´ٸ� �߰� ���� ���͸��� ���� ��� �߰�
// $(SolutionDir)..\Libraries\D3D12\Include
#include "d3dx12.h"

D3D12Hi::D3D12Hi(UINT width, UINT height, std::wstring name)
	: DXSample(width, height, name),
	m_frameIndex(0),
	m_rtvDescriptorSize(0)
{
}

void D3D12Hi::OnInit()
{
	LoadPipeline();
	LoadAssets();
}


// ������ ��� ��(frame-based values) ������Ʈ.
void D3D12Hi::OnUpdate()
{
}

// �� ����
void D3D12Hi::OnRender()
{
	// ���� �������ϱ� ���� ��� ���(commands)�� ��� ����Ʈ(commands list)�� ����մϴ�.
	PopulateCommandList();

	// ��� ����Ʈ(commands list) ����.
	ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	// ������ ����(present).
	ThrowIfFailed(m_swapChain->Present(1, 0));

	WaitForPreviousFrame();
}

void D3D12Hi::OnDestroy()
{
	// �Ҹ��ڿ� ���� ������ �ڿ��� GPU�� �� �̻� �������� �ʰ� ��.
	WaitForPreviousFrame();

	CloseHandle(m_fenceEvent);
}

// ������ ������������ �����ϴ� �� �ε�
void D3D12Hi::LoadPipeline()
{
	UINT dxgiFactoryFlags = 0;

#if defined(_DEBUG)
	// ����� ���̾ Ȱ��ȭ
	// ����: ����̽� ���� �Ŀ� ����� ���̾ Ȱ��ȭ�ϸ� Ȱ�� ����̽��� ��ȿȭ �ȴ�.
	{
		ComPtr<ID3D12Debug> debugController;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
		{
			debugController->EnableDebugLayer();

			// �߰� ����� ���̾ Ȱ��ȭ
			dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
		}
	}
#endif

	ComPtr<IDXGIFactory4> factory;
	ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)));

	if (m_useWarpDevice)
	{
		ComPtr<IDXGIAdapter> warpAdapter;
		ThrowIfFailed(factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));

		ThrowIfFailed(D3D12CreateDevice(
			warpAdapter.Get(),
			D3D_FEATURE_LEVEL_11_0,
			IID_PPV_ARGS(&m_device)
		));
	}
	else
	{
		ComPtr<IDXGIAdapter1> hardwareAdapter;
		GetHardwareAdapter(factory.Get(), &hardwareAdapter);

		ThrowIfFailed(D3D12CreateDevice(
			hardwareAdapter.Get(),
			D3D_FEATURE_LEVEL_11_0,
			IID_PPV_ARGS(&m_device)
		));
	}

	// commend queue ���� �� ����
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));

	// swap chain ���� �� ����
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.BufferCount = FrameCount;
	swapChainDesc.Width = m_width;
	swapChainDesc.Height = m_height;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.SampleDesc.Count = 1;

	ComPtr<IDXGISwapChain1> swapChain;
	ThrowIfFailed(factory->CreateSwapChainForHwnd(
		m_commandQueue.Get(),        // ���� ü���� ť�� �ʿ��ϴ�. �׷��� ������ �÷���(force a flush; ���� ó�� ���� �۾��� ��� �Ϸ��ϰ�, �ش� �۾��� �Ϸ�Ǿ����� �����ϴ� ����ȭ �۾�)�� �� �ִ�.
		Win32Application::GetHwnd(),
		&swapChainDesc,
		nullptr,
		nullptr,
		&swapChain
	));

	// ���� ������ Ǯ��ũ�� ��ȯ�� ���������� �ʴ´�.
	ThrowIfFailed(factory->MakeWindowAssociation(Win32Application::GetHwnd(), DXGI_MWA_NO_ALT_ENTER));

	ThrowIfFailed(swapChain.As(&m_swapChain));
	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

	// descriptor heaps ����.
	{
		// ���� Ÿ�� ����(RTV) descriptor heap�� ���� �� ����.
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
		rtvHeapDesc.NumDescriptors = FrameCount;
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		ThrowIfFailed(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));

		m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}

	// frame resources ����.
	{
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());

		// �� ������ RTV ����
		for (UINT n = 0; n < FrameCount; n++)
		{
			ThrowIfFailed(m_swapChain->GetBuffer(n, IID_PPV_ARGS(&m_renderTargets[n])));
			m_device->CreateRenderTargetView(m_renderTargets[n].Get(), nullptr, rtvHandle);
			rtvHandle.Offset(1, m_rtvDescriptorSize);
		}
	}

	ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator)));
}

void D3D12Hi::LoadAssets()
{
	// Ŀ�ǵ� ����(cl) ����
	ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), nullptr, IID_PPV_ARGS(&m_commandList)));

	// Ŀ�ǵ� ����(cl)�� ��� ���¸� ����������, �� ���� ����� �� ����.
	// ���� ������ ���� �ִٰ� �����ϰ� Ŀ��� ������ �ݴ´�.
	ThrowIfFailed(m_commandList->Close());

	// ����ȭ(synchronization) ���� ������Ʈ ����.
	{
		ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
		m_fenceValue = 1;

		// ������ ����ȭ�� ����� �̺�Ʈ �ڵ��� ����.
		m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		if (m_fenceEvent == nullptr)
		{
			ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
		}
	}
}

void D3D12Hi::PopulateCommandList()
{
	// ��� ����Ʈ �Ҵ���(Command list allocators)�� ������ ��� ����Ʈ(cl)�� 
	// GPU���� ������ �Ϸ��� ��쿡�� �缳���� �� �ִ�.
	// ���� GPU ���� ���� ��Ȳ�� �����ϱ� ���� �潺(fences)�� ����ؾ� �մϴ�.
	ThrowIfFailed(m_commandAllocator->Reset());

	// ������, Ư�� ��� ����Ʈ(cl)�� ���� ExecuteCommandList()�� ȣ��Ǹ� 
	// �ش� ��� ����Ʈ�� �������� �缳���� �� ������, ����(re-recording) 
	// �ؾ� �Ѵ�.
	ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), m_pipelineState.Get()));

	// �� ���۰� ���� Ÿ������ ���� ���� ��Ÿ��.
	m_commandList->ResourceBarrier(
		1, 
		&CD3DX12_RESOURCE_BARRIER::Transition(
			m_renderTargets[m_frameIndex].Get(), 
			D3D12_RESOURCE_STATE_PRESENT, 
			D3D12_RESOURCE_STATE_RENDER_TARGET));

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);

	// ��� ���(Record commands).
	const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
	m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

	// ���� �� ���۰� ����(present)�� ���� ���� ��Ÿ��.
	m_commandList->ResourceBarrier(
		1, 
		&CD3DX12_RESOURCE_BARRIER::Transition(
			m_renderTargets[m_frameIndex].Get(), 
			D3D12_RESOURCE_STATE_RENDER_TARGET, 
			D3D12_RESOURCE_STATE_PRESENT));

	ThrowIfFailed(m_commandList->Close());
}

void D3D12Hi::WaitForPreviousFrame()
{
	// �� �ڵ�� �ܼ����� ���� ������ ���� �����Ǿ���. �������� �Ϸ�� ������ GPU�� ������ �۾� �ϷḦ ��ٸ��� ���� 
	// �ּ��� ����� �ƴϴ�.

	// �ñ׳ΰ� �潺 ���� ����.
	const UINT64 fence = m_fenceValue;
	ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), fence));
	m_fenceValue++;

	// ���� �������� �Ϸ�� ������ ��ٸ��ϴ�.
	if (m_fence->GetCompletedValue() < fence)
	{
		ThrowIfFailed(m_fence->SetEventOnCompletion(fence, m_fenceEvent));
		WaitForSingleObject(m_fenceEvent, INFINITE);
	}

	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
}
