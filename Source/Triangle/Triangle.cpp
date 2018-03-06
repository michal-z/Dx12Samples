#include "Pch.h"
#include "Dx12SamplesLib.h"


#define k_Name "Triangle"
#define k_ResolutionX 1920
#define k_ResolutionY 1080
#define k_SwapBufferCount 4

static ID3D12Device *s_Gpu;
static ID3D12CommandQueue *s_CmdQueue;
static ID3D12CommandAllocator *s_CmdAlloc[2];
static ID3D12GraphicsCommandList *s_CmdList;
static IDXGISwapChain3 *s_SwapChain;
static unsigned s_DescriptorSize;
static unsigned s_DescriptorSizeRtv;
static ID3D12DescriptorHeap *s_SwapBufferHeap;
static ID3D12DescriptorHeap *s_DepthBufferHeap;
static D3D12_CPU_DESCRIPTOR_HANDLE s_SwapBufferHeapStart;
static D3D12_CPU_DESCRIPTOR_HANDLE s_DepthBufferHeapStart;
static ID3D12Resource *s_SwapBuffers[k_SwapBufferCount];
static ID3D12Resource *s_DepthBuffer;

static LRESULT CALLBACK ProcessWindowMessage(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
	switch (message)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	case WM_KEYDOWN:
		if (wparam == VK_ESCAPE)
		{
			PostQuitMessage(0);
			return 0;
		}
		break;
	}
	return DefWindowProc(window, message, wparam, lparam);
}

static HWND MakeWindow(const char *name, unsigned resolutionX, unsigned resolutionY)
{
	WNDCLASS winclass = {};
	winclass.lpfnWndProc = ProcessWindowMessage;
	winclass.hInstance = GetModuleHandle(nullptr);
	winclass.hCursor = LoadCursor(nullptr, IDC_ARROW);
	winclass.lpszClassName = name;
	if (!RegisterClass(&winclass))
		assert(0);

	RECT rect = { 0, 0, (LONG)resolutionX, (LONG)resolutionY };
	if (!AdjustWindowRect(&rect, WS_OVERLAPPED | WS_SYSMENU | WS_CAPTION | WS_MINIMIZEBOX, 0))
		assert(0);

	HWND window = CreateWindowEx(
		0, name, name, WS_OVERLAPPED | WS_SYSMENU | WS_CAPTION | WS_MINIMIZEBOX | WS_VISIBLE,
		CW_USEDEFAULT, CW_USEDEFAULT,
		rect.right - rect.left, rect.bottom - rect.top,
		NULL, NULL, NULL, 0);
	assert(window);

	IDXGIFactory4 *factory;
#ifdef _DEBUG
	VHR(CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&factory)));
#else
	VHR(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory)));
#endif

#ifdef _DEBUG
	{
		ID3D12Debug *dbg;
		D3D12GetDebugInterface(IID_PPV_ARGS(&dbg));
		if (dbg)
		{
			dbg->EnableDebugLayer();
			ID3D12Debug1 *dbg1;
			dbg->QueryInterface(IID_PPV_ARGS(&dbg1));
			if (dbg1)
				dbg1->SetEnableGPUBasedValidation(TRUE);
			SAFE_RELEASE(dbg);
			SAFE_RELEASE(dbg1);
		}
	}
#endif
	if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_1, IID_PPV_ARGS(&s_Gpu))))
	{
		return false;
	}

	D3D12_COMMAND_QUEUE_DESC cmdQueueDesc = {};
	cmdQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	cmdQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	cmdQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	VHR(s_Gpu->CreateCommandQueue(&cmdQueueDesc, IID_PPV_ARGS(&s_CmdQueue)));

	DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
	swapChainDesc.BufferCount = k_SwapBufferCount;
	swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.OutputWindow = window;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
	swapChainDesc.Windowed = TRUE;

	IDXGISwapChain *tempSwapChain;
	VHR(factory->CreateSwapChain(s_CmdQueue, &swapChainDesc, &tempSwapChain));
	VHR(tempSwapChain->QueryInterface(IID_PPV_ARGS(&s_SwapChain)));
	SAFE_RELEASE(tempSwapChain);
	SAFE_RELEASE(factory);

	for (unsigned i = 0; i < 2; ++i)
		VHR(s_Gpu->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&s_CmdAlloc[i])));

	s_DescriptorSize = s_Gpu->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	s_DescriptorSizeRtv = s_Gpu->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	/* swap buffers */ {
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
		heapDesc.NumDescriptors = k_SwapBufferCount;
		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		VHR(s_Gpu->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&s_SwapBufferHeap)));
		s_SwapBufferHeapStart = s_SwapBufferHeap->GetCPUDescriptorHandleForHeapStart();

		CD3DX12_CPU_DESCRIPTOR_HANDLE handle(s_SwapBufferHeapStart);

		for (unsigned i = 0; i < k_SwapBufferCount; ++i)
		{
			VHR(s_SwapChain->GetBuffer(i, IID_PPV_ARGS(&s_SwapBuffers[i])));

			s_Gpu->CreateRenderTargetView(s_SwapBuffers[i], nullptr, handle);
			handle.Offset(s_DescriptorSizeRtv);
		}
	}
	/* depth buffer */ {
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
		heapDesc.NumDescriptors = 1;
		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		VHR(s_Gpu->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&s_DepthBufferHeap)));
		s_DepthBufferHeapStart = s_DepthBufferHeap->GetCPUDescriptorHandleForHeapStart();

		CD3DX12_RESOURCE_DESC imageDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, k_ResolutionX, k_ResolutionY);
		imageDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL | D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
		VHR(s_Gpu->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE,
			&imageDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE,
			&CD3DX12_CLEAR_VALUE(DXGI_FORMAT_D32_FLOAT, 1.0f, 0), IID_PPV_ARGS(&s_DepthBuffer)));

		D3D12_DEPTH_STENCIL_VIEW_DESC viewDesc = {};
		viewDesc.Format = DXGI_FORMAT_D32_FLOAT;
		viewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		viewDesc.Flags = D3D12_DSV_FLAG_NONE;
		s_Gpu->CreateDepthStencilView(s_DepthBuffer, &viewDesc, s_DepthBufferHeapStart);
	}

	return window;
}

int CALLBACK WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	SetProcessDPIAware();
	HWND window = MakeWindow(k_Name, k_ResolutionX, k_ResolutionY);

	for (;;)
	{
		MSG message = {};
		if (PeekMessage(&message, 0, 0, 0, PM_REMOVE))
		{
			DispatchMessage(&message);
			if (message.message == WM_QUIT)
				break;
		}
		else
		{
			double frameTime;
			float frameDeltaTime;
			Lib::UpdateFrameTime(window, k_Name, &frameTime, &frameDeltaTime);
		}
	}

	return 0;
}
