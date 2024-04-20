#include "D3D12Hi.h"

D3D12Hi::D3D12Hi(UINT width, UINT height, std::wstring name)
	: DXSample(width, height, name)
	, m_frameIndex(0)
	, m_viewport(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)) // *
	, m_scissorRect(0, 0, static_cast<LONG>(width), static_cast<LONG>(height)) // *
	, m_fenceValues{}
	, m_rtvDescriptorSize(0)
	, m_constantBufferData{}
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
	const float translationSpeed = 0.005f;
	const float offsetBounds = 1.25f;

	m_constantBufferData.offset.x += translationSpeed;
	if (m_constantBufferData.offset.x > offsetBounds)
	{
		m_constantBufferData.offset.x = -offsetBounds;
	}
	memcpy(m_pCbvDataBegin, &m_constantBufferData, sizeof(m_constantBufferData));
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

	//WaitForPreviousFrame();
	MoveToNextFrame();
}

void D3D12Hi::OnDestroy()
{
	// 소멸자에 의해 정리될 자원에 GPU가 더 이상 참조하지 않게 함.
	//WaitForPreviousFrame();
	WaitForGpu();

	CloseHandle(m_fenceEvent);
}

// 랜더링 파이프라인이 의존하는 것 로드.
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
		m_commandQueue.Get(), // 스왑 체인은 큐를 필요하다. 그래야 강제로 플러시(force a flush; 현재 처리 중인 작업을 즉시 완료하고, 해당 작업이 완료되었음을 보장하는 동기화 작업)할 수 있다.
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
		// 랜더 타깃 뷰의 (RTV) descriptor heap을 서술 및 생성.
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
		rtvHeapDesc.NumDescriptors = FrameCount;
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		ThrowIfFailed(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));

		m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		// 상수 버퍼 뷰 (CBV) descriptor heap을 서술 및 생성.
		// 플래그는 descriptor heap 이 파이프라인에 바인딩 될 수 있고
		// 그 안에 포함된 descriptor가 루트 테이블에서 참조 될 수 있음을 나타낸다.
		D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc = {};
		cbvHeapDesc.NumDescriptors = 1;
		cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		ThrowIfFailed(m_device->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&m_cbvHeap)));
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
			
			ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocators[n])));
		}
	}
}

