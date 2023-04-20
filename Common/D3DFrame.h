#pragma once

#include <WindowsX.h>
#include "D3DFrameHelper.h"
#include "GameTimer.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")
#pragma comment(lib, "dxgi.lib")

// This is the main class for the application. It is responsible for
// initializing the application window, initializing Direct3D, and
// managing the main message loop.

class D3DFrame
{
public:
	static D3DFrame* getApp();
	HINSTANCE appInst()const;
	HWND      mainWnd()const;
	float     aspectRatio()const;
	bool 	get4xMsaaState()const;
	void 	set4xMsaaState(bool value);
	 int 	run();

	 virtual bool initialize();
	 virtual LRESULT msgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

protected:
	D3DFrame(HINSTANCE hInstance);
	virtual ~D3DFrame();
	//Create the RTV and DSV descriptor heaps required by the application
	virtual void createRtvAndDsvDescriptorHeap();
	virtual void onResize();
	virtual void update(const GameTimer& gt) = 0;
	virtual void draw(const GameTimer& gt) = 0;

	virtual void onMouseDown(WPARAM btnState, int x, int y) { }
	virtual void onMouseUp(WPARAM btnState, int x, int y) { }
	virtual void onMouseMove(WPARAM btnState, int x, int y) { }

	bool initMainWindow();
	bool initDirect3D();
	//Create command queues, command list allocators, and command lists
	void createCommandObjects();
	void createSwapChain();
	void flushCommandQueue();
	//Return the ID3DResource of the current back buffer in the swap chain
	ID3D12Resource* currentBackBuffer()const;
	//Return the RTV of the current back buffer
	D3D12_CPU_DESCRIPTOR_HANDLE currentBackBufferView()const;
	//Return the DSV of the depth stencil buffer
	D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView()const;
	//Calculate the frame rate
	void calculateFrameStats();
	//Enumerate all adapters
	void logAdapters();
	//Enumerate all display output for the specified adapter
	void logAdapterOutputs(IDXGIAdapter* adapter);
	//Enumerate the display modes that a display output supports for a particular format
	void logOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format); 
	

	static D3DFrame* m_App;

	HINSTANCE m_hAppInst = nullptr;// application instance handle
	HWND m_hMainWnd = nullptr;// main window handle
	bool m_AppPaused = false;// is the application paused?
	bool m_Minimized = false;// is the application minimized?
	bool m_Maximized = false;// is the application maximized?
	bool m_Resizing = false;// are the resize bars being dragged?
	bool m_FullScreen = false;// fullscreen enabled?

	// Set ture to enable 4X MSAA. The default is false.
	bool m_4xMsaaState = false;// 4X MSAA enabled?
	UINT m_4xMsaaQuality = 0;// quality level of 4X MSAA

	// Used to keep track of the delta-time and game time
	GameTimer m_Timer;

	Microsoft::WRL::ComPtr<IDXGIFactory4> m_dxgiFactory;
	Microsoft::WRL::ComPtr<IDXGISwapChain> m_SwapChain;
	Microsoft::WRL::ComPtr<ID3D12Device> m_d3dDevice;
	Microsoft::WRL::ComPtr<ID3D12Fence> m_Fence;
	UINT64 m_CurrentFence = 0;
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_CommandQueue;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_DirectCmdListAlloc;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_CommandList;

	static const int SwapChainBufferCount = 2;
	int m_CurrentBackBuffer = 0;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_SwapChainBuffer[SwapChainBufferCount];
	Microsoft::WRL::ComPtr<ID3D12Resource> m_DepthStencilBuffer;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_RtvHeap;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_DsvHeap;
	D3D12_VIEWPORT m_ScreenViewport;
	D3D12_RECT m_ScissorRect;
	UINT m_RtvDescriptorSize = 0;
	UINT m_DsvDescriptorSize = 0;
	UINT m_CbvSrvUavDescriptorSize = 0;

	std::wstring m_MainWndCaption = L"D3D DEMO";
	D3D_DRIVER_TYPE m_D3dDriverType = D3D_DRIVER_TYPE_HARDWARE;
	DXGI_FORMAT m_BackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_FORMAT m_DepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	// The width and height of the client area of the window
	int m_ClientWidth = 800;
	int m_ClientHeight = 600;
};