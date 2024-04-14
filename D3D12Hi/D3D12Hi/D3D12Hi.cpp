#include "D3D12Hi.h"

// 불러오지 않는다면 추가 포함 디렉터리에 다음 경로 추가
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


// 프레임 기반 값(frame-based values) 업데이트.
void D3D12Hi::OnUpdate()
{
}

// 씬 랜더
void D3D12Hi::OnRender()
{
	// 씬을 렌더링하기 위한 모든 명령(commands)을 명령 리스트(commands list)에 기록합니다.
	PopulateCommandList();

	// 명령 리스트(commands list) 실행.
	ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	// 프레임 제시(present).
	ThrowIfFailed(m_swapChain->Present(1, 0));

	WaitForPreviousFrame();
}

void D3D12Hi::OnDestroy()
{
	// 소멸자에 의해 정리될 자원에 GPU가 더 이상 참조하지 않게 함.
	WaitForPreviousFrame();

	CloseHandle(m_fenceEvent);
}

// 랜더링 파이프라인이 의존하는 것 로드
void D3D12Hi::LoadPipeline()
{
	UINT dxgiFactoryFlags = 0;

#if defined(_DEBUG)
	// 디버그 레이어를 활성화
	// 참고: 디바이스 생성 후에 디버그 레이어를 활성화하면 활성 디바이스가 무효화 된다.
	{
		ComPtr<ID3D12Debug> debugController;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
		{
			debugController->EnableDebugLayer();

			// 추가 디버그 레이어를 활성화
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

	// commend queue 서술 및 생성
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));

	// swap chain 서술 및 생성
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
		m_commandQueue.Get(),        // 스왑 체인은 큐를 필요하다. 그래야 강제로 플러시(force a flush; 현재 처리 중인 작업을 즉시 완료하고, 해당 작업이 완료되었음을 보장하는 동기화 작업)할 수 있다.
		Win32Application::GetHwnd(),
		&swapChainDesc,
		nullptr,
		nullptr,
		&swapChain
	));

	// 현재 샘플은 풀스크린 변환을 지원해주지 않는다.
	ThrowIfFailed(factory->MakeWindowAssociation(Win32Application::GetHwnd(), DXGI_MWA_NO_ALT_ENTER));

	ThrowIfFailed(swapChain.As(&m_swapChain));
	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

	// descriptor heaps 생성.
	{
		// 랜더 타깃 뷰의(RTV) descriptor heap을 서술 및 생성.
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
		rtvHeapDesc.NumDescriptors = FrameCount;
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		ThrowIfFailed(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));

		m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}

	// frame resources 생성.
	{
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());

		// 매 프레임 RTV 생성
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
	// 커맨드 라인(cl) 생성
	ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), nullptr, IID_PPV_ARGS(&m_commandList)));

	// 커맨드 라인(cl)은 기록 상태를 생상하지만, 이 아직 기록할 게 없다.
	// 메인 루프는 닫혀 있다고 예상하고 커멘드 라인을 닫는다.
	ThrowIfFailed(m_commandList->Close());

	// 동기화(synchronization) 관련 오브젝트 생성.
	{
		ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
		m_fenceValue = 1;

		// 프레임 동기화에 사용할 이벤트 핸들을 생성.
		m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		if (m_fenceEvent == nullptr)
		{
			ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
		}
	}
}

void D3D12Hi::PopulateCommandList()
{
	// 명령 리스트 할당자(Command list allocators)는 연관된 명령 리스트(cl)가 
	// GPU에서 실행을 완료한 경우에만 재설정될 수 있다.
	// 앱은 GPU 실행 진행 상황을 결정하기 위해 펜스(fences)를 사용해야 합니다.
	ThrowIfFailed(m_commandAllocator->Reset());

	// 하지만, 특정 명령 리스트(cl)에 대해 ExecuteCommandList()가 호출되면 
	// 해당 명령 리스트는 언제든지 재설정할 수 있으며, 재기록(re-recording) 
	// 해야 한다.
	ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), m_pipelineState.Get()));

	// 백 버퍼가 렌더 타겟으로 사용될 것을 나타냄.
	m_commandList->ResourceBarrier(
		1, 
		&CD3DX12_RESOURCE_BARRIER::Transition(
			m_renderTargets[m_frameIndex].Get(), 
			D3D12_RESOURCE_STATE_PRESENT, 
			D3D12_RESOURCE_STATE_RENDER_TARGET));

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);

	// 명령 기록(Record commands).
	const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
	m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

	// 이제 백 버퍼가 제시(present)에 사용될 것을 나타냄.
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
	// 이 코드는 단순성을 위해 다음과 같이 구현되었다. 프레임이 완료될 때까지 GPU의 렌더링 작업 완료를 기다리는 것이 
	// 최선의 방법이 아니다.

	// 시그널과 펜스 값을 증가.
	const UINT64 fence = m_fenceValue;
	ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), fence));
	m_fenceValue++;

	// 이전 프레임이 완료될 때까지 기다립니다.
	if (m_fence->GetCompletedValue() < fence)
	{
		ThrowIfFailed(m_fence->SetEventOnCompletion(fence, m_fenceEvent));
		WaitForSingleObject(m_fenceEvent, INFINITE);
	}

	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
}