void D3D12Hi::LoadAssets()
{
	// 하나의 CBV descriptor table 로 구성된 루트 시그니처 생성. 
	{
		D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};

		// 해당 앱이 지원하는 가장 높은 버전. 
		// CheckFeatureSupport가 반환하는 버전은 HighestVersion 보다 높으면 안된다.
		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

		if (FAILED(m_device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
		{
			featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
		}

		CD3DX12_DESCRIPTOR_RANGE1 ranges[1];
		CD3DX12_ROOT_PARAMETER1 rootParameters[1];

		ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
		rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_VERTEX);

		// 특정 파이프라인 단계에 대해 불필요한 접근를 거부한다.
		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
		rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, 0, nullptr, rootSignatureFlags);

		ComPtr<ID3DBlob> signature;
		ComPtr<ID3DBlob> error;
		ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &signature, &error));
		ThrowIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
	}

	// 셰이더를 컴파일하고 로드하는 파이프라인 상태(state) 생성.
	{
		ComPtr<ID3DBlob> vertexShader;
		ComPtr<ID3DBlob> pixelShader;

#if defined(_DEBUG)
		// 그래픽스 디버깅 도구 활성화 플래그.
		UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
		UINT compileFlags = 0;
#endif

		ThrowIfFailed(D3DCompileFromFile(GetAssetFullPath(L"shaders.hlsl").c_str(), nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, nullptr));
		ThrowIfFailed(D3DCompileFromFile(GetAssetFullPath(L"shaders.hlsl").c_str(), nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, nullptr));

		// 버텍스 인풋 레이아웃(vertex input layout) 정의.
		D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};

		// 그래픽 파이프라인 상태 객체 (PSO) 서술 및 생성.
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
		psoDesc.pRootSignature = m_rootSignature.Get();
		psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
		psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psoDesc.DepthStencilState.DepthEnable = FALSE;
		psoDesc.DepthStencilState.StencilEnable = FALSE;
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.SampleDesc.Count = 1;

		ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)));
	}

	// 커맨드 라인(cl) 생성
	ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocators[m_frameIndex].Get(), m_pipelineState.Get(), IID_PPV_ARGS(&m_commandList)));

	ThrowIfFailed(m_commandList->Close());

	// 버텍스 버퍼 생성.
	{
		// 삼각형을 위한 지오메트리 정의.
		Vertex triangleVertices[] =
		{
			{ { 0.0f, 0.25f * m_aspectRatio, 0.0f }, { 0.867f, 0.341f, 0.275f, 1.0f } },
			{ { 0.25f, -0.25f * m_aspectRatio, 0.0f }, { 1.0f, 0.769f, 0.439f, 1.0f } },
			{ { -0.25f, -0.25f * m_aspectRatio, 0.0f }, { 0.545f, 0.196f, 0.173f, 1.0f } }
		};

		const UINT vertexBufferSize = sizeof(triangleVertices);

		// 참고: 정적 데이터를 전달하기 위해 업로드 힙을 사용하는 것은 권장되지 
		// 않음. 이 코드에서는 실제로 전송해야 할 버텍스가 매우 적고 코드가 
		// 간단하기 때문에 업로드 힙을 사용한다.
		ThrowIfFailed(m_device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&m_vertexBuffer)));

		// 삼각형 데이터를 버텍스 버퍼로 복사.
		UINT8* pVertexDataBegin;
		CD3DX12_RANGE readRange(0, 0); // CPU에서 이 리소스를 읽지 않음.
		ThrowIfFailed(m_vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
		memcpy(pVertexDataBegin, triangleVertices, sizeof(triangleVertices));
		m_vertexBuffer->Unmap(0, nullptr);

		// 버텍스 버퍼 뷰 초기화.
		m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
		m_vertexBufferView.StrideInBytes = sizeof(Vertex);
		m_vertexBufferView.SizeInBytes = vertexBufferSize;
	}

	// 상수 버퍼 생성.
	{
		const UINT constantBufferSize = sizeof(SceneConstantBuffer);    // 상수 버퍼 (CB)의 크기는 256-바이트 정렬을 필요로한다.

		ThrowIfFailed(m_device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(constantBufferSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&m_constantBuffer)));

		// 상수 버퍼 뷰 (constant buffer view) 서술 및 생성.
		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
		cbvDesc.BufferLocation = m_constantBuffer->GetGPUVirtualAddress();
		cbvDesc.SizeInBytes = constantBufferSize;
		m_device->CreateConstantBufferView(&cbvDesc, m_cbvHeap->GetCPUDescriptorHandleForHeapStart());

		// 상수 버퍼 매핑 및 초기화를 한다. 앱이 종료될 때까지 매핑을 해제하지 않는다.
		// 리소스가 할당된 동안 매핑된 상태를 유지해도 된다.
		CD3DX12_RANGE readRange(0, 0);
		ThrowIfFailed(m_constantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_pCbvDataBegin)));
		memcpy(m_pCbvDataBegin, &m_constantBufferData, sizeof(m_constantBufferData));
	}

	// 동기화(synchronization) 관련 오브젝트 생성 및 GPU에 에셋이 업로드 될 때까지 기다림.
	{
		ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
		//m_fenceValue = 1;
		m_fenceValues[m_frameIndex]++;

		// 프레임 동기화에 사용할 이벤트 핸들을 생성.
		m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		if (m_fenceEvent == nullptr)
		{
			ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
		}

		// 명령 리스트가 실행될 때까지 기다림. 메인 루프에서는 같은 
		// 명령 리스트를 재사용하지만, 해당 예제는 계속하기 전에 
		// 설정이 완료될 때까지 기다림.
		//WaitForPreviousFrame();
		WaitForGpu();
	}
}

