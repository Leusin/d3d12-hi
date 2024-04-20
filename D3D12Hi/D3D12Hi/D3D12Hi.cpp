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

// ������ ��� ��(frame-based values) ������Ʈ.
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

	//WaitForPreviousFrame();
	MoveToNextFrame();
}

void D3D12Hi::OnDestroy()
{
	// �Ҹ��ڿ� ���� ������ �ڿ��� GPU�� �� �̻� �������� �ʰ� ��.
	//WaitForPreviousFrame();
	WaitForGpu();

	CloseHandle(m_fenceEvent);
}

// ������ ������������ �����ϴ� �� �ε�.
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
		m_commandQueue.Get(), // ���� ü���� ť�� �ʿ��ϴ�. �׷��� ������ �÷���(force a flush; ���� ó�� ���� �۾��� ��� �Ϸ��ϰ�, �ش� �۾��� �Ϸ�Ǿ����� �����ϴ� ����ȭ �۾�)�� �� �ִ�.
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
		// ���� Ÿ�� ���� (RTV) descriptor heap�� ���� �� ����.
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
		rtvHeapDesc.NumDescriptors = FrameCount;
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		ThrowIfFailed(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));

		m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		// ��� ���� �� (CBV) descriptor heap�� ���� �� ����.
		// �÷��״� descriptor heap �� ���������ο� ���ε� �� �� �ְ�
		// �� �ȿ� ���Ե� descriptor�� ��Ʈ ���̺��� ���� �� �� ������ ��Ÿ����.
		D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc = {};
		cbvHeapDesc.NumDescriptors = 1;
		cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		ThrowIfFailed(m_device->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&m_cbvHeap)));
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
			
			ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocators[n])));
		}
	}
}

void D3D12Hi::LoadAssets()
{
	// �ϳ��� CBV descriptor table �� ������ ��Ʈ �ñ״�ó ����. 
	{
		D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};

		// �ش� ���� �����ϴ� ���� ���� ����. 
		// CheckFeatureSupport�� ��ȯ�ϴ� ������ HighestVersion ���� ������ �ȵȴ�.
		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

		if (FAILED(m_device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
		{
			featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
		}

		CD3DX12_DESCRIPTOR_RANGE1 ranges[1];
		CD3DX12_ROOT_PARAMETER1 rootParameters[1];

		ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
		rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_VERTEX);

		// Ư�� ���������� �ܰ迡 ���� ���ʿ��� ���ٸ� �ź��Ѵ�.
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

	// ���̴��� �������ϰ� �ε��ϴ� ���������� ����(state) ����.
	{
		ComPtr<ID3DBlob> vertexShader;
		ComPtr<ID3DBlob> pixelShader;

#if defined(_DEBUG)
		// �׷��Ƚ� ����� ���� Ȱ��ȭ �÷���.
		UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
		UINT compileFlags = 0;
#endif

		ThrowIfFailed(D3DCompileFromFile(GetAssetFullPath(L"shaders.hlsl").c_str(), nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, nullptr));
		ThrowIfFailed(D3DCompileFromFile(GetAssetFullPath(L"shaders.hlsl").c_str(), nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, nullptr));

		// ���ؽ� ��ǲ ���̾ƿ�(vertex input layout) ����.
		D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};

		// �׷��� ���������� ���� ��ü (PSO) ���� �� ����.
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

	// Ŀ�ǵ� ����(cl) ����
	ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocators[m_frameIndex].Get(), m_pipelineState.Get(), IID_PPV_ARGS(&m_commandList)));

	ThrowIfFailed(m_commandList->Close());

	// ���ؽ� ���� ����.
	{
		// �ﰢ���� ���� ������Ʈ�� ����.
		Vertex triangleVertices[] =
		{
			{ { 0.0f, 0.25f * m_aspectRatio, 0.0f }, { 0.867f, 0.341f, 0.275f, 1.0f } },
			{ { 0.25f, -0.25f * m_aspectRatio, 0.0f }, { 1.0f, 0.769f, 0.439f, 1.0f } },
			{ { -0.25f, -0.25f * m_aspectRatio, 0.0f }, { 0.545f, 0.196f, 0.173f, 1.0f } }
		};

		const UINT vertexBufferSize = sizeof(triangleVertices);

		// ����: ���� �����͸� �����ϱ� ���� ���ε� ���� ����ϴ� ���� ������� 
		// ����. �� �ڵ忡���� ������ �����ؾ� �� ���ؽ��� �ſ� ���� �ڵ尡 
		// �����ϱ� ������ ���ε� ���� ����Ѵ�.
		ThrowIfFailed(m_device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&m_vertexBuffer)));

		// �ﰢ�� �����͸� ���ؽ� ���۷� ����.
		UINT8* pVertexDataBegin;
		CD3DX12_RANGE readRange(0, 0); // CPU���� �� ���ҽ��� ���� ����.
		ThrowIfFailed(m_vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
		memcpy(pVertexDataBegin, triangleVertices, sizeof(triangleVertices));
		m_vertexBuffer->Unmap(0, nullptr);

		// ���ؽ� ���� �� �ʱ�ȭ.
		m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
		m_vertexBufferView.StrideInBytes = sizeof(Vertex);
		m_vertexBufferView.SizeInBytes = vertexBufferSize;
	}

	// ��� ���� ����.
	{
		const UINT constantBufferSize = sizeof(SceneConstantBuffer);    // ��� ���� (CB)�� ũ��� 256-����Ʈ ������ �ʿ���Ѵ�.

		ThrowIfFailed(m_device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(constantBufferSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&m_constantBuffer)));

		// ��� ���� �� (constant buffer view) ���� �� ����.
		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
		cbvDesc.BufferLocation = m_constantBuffer->GetGPUVirtualAddress();
		cbvDesc.SizeInBytes = constantBufferSize;
		m_device->CreateConstantBufferView(&cbvDesc, m_cbvHeap->GetCPUDescriptorHandleForHeapStart());

		// ��� ���� ���� �� �ʱ�ȭ�� �Ѵ�. ���� ����� ������ ������ �������� �ʴ´�.
		// ���ҽ��� �Ҵ�� ���� ���ε� ���¸� �����ص� �ȴ�.
		CD3DX12_RANGE readRange(0, 0);
		ThrowIfFailed(m_constantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_pCbvDataBegin)));
		memcpy(m_pCbvDataBegin, &m_constantBufferData, sizeof(m_constantBufferData));
	}

	// ����ȭ(synchronization) ���� ������Ʈ ���� �� GPU�� ������ ���ε� �� ������ ��ٸ�.
	{
		ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
		//m_fenceValue = 1;
		m_fenceValues[m_frameIndex]++;

		// ������ ����ȭ�� ����� �̺�Ʈ �ڵ��� ����.
		m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		if (m_fenceEvent == nullptr)
		{
			ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
		}

		// ��� ����Ʈ�� ����� ������ ��ٸ�. ���� ���������� ���� 
		// ��� ����Ʈ�� ����������, �ش� ������ ����ϱ� ���� 
		// ������ �Ϸ�� ������ ��ٸ�.
		//WaitForPreviousFrame();
		WaitForGpu();
	}
}