void D3D12Hi::PopulateCommandList()
{
	// 명령 리스트 할당자(Command list allocators)는 연관된 명령 리스트(cl)가 
	// GPU에서 실행을 완료한 경우에만 재설정될 수 있다. 따라서 
	// 앱은 GPU 실행 진행 상황을 결정하기 위해 펜스(fences)를 사용해야 한다.
	ThrowIfFailed(m_commandAllocators[m_frameIndex]->Reset());

	// 하지만, 특정 명령 리스트(cl)에 대해 ExecuteCommandList()가 호출되면 
	// 해당 명령 리스트는 언제든지 재설정할 수 있으며, 재기록(re-recording) 
	// 해야 한다.
	ThrowIfFailed(m_commandList->Reset(m_commandAllocators[m_frameIndex].Get(), m_pipelineState.Get()));

	// 필요한 상태(state) 설정.
	m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());

	ID3D12DescriptorHeap* ppHeaps[] = { m_cbvHeap.Get() };
	m_commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

	m_commandList->SetGraphicsRootDescriptorTable(0, m_cbvHeap->GetGPUDescriptorHandleForHeapStart());
	m_commandList->RSSetViewports(1, &m_viewport);
	m_commandList->RSSetScissorRects(1, &m_scissorRect);

	// 백 버퍼가 렌더 타겟으로 사용될 것을 나타냄.
	m_commandList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(
			m_renderTargets[m_frameIndex].Get(),
			D3D12_RESOURCE_STATE_PRESENT,
			D3D12_RESOURCE_STATE_RENDER_TARGET));

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);
	m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

	// 명령 기록(Record commands).
	const float clearColor[] = { 0.278f, 0.576f, 0.686f, 1.f };
	m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
	m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
	m_commandList->DrawInstanced(3, 1, 0, 0);

	// 이제 백 버퍼가 제시(present)에 사용될 것을 나타냄.
	m_commandList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(
			m_renderTargets[m_frameIndex].Get(),
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PRESENT));

	ThrowIfFailed(m_commandList->Close());
}

//void D3D12Hi::WaitForPreviousFrame()
//{
//	// 이 코드는 단순성을 위해 다음과 같이 구현되었다. 프레임이 완료될 때까지 GPU의 렌더링 작업 완료를 기다리는 것이 
//	// 최선의 방법이 아니다.
//
//	// 시그널과 펜스 값을 증가.
//	const UINT64 fence = m_fenceValue;
//	ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), fence));
//	m_fenceValue++;
//
//	// 이전 프레임이 완료될 때까지 기다립니다.
//	if (m_fence->GetCompletedValue() < fence)
//	{
//		ThrowIfFailed(m_fence->SetEventOnCompletion(fence, m_fenceEvent));
//		WaitForSingleObject(m_fenceEvent, INFINITE);
//	}
//
//	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
//}

// 다음 프레임 렌더 준비.
void D3D12Hi::MoveToNextFrame()
{
	// 큐에서 singnal 명령 예약.
	const UINT64 currentFenceValue = m_fenceValues[m_frameIndex];
	ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), currentFenceValue));

	// 프레임 업데이트 인덱스.
	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

	// 다음 프레임이 렌더될 준비가 안됬다면, 준비될 때까지 기다림.
	if (m_fence->GetCompletedValue() < m_fenceValues[m_frameIndex])
	{
		ThrowIfFailed(m_fence->SetEventOnCompletion(m_fenceValues[m_frameIndex], m_fenceEvent));
		WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);
	}

	// 다음 프레임 fence 값 세팅.
	m_fenceValues[m_frameIndex] = currentFenceValue + 1;
}

// 보류 중인 GPU 작업이 끝날 때까지 기다림.
void D3D12Hi::WaitForGpu()
{
	// 큐에서 singnal 명령 예약.
	ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), m_fenceValues[m_frameIndex]));

	// fence 가 처리 될 때까지 기다림.
	ThrowIfFailed(m_fence->SetEventOnCompletion(m_fenceValues[m_frameIndex], m_fenceEvent));
	WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);

	// 현재 프레임의 fence 값 증가.
	m_fenceValues[m_frameIndex]++;
}