void D3D12Hi::PopulateCommandList()
{
	// ��� ����Ʈ �Ҵ���(Command list allocators)�� ������ ��� ����Ʈ(cl)�� 
	// GPU���� ������ �Ϸ��� ��쿡�� �缳���� �� �ִ�. ���� 
	// ���� GPU ���� ���� ��Ȳ�� �����ϱ� ���� �潺(fences)�� ����ؾ� �Ѵ�.
	ThrowIfFailed(m_commandAllocators[m_frameIndex]->Reset());

	// ������, Ư�� ��� ����Ʈ(cl)�� ���� ExecuteCommandList()�� ȣ��Ǹ� 
	// �ش� ��� ����Ʈ�� �������� �缳���� �� ������, ����(re-recording) 
	// �ؾ� �Ѵ�.
	ThrowIfFailed(m_commandList->Reset(m_commandAllocators[m_frameIndex].Get(), m_pipelineState.Get()));

	// �ʿ��� ����(state) ����.
	m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());

	ID3D12DescriptorHeap* ppHeaps[] = { m_cbvHeap.Get() };
	m_commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

	m_commandList->SetGraphicsRootDescriptorTable(0, m_cbvHeap->GetGPUDescriptorHandleForHeapStart());
	m_commandList->RSSetViewports(1, &m_viewport);
	m_commandList->RSSetScissorRects(1, &m_scissorRect);

	// �� ���۰� ���� Ÿ������ ���� ���� ��Ÿ��.
	m_commandList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(
			m_renderTargets[m_frameIndex].Get(),
			D3D12_RESOURCE_STATE_PRESENT,
			D3D12_RESOURCE_STATE_RENDER_TARGET));

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);
	m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

	// ��� ���(Record commands).
	const float clearColor[] = { 0.278f, 0.576f, 0.686f, 1.f };
	m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
	m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
	m_commandList->DrawInstanced(3, 1, 0, 0);

	// ���� �� ���۰� ����(present)�� ���� ���� ��Ÿ��.
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
//	// �� �ڵ�� �ܼ����� ���� ������ ���� �����Ǿ���. �������� �Ϸ�� ������ GPU�� ������ �۾� �ϷḦ ��ٸ��� ���� 
//	// �ּ��� ����� �ƴϴ�.
//
//	// �ñ׳ΰ� �潺 ���� ����.
//	const UINT64 fence = m_fenceValue;
//	ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), fence));
//	m_fenceValue++;
//
//	// ���� �������� �Ϸ�� ������ ��ٸ��ϴ�.
//	if (m_fence->GetCompletedValue() < fence)
//	{
//		ThrowIfFailed(m_fence->SetEventOnCompletion(fence, m_fenceEvent));
//		WaitForSingleObject(m_fenceEvent, INFINITE);
//	}
//
//	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
//}

// ���� ������ ���� �غ�.
void D3D12Hi::MoveToNextFrame()
{
	// ť���� singnal ��� ����.
	const UINT64 currentFenceValue = m_fenceValues[m_frameIndex];
	ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), currentFenceValue));

	// ������ ������Ʈ �ε���.
	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

	// ���� �������� ������ �غ� �ȉ�ٸ�, �غ�� ������ ��ٸ�.
	if (m_fence->GetCompletedValue() < m_fenceValues[m_frameIndex])
	{
		ThrowIfFailed(m_fence->SetEventOnCompletion(m_fenceValues[m_frameIndex], m_fenceEvent));
		WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);
	}

	// ���� ������ fence �� ����.
	m_fenceValues[m_frameIndex] = currentFenceValue + 1;
}

// ���� ���� GPU �۾��� ���� ������ ��ٸ�.
void D3D12Hi::WaitForGpu()
{
	// ť���� singnal ��� ����.
	ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), m_fenceValues[m_frameIndex]));

	// fence �� ó�� �� ������ ��ٸ�.
	ThrowIfFailed(m_fence->SetEventOnCompletion(m_fenceValues[m_frameIndex], m_fenceEvent));
	WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);

	// ���� �������� fence �� ����.
	m_fenceValues[m_frameIndex]++;
}
