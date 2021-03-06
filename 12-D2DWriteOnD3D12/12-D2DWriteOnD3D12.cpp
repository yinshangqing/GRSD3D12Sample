#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN // 从 Windows 头中排除极少使用的资料
#include <windows.h>
#include <tchar.h>
#include <fstream>  //for ifstream
#include <wrl.h> //添加WTL支持 方便使用COM
#include <atlconv.h>
#include <atlcoll.h>  //for atl array
#include <strsafe.h>  //for StringCchxxxxx function
#include <dxgi1_6.h>
#include <d3d12.h> //for d3d12
#include <d3dcompiler.h>
#if defined(_DEBUG)
#include <dxgidebug.h>
#endif
#include <DirectXMath.h>
#include "..\WindowsCommons\d3dx12.h"
#include "..\WindowsCommons\DDSTextureLoader12.h"
//--------------------------------------------------------------
#include <d3d11_4.h>
#include <d3d11on12.h>
#include <d2d1_3.h>
#include <dwrite.h>
//--------------------------------------------------------------

using namespace std;
using namespace Microsoft;
using namespace Microsoft::WRL;
using namespace DirectX;

#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3dcompiler.lib")
//--------------------------------------------------------------
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "d3d11.lib")
//--------------------------------------------------------------
#ifndef GRS_BLOCK

#define GRS_WND_CLASS_NAME _T("GRS Game Window Class")
#define GRS_WND_TITLE	_T("GRS D2D DWrite On D3D12 Sample(Base On MultiThread&Adapter)")

#define GRS_THROW_IF_FAILED(hr) { HRESULT _hr = (hr);if (FAILED(_hr)){ throw CGRSCOMException(_hr); } }
#define GRS_SAFE_RELEASE(p) if(nullptr != (p)){(p)->Release();(p)=nullptr;}

//新定义的宏用于上取整除法
#define GRS_UPPER_DIV(A,B) ((UINT)(((A)+((B)-1))/(B)))
//更简洁的向上边界对齐算法 内存管理中常用 请记住
#define GRS_UPPER(A,B) ((UINT)(((A)+((B)-1))&~(B - 1)))

// 为了调试加入下面的内联函数和宏定义，为每个接口对象设置名称，方便查看调试输出
#if defined(_DEBUG)
inline void GRS_SetD3D12DebugName(ID3D12Object* pObject, LPCWSTR name)
{
	pObject->SetName(name);
}

inline void GRS_SetD3D12DebugNameIndexed(ID3D12Object* pObject, LPCWSTR name, UINT index)
{
	WCHAR _DebugName[MAX_PATH] = {};
	if (SUCCEEDED(StringCchPrintfW(_DebugName, _countof(_DebugName), L"%s[%u]", name, index)))
	{
		pObject->SetName(_DebugName);
	}
}
#else

inline void GRS_SetD3D12DebugName(ID3D12Object*, LPCWSTR)
{
}
inline void GRS_SetD3D12DebugNameIndexed(ID3D12Object*, LPCWSTR, UINT)
{
}

#endif

#define GRS_SET_D3D12_DEBUGNAME(x)						GRS_SetD3D12DebugName(x, L#x)
#define GRS_SET_D3D12_DEBUGNAME_INDEXED(x, n)			GRS_SetD3D12DebugNameIndexed(x[n], L#x, n)

#define GRS_SET_D3D12_DEBUGNAME_COMPTR(x)				GRS_SetD3D12DebugName(x.Get(), L#x)
#define GRS_SET_D3D12_DEBUGNAME_INDEXED_COMPTR(x, n)	GRS_SetD3D12DebugNameIndexed(x[n].Get(), L#x, n)

#if defined(_DEBUG)
inline void GRS_SetDXGIDebugName(IDXGIObject* pObject, LPCWSTR name)
{
	size_t szLen = 0;
	StringCchLengthW(name, 50, &szLen);
	pObject->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(szLen - 1), name);
}

inline void GRS_SetDXGIDebugNameIndexed(IDXGIObject* pObject, LPCWSTR name, UINT index)
{
	size_t szLen = 0;
	WCHAR _DebugName[MAX_PATH] = {};
	if (SUCCEEDED(StringCchPrintfW(_DebugName, _countof(_DebugName), L"%s[%u]", name, index)))
	{
		StringCchLengthW(_DebugName, _countof(_DebugName), &szLen);
		pObject->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(szLen), _DebugName);
	}
}
#else

inline void GRS_SetDXGIDebugName(IDXGIObject*, LPCWSTR)
{
}
inline void GRS_SetDXGIDebugNameIndexed(IDXGIObject*, LPCWSTR, UINT)
{
}

#endif

#define GRS_SET_DXGI_DEBUGNAME(x)						GRS_SetDXGIDebugName(x, L#x)
#define GRS_SET_DXGI_DEBUGNAME_INDEXED(x, n)			GRS_SetDXGIDebugNameIndexed(x[n], L#x, n)

#define GRS_SET_DXGI_DEBUGNAME_COMPTR(x)				GRS_SetDXGIDebugName(x.Get(), L#x)
#define GRS_SET_DXGI_DEBUGNAME_INDEXED_COMPTR(x, n)		GRS_SetDXGIDebugNameIndexed(x[n].Get(), L#x, n)


class CGRSCOMException
{
public:
	CGRSCOMException(HRESULT hr) : m_hrError(hr)
	{
	}
	HRESULT Error() const
	{
		return m_hrError;
	}
private:
	const HRESULT m_hrError;
};

#endif

//默认后缓冲数量
const UINT								g_nFrameBackBufCount = 3u;

// 显卡参数集合
struct ST_GRS_GPU_PARAMS
{
	UINT								m_nGPUIndex;

	UINT								m_nRTVDescriptorSize;
	UINT								m_nSRVDescriptorSize;

	ComPtr<ID3D12Device4>				m_pID3D12Device4;
	ComPtr<ID3D12CommandQueue>			m_pICMDQueue;

	ComPtr<ID3D12Fence>					m_pIFence;
	ComPtr<ID3D12Fence>					m_pISharedFence;

	HANDLE								m_hEventFence;

	ComPtr<ID3D12Resource>				m_pIRTRes[g_nFrameBackBufCount];
	ComPtr<ID3D12DescriptorHeap>		m_pIRTVHeap;
	ComPtr<ID3D12Resource>				m_pIDSRes;
	ComPtr<ID3D12DescriptorHeap>		m_pIDSVHeap;

	ComPtr<ID3D12DescriptorHeap>		m_pISRVHeap;
	ComPtr<ID3D12DescriptorHeap>		m_pISampleHeap;

	ComPtr<ID3D12Heap>					m_pICrossAdapterHeap;
	ComPtr<ID3D12Resource>				m_pICrossAdapterResPerFrame[g_nFrameBackBufCount];

	ComPtr<ID3D12CommandAllocator>		m_pICMDAlloc;
	ComPtr<ID3D12GraphicsCommandList>	m_pICMDList;

	ComPtr<ID3D12RootSignature>			m_pIRS;
	ComPtr<ID3D12PipelineState>			m_pIPSO;

};

// 渲染子线程参数
struct ST_GRS_THREAD_PARAMS
{
	UINT								m_nThreadIndex;				//序号
	DWORD								m_dwThisThreadID;
	HANDLE								m_hThisThread;
	DWORD								m_dwMainThreadID;
	HANDLE								m_hMainThread;

	HANDLE								m_hEventRun;
	HANDLE								m_hEventRenderOver;

	UINT								m_nCurrentFrameIndex;//当前渲染后缓冲序号
	ULONGLONG							m_nStartTime;		 //当前帧开始时间
	ULONGLONG							m_nCurrentTime;	     //当前时间

	XMFLOAT4							m_v4ModelPos;
	TCHAR								m_pszDDSFile[MAX_PATH];
	CHAR								m_pszMeshFile[MAX_PATH];

	ID3D12Device4* m_pID3D12Device4;

	ID3D12DescriptorHeap* m_pIRTVHeap;
	ID3D12DescriptorHeap* m_pIDSVHeap;

	ID3D12CommandAllocator* m_pICMDAlloc;
	ID3D12GraphicsCommandList* m_pICMDList;

	ID3D12RootSignature* m_pIRS;
	ID3D12PipelineState* m_pIPSO;
};

// 顶点结构
struct ST_GRS_VERTEX
{
	XMFLOAT4 m_vPos;		//Position
	XMFLOAT2 m_vTex;		//Texcoord
	XMFLOAT3 m_vNor;		//Normal
};

//用于后处理渲染的矩形定点结构
struct ST_GRS_VERTEX_QUAD
{
	XMFLOAT4 m_vPos;		//Position
	XMFLOAT2 m_vTex;		//Texcoord
};

// 常量缓冲区
struct ST_GRS_MVP
{
	XMFLOAT4X4 m_MVP;			//经典的Model-view-projection(MVP)矩阵.
};

struct ST_GRS_PEROBJECT_CB
{
	UINT	m_nFun;
	float	m_fQuatLevel;   //量化bit数，取值2-6
	float	m_fWaterPower;  //表示水彩扩展力度，单位为像素
};

int						g_iWndWidth = 1024;
int						g_iWndHeight = 768;
CD3DX12_VIEWPORT		g_stViewPort(0.0f, 0.0f, static_cast<float>(g_iWndWidth), static_cast<float>(g_iWndHeight));
CD3DX12_RECT			g_stScissorRect(0, 0, static_cast<LONG>(g_iWndWidth), static_cast<LONG>(g_iWndHeight));

//初始的默认摄像机的位置
XMFLOAT3				g_f3EyePos = XMFLOAT3(0.0f, 5.0f, -10.0f);  //眼睛位置
XMFLOAT3				g_f3LockAt = XMFLOAT3(0.0f, 0.0f, 0.0f);    //眼睛所盯的位置
XMFLOAT3				g_f3HeapUp = XMFLOAT3(0.0f, 1.0f, 0.0f);    //头部正上方位置

float					g_fYaw = 0.0f;			// 绕正Z轴的旋转量.
float					g_fPitch = 0.0f;			// 绕XZ平面的旋转量

double					g_fPalstance = 5.0f * XM_PI / 180.0f;	//物体旋转的角速度，单位：弧度/秒

XMFLOAT4X4				g_mxWorld = {}; //World Matrix
XMFLOAT4X4				g_mxVP = {};    //View Projection Matrix

// 全局线程参数
const UINT				g_nMaxThread = 3;
const UINT				g_nThdSphere = 0;
const UINT				g_nThdCube = 1;
const UINT				g_nThdPlane = 2;
ST_GRS_THREAD_PARAMS	g_stThread[g_nMaxThread] = {};

// App路径，方便查找资源
TCHAR					g_pszAppPath[MAX_PATH] = {};

//后处理需要的参数，设置为全局变量方便在窗口过程中根据按键输入调整
UINT					g_nFunNO = 1;		//当前使用效果函数的序号（按空格键循环切换）
UINT					g_nMaxFunNO = 2;    //总的效果函数个数
float					g_fQuatLevel = 2.0f;    //量化bit数，取值2-6
float					g_fWaterPower = 40.0f;  //表示水彩扩展力度，单位为像素

//使用哪个版本PS做最后的模糊处理：0 原样输出 1 矩阵形式高斯模糊PS 2 双向分离优化后的高斯模糊PS（来自微软官方示例）
UINT					g_nUsePSID = 1;

//渲染子线程函数
UINT __stdcall RenderThread(void* pParam);
//主窗口过程
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
//加载纹理的函数
BOOL LoadMeshVertex(const CHAR* pszMeshFileName, UINT& nVertexCnt, ST_GRS_VERTEX*& ppVertex, UINT*& ppIndices);

int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR    lpCmdLine, int nCmdShow)
{
	HWND								hWnd = nullptr;
	MSG									msg = {};

	UINT								nDXGIFactoryFlags = 0U;
	const float							faClearColor[] = { 0.2f, 0.5f, 1.0f, 1.0f };

	ComPtr<IDXGIFactory5>				pIDXGIFactory5;
	ComPtr<IDXGIFactory6>				pIDXGIFactory6;
	ComPtr<IDXGIAdapter1>				pIAdapter1;
	ComPtr<IDXGISwapChain1>				pISwapChain1;
	ComPtr<IDXGISwapChain3>				pISwapChain3;

	const UINT							c_nMaxGPUCnt = 2;			//支持两个GPU进行渲染
	const UINT							c_nMainGPU = 0;
	const UINT							c_nSecondGPU = 1;
	ST_GRS_GPU_PARAMS					stGPU[c_nMaxGPUCnt] = {};

	// 用于复制渲染结果的复制命令队列、命令分配器、命令列表对象
	ComPtr<ID3D12CommandQueue>			pICmdQueueCopy;
	ComPtr<ID3D12CommandAllocator>		pICMDAllocCopy;
	ComPtr<ID3D12GraphicsCommandList>	pICMDListCopy;

	//当不能直接共享资源时，就在辅助显卡上创建独立的资源，用于复制主显卡渲染结果过来
	ComPtr<ID3D12Resource>				pISecondaryAdapterTexutrePerFrame[g_nFrameBackBufCount];
	BOOL								bCrossAdapterTextureSupport = FALSE;
	CD3DX12_RESOURCE_DESC				stRenderTargetDesc = {};
	const float							v4ClearColor[4] = { 0.2f, 0.5f, 1.0f, 1.0f };

	UINT								nCurrentFrameIndex = 0;

	DXGI_FORMAT							emRTFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_FORMAT							emDSFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	UINT64								n64FenceValue = 1ui64;
	UINT64								n64CurrentFenceValue = 0;

	ComPtr<ID3D12CommandAllocator>		pICMDAllocBefore;
	ComPtr<ID3D12GraphicsCommandList>	pICMDListBefore;

	ComPtr<ID3D12CommandAllocator>		pICMDAllocLater;
	ComPtr<ID3D12GraphicsCommandList>	pICMDListLater;

	CAtlArray<HANDLE>					arHWaited;
	CAtlArray<HANDLE>					arHSubThread;

	//辅助显卡上后处理需要的资源变量（Second Pass）
	const UINT							c_nPostPassCnt = 2;										//后处理趟数
	const UINT							c_nPostPass0 = 0;
	const UINT							c_nPostPass1 = 1;
	ComPtr<ID3D12Resource>				pIOffLineRTRes[c_nPostPassCnt][g_nFrameBackBufCount];	//离线渲染的渲染目标资源
	ComPtr<ID3D12DescriptorHeap>		pIRTVOffLine[c_nPostPassCnt];							//离线渲染RTV

	// Second Pass Values
	ComPtr<ID3D12Resource>				pIVBQuad;
	ComPtr<ID3D12Resource>				pIVBQuadUpload;
	D3D12_VERTEX_BUFFER_VIEW			pstVBVQuad;
	SIZE_T								szSecondPassCB = GRS_UPPER(sizeof(ST_GRS_PEROBJECT_CB), 256);
	ST_GRS_PEROBJECT_CB*				pstCBSecondPass = nullptr;
	ComPtr<ID3D12Resource>				pICBResSecondPass;
	ComPtr<ID3D12Resource>				pINoiseTexture;
	ComPtr<ID3D12Resource>				pINoiseTextureUpload;

	// Third Pass Values
	ComPtr<ID3D12CommandAllocator>		pICMDAllocPostPass;
	ComPtr<ID3D12GraphicsCommandList>	pICMDListPostPass;
	ComPtr<ID3D12RootSignature>			pIRSPostPass;
	ComPtr<ID3D12PipelineState>			pIPSOPostPass[c_nPostPassCnt];
	ComPtr<ID3D12DescriptorHeap>		pISRVHeapPostPass[c_nPostPassCnt];
	ComPtr<ID3D12DescriptorHeap>		pISampleHeapPostPass;

	//-----------------------------------------------------------------------------------------------------------
	// D2D & DWrite 相关变量
	UINT								nD3D11DeviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
	const FLOAT							DEFAULT_DPI = 96.f;
	ComPtr<ID3D11Device5>				pID3D11Device5;
	ComPtr<ID3D11DeviceContext4>		pID3D11DeviceContext4;
	ComPtr<ID3D11On12Device1>			pID3D11On12Device1;
	ComPtr<ID3D11Resource>				pID3D11WrappedBackBuffers[g_nFrameBackBufCount];
	ComPtr<ID2D1Bitmap1>				pID2DRenderTargets[g_nFrameBackBufCount];
	ComPtr<ID2D1Factory7>				pID2D1Factory7;
	ComPtr<IDWriteFactory>				pIDWriteFactory;
	ComPtr<ID2D1Device6>				pID2D1Device6;
	ComPtr<ID2D1DeviceContext6>			pID2D1DeviceContext6;
	ComPtr<ID2D1SolidColorBrush>		pID2D1SolidColorBrush;
	ComPtr<IDWriteTextFormat>			pIDWriteTextFormat;
	//-----------------------------------------------------------------------------------------------------------

	try
	{
		GRS_THROW_IF_FAILED(::CoInitialize(nullptr));
		// 得到当前的工作目录，方便我们使用相对路径来访问各种资源文件
		{
			if (0 == ::GetModuleFileName(nullptr, g_pszAppPath, MAX_PATH))
			{
				GRS_THROW_IF_FAILED(HRESULT_FROM_WIN32(GetLastError()));
			}

			WCHAR* lastSlash = _tcsrchr(g_pszAppPath, _T('\\'));
			if (lastSlash)
			{//删除Exe文件名
				*(lastSlash) = _T('\0');
			}

			lastSlash = _tcsrchr(g_pszAppPath, _T('\\'));
			if (lastSlash)
			{//删除x64路径
				*(lastSlash) = _T('\0');
			}
			
			lastSlash = _tcsrchr(g_pszAppPath, _T('\\'));
			if (lastSlash)
			{//删除Debug 或 Release路径
				*(lastSlash + 1) = _T('\0');
			}
		}

		//1、创建窗口
		{
			//---------------------------------------------------------------------------------------------
			WNDCLASSEX wcex = {};
			wcex.cbSize = sizeof(WNDCLASSEX);
			wcex.style = CS_GLOBALCLASS;
			wcex.lpfnWndProc = WndProc;
			wcex.cbClsExtra = 0;
			wcex.cbWndExtra = 0;
			wcex.hInstance = hInstance;
			wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
			wcex.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);		//防止无聊的背景重绘
			wcex.lpszClassName = GRS_WND_CLASS_NAME;
			RegisterClassEx(&wcex);

			DWORD dwWndStyle = WS_OVERLAPPED | WS_SYSMENU;
			RECT rtWnd = { 0, 0, g_iWndWidth, g_iWndHeight };
			AdjustWindowRect(&rtWnd, dwWndStyle, FALSE);

			// 计算窗口居中的屏幕坐标
			INT posX = (GetSystemMetrics(SM_CXSCREEN) - rtWnd.right - rtWnd.left) / 2;
			INT posY = (GetSystemMetrics(SM_CYSCREEN) - rtWnd.bottom - rtWnd.top) / 2;

			hWnd = CreateWindowW(GRS_WND_CLASS_NAME, GRS_WND_TITLE, dwWndStyle
				, posX, posY, rtWnd.right - rtWnd.left, rtWnd.bottom - rtWnd.top
				, nullptr, nullptr, hInstance, nullptr);

			if (!hWnd)
			{
				throw CGRSCOMException(HRESULT_FROM_WIN32(GetLastError()));
			}
		}

		//2、打开显示子系统的调试支持
		{
#if defined(_DEBUG)
			ComPtr<ID3D12Debug> debugController;
			if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
			{
				debugController->EnableDebugLayer();
				// 打开附加的调试支持
				nDXGIFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
			}
#endif
		}

		//3、创建DXGI Factory对象
		{
			GRS_THROW_IF_FAILED(CreateDXGIFactory2(nDXGIFactoryFlags, IID_PPV_ARGS(&pIDXGIFactory5)));
			GRS_SET_DXGI_DEBUGNAME_COMPTR(pIDXGIFactory5);
			// 关闭ALT+ENTER键切换全屏的功能，因为我们没有实现OnSize处理，所以先关闭
			GRS_THROW_IF_FAILED(pIDXGIFactory5->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER));
			//获取IDXGIFactory6接口
			GRS_THROW_IF_FAILED(pIDXGIFactory5.As(&pIDXGIFactory6));
			GRS_SET_DXGI_DEBUGNAME_COMPTR(pIDXGIFactory6);
		}

		//4、枚举适配器创建设备
		{//选择NUMA架构的独显来创建3D设备对象,暂时先不支持集显了，当然你可以修改这些行为
			D3D12_FEATURE_DATA_ARCHITECTURE stArchitecture = {};
			DXGI_ADAPTER_DESC1 stAdapterDesc[c_nMaxGPUCnt] = {};
			D3D_FEATURE_LEVEL emFeatureLevel = D3D_FEATURE_LEVEL_12_1;
			D3D12_COMMAND_QUEUE_DESC stQueueDesc = {};
			stQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

			HRESULT hr = S_OK;
			UINT i = 0;

			for (i = 0;
				(i < c_nMaxGPUCnt) &&
				SUCCEEDED(pIDXGIFactory6->EnumAdapterByGpuPreference(
					i
					, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE
					, IID_PPV_ARGS(&pIAdapter1)));
				++i)
			{
				GRS_THROW_IF_FAILED(pIAdapter1->GetDesc1(&stAdapterDesc[i]));

				if (stAdapterDesc[i].Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
				{//跳过软件虚拟适配器设备，
				 //注释这个if判断，可以使用一个真实GPU和一个虚拟软适配器来看双GPU的示例
					continue;
				}

				HRESULT hr = D3D12CreateDevice(pIAdapter1.Get()
					, emFeatureLevel
					, IID_PPV_ARGS(stGPU[i].m_pID3D12Device4.GetAddressOf()));

				if (!SUCCEEDED(hr))
				{
					::MessageBox(hWnd
						, _T("非常抱歉的通知您，\r\n您系统中最NB的显卡也不能支持DX12，例子没法继续运行！\r\n程序将退出！")
						, GRS_WND_TITLE
						, MB_OK | MB_ICONINFORMATION);

					return -1;
				}

				GRS_SET_D3D12_DEBUGNAME_COMPTR(stGPU[i].m_pID3D12Device4);

				GRS_THROW_IF_FAILED(stGPU[i].m_pID3D12Device4->CreateCommandQueue(
					&stQueueDesc
					, IID_PPV_ARGS(&stGPU[i].m_pICMDQueue)));
				GRS_SET_D3D12_DEBUGNAME_COMPTR(stGPU[i].m_pICMDQueue);

				stGPU[i].m_nGPUIndex = i;
				stGPU[i].m_nRTVDescriptorSize
					= stGPU[i].m_pID3D12Device4->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
				stGPU[i].m_nSRVDescriptorSize
					= stGPU[i].m_pID3D12Device4->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				pIAdapter1.Reset();
			}

			if (nullptr == stGPU[c_nMainGPU].m_pID3D12Device4.Get()
				|| nullptr == stGPU[c_nSecondGPU].m_pID3D12Device4.Get())
			{// 机器上没有两个以上显卡 退出了事 
				::MessageBox(hWnd
					, _T("非常抱歉的通知您，\r\n机器中显卡数量不足两个，示例程序将退出！")
					, GRS_WND_TITLE
					, MB_OK | MB_ICONINFORMATION);
				throw CGRSCOMException(E_FAIL);
			}

			//在主GPU上创建复制命令队列，所谓能者多劳
			stQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
			GRS_THROW_IF_FAILED(stGPU[c_nMainGPU].m_pID3D12Device4->CreateCommandQueue(
				&stQueueDesc
				, IID_PPV_ARGS(&pICmdQueueCopy)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pICmdQueueCopy);

			//将被使用的设备名称显示到窗口标题里
			TCHAR pszWndTitle[MAX_PATH] = {};
			::GetWindowText(hWnd, pszWndTitle, MAX_PATH);
			StringCchPrintf(pszWndTitle
				, MAX_PATH
				, _T("%s(GPU[0]:%s & GPU[1]:%s)")
				, pszWndTitle
				, stAdapterDesc[c_nMainGPU].Description
				, stAdapterDesc[c_nSecondGPU].Description);
			::SetWindowText(hWnd, pszWndTitle);
		}

		//-----------------------------------------------------------------------------------------------------------
		// 创建D3D11on12设备
		{
#if defined(_DEBUG)
			nD3D11DeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
			ComPtr<ID3D11Device> pID3D11Device;
			ComPtr<ID3D11DeviceContext> pID3D11DeviceContext;
			ComPtr<ID3D12Device> pID3D12Device;
			GRS_THROW_IF_FAILED(stGPU[c_nSecondGPU].m_pID3D12Device4.As(&pID3D12Device));
			GRS_THROW_IF_FAILED(D3D11On12CreateDevice(
				pID3D12Device.Get(),
				nD3D11DeviceFlags,
				nullptr,
				0,
				reinterpret_cast<IUnknown**>(stGPU[c_nSecondGPU].m_pICMDQueue.GetAddressOf()),
				1,
				0,
				&pID3D11Device,
				&pID3D11DeviceContext,
				nullptr
			));

			GRS_THROW_IF_FAILED(pID3D11Device.As(&pID3D11Device5));
			GRS_THROW_IF_FAILED(pID3D11DeviceContext.As(&pID3D11DeviceContext4));
			// Query the 11On12 device from the 11 device.
			GRS_THROW_IF_FAILED(pID3D11Device5.As(&pID3D11On12Device1));
		}

		//创建D2D、DWrite工厂
		{
			D2D1_FACTORY_OPTIONS stD2DFactoryOptions = {};
#if defined(_DEBUG)
			stD2DFactoryOptions.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif
			D2D1_DEVICE_CONTEXT_OPTIONS emD2DDeviceOptions = D2D1_DEVICE_CONTEXT_OPTIONS_NONE;

			GRS_THROW_IF_FAILED(D2D1CreateFactory(
				D2D1_FACTORY_TYPE_SINGLE_THREADED
				, __uuidof(ID2D1Factory7)
				, &stD2DFactoryOptions
				, &pID2D1Factory7));

			ComPtr<IDXGIDevice> pIDXGIDevice;
			GRS_THROW_IF_FAILED(pID3D11On12Device1.As(&pIDXGIDevice));
			GRS_THROW_IF_FAILED(pID2D1Factory7->CreateDevice(pIDXGIDevice.Get(), &pID2D1Device6));
			GRS_THROW_IF_FAILED(pID2D1Device6->CreateDeviceContext(emD2DDeviceOptions, &pID2D1DeviceContext6));
			GRS_THROW_IF_FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), &pIDWriteFactory));
		}
		//-----------------------------------------------------------------------------------------------------------

		//5、创建命令列表
		{
			WCHAR pszDebugName[MAX_PATH] = {};

			for (int i = 0; i < c_nMaxGPUCnt; i++)
			{
				GRS_THROW_IF_FAILED(stGPU[i].m_pID3D12Device4->CreateCommandAllocator(
					D3D12_COMMAND_LIST_TYPE_DIRECT
					, IID_PPV_ARGS(&stGPU[i].m_pICMDAlloc)));

				if (SUCCEEDED(StringCchPrintfW(pszDebugName, MAX_PATH, L"stGPU[%u].m_pICMDAlloc", i)))
				{
					stGPU[i].m_pICMDAlloc->SetName(pszDebugName);
				}

				GRS_THROW_IF_FAILED(stGPU[i].m_pID3D12Device4->CreateCommandList(
					0, D3D12_COMMAND_LIST_TYPE_DIRECT
					, stGPU[i].m_pICMDAlloc.Get(), nullptr, IID_PPV_ARGS(&stGPU[i].m_pICMDList)));
				if (SUCCEEDED(StringCchPrintfW(pszDebugName, MAX_PATH, L"stGPU[%u].m_pICMDList", i)))
				{
					stGPU[i].m_pICMDList->SetName(pszDebugName);
				}
			}

			// 主显卡上的命令列表
			//前处理命令列表
			GRS_THROW_IF_FAILED(stGPU[c_nMainGPU].m_pID3D12Device4->CreateCommandAllocator(
				D3D12_COMMAND_LIST_TYPE_DIRECT
				, IID_PPV_ARGS(&pICMDAllocBefore)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pICMDAllocBefore);

			GRS_THROW_IF_FAILED(stGPU[c_nMainGPU].m_pID3D12Device4->CreateCommandList(
				0, D3D12_COMMAND_LIST_TYPE_DIRECT
				, pICMDAllocBefore.Get(), nullptr, IID_PPV_ARGS(&pICMDListBefore)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pICMDListBefore);

			//后处理命令列表
			GRS_THROW_IF_FAILED(stGPU[c_nMainGPU].m_pID3D12Device4->CreateCommandAllocator(
				D3D12_COMMAND_LIST_TYPE_DIRECT
				, IID_PPV_ARGS(&pICMDAllocLater)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pICMDAllocLater);
			GRS_THROW_IF_FAILED(stGPU[c_nMainGPU].m_pID3D12Device4->CreateCommandList(
				0, D3D12_COMMAND_LIST_TYPE_DIRECT
				, pICMDAllocLater.Get(), nullptr, IID_PPV_ARGS(&pICMDListLater)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pICMDListLater);

			//复制命令列表
			GRS_THROW_IF_FAILED(stGPU[c_nMainGPU].m_pID3D12Device4->CreateCommandAllocator(
				D3D12_COMMAND_LIST_TYPE_COPY
				, IID_PPV_ARGS(&pICMDAllocCopy)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pICMDAllocCopy);
			GRS_THROW_IF_FAILED(stGPU[c_nMainGPU].m_pID3D12Device4->CreateCommandList(
				0, D3D12_COMMAND_LIST_TYPE_COPY
				, pICMDAllocCopy.Get(), nullptr, IID_PPV_ARGS(&pICMDListCopy)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pICMDListCopy);

			// 辅助显卡上的命令列表
			// Third Pass 命令列表（辅助显卡做后处理，创建在辅助显卡上）
			GRS_THROW_IF_FAILED(stGPU[c_nSecondGPU].m_pID3D12Device4->CreateCommandAllocator(
				D3D12_COMMAND_LIST_TYPE_DIRECT
				, IID_PPV_ARGS(&pICMDAllocPostPass)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pICMDAllocPostPass);
			GRS_THROW_IF_FAILED(stGPU[c_nSecondGPU].m_pID3D12Device4->CreateCommandList(
				0, D3D12_COMMAND_LIST_TYPE_DIRECT
				, pICMDAllocPostPass.Get(), nullptr, IID_PPV_ARGS(&pICMDListPostPass)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pICMDListPostPass);
		}

		//6、创建围栏对象，以及多GPU同步围栏对象
		{
			for (int i = 0; i < c_nMaxGPUCnt; i++)
			{
				GRS_THROW_IF_FAILED(
					stGPU[i].m_pID3D12Device4->CreateFence(
						0
						, D3D12_FENCE_FLAG_NONE
						, IID_PPV_ARGS(&stGPU[i].m_pIFence)));

				stGPU[i].m_hEventFence = CreateEvent(nullptr, FALSE, FALSE, nullptr);
				if (stGPU[i].m_hEventFence == nullptr)
				{
					GRS_THROW_IF_FAILED(HRESULT_FROM_WIN32(GetLastError()));
				}
			}

			// 在主显卡上创建一个可共享的围栏对象
			GRS_THROW_IF_FAILED(stGPU[c_nMainGPU].m_pID3D12Device4->CreateFence(0
				, D3D12_FENCE_FLAG_SHARED | D3D12_FENCE_FLAG_SHARED_CROSS_ADAPTER
				, IID_PPV_ARGS(&stGPU[c_nMainGPU].m_pISharedFence)));

			// 共享这个围栏，通过句柄方式
			HANDLE hFenceShared = nullptr;
			GRS_THROW_IF_FAILED(stGPU[c_nMainGPU].m_pID3D12Device4->CreateSharedHandle(
				stGPU[c_nMainGPU].m_pISharedFence.Get(),
				nullptr,
				GENERIC_ALL,
				nullptr,
				&hFenceShared));

			// 在辅助显卡上打开这个围栏对象完成共享
			HRESULT hrOpenSharedHandleResult
				= stGPU[c_nSecondGPU].m_pID3D12Device4->OpenSharedHandle(
					hFenceShared
					, IID_PPV_ARGS(&stGPU[c_nSecondGPU].m_pISharedFence));

			// 先关闭句柄，再判定是否共享成功
			::CloseHandle(hFenceShared);
			GRS_THROW_IF_FAILED(hrOpenSharedHandleResult);
		}

		//7、创建交换链，创建共享堆
		{
			DXGI_SWAP_CHAIN_DESC1 stSwapChainDesc = {};
			stSwapChainDesc.BufferCount = g_nFrameBackBufCount;
			stSwapChainDesc.Width = g_iWndWidth;
			stSwapChainDesc.Height = g_iWndHeight;
			stSwapChainDesc.Format = emRTFormat;
			stSwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			stSwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
			stSwapChainDesc.SampleDesc.Count = 1;

			//在第二个显卡上Present
			GRS_THROW_IF_FAILED(pIDXGIFactory5->CreateSwapChainForHwnd(
				stGPU[c_nSecondGPU].m_pICMDQueue.Get(),		// Swap chain needs the queue so that it can force a flush on it.
				hWnd,
				&stSwapChainDesc,
				nullptr,
				nullptr,
				&pISwapChain1
			));
			GRS_SET_DXGI_DEBUGNAME_COMPTR(pISwapChain1);

			//注意此处使用了高版本的SwapChain接口的函数
			GRS_THROW_IF_FAILED(pISwapChain1.As(&pISwapChain3));
			GRS_SET_DXGI_DEBUGNAME_COMPTR(pISwapChain3);

			// 获取当前第一个供绘制的后缓冲序号
			nCurrentFrameIndex = pISwapChain3->GetCurrentBackBufferIndex();

			//创建RTV(渲染目标视图)描述符堆(这里堆的含义应当理解为数组或者固定大小元素的固定大小显存池)
			D3D12_DESCRIPTOR_HEAP_DESC stRTVHeapDesc = {};
			stRTVHeapDesc.NumDescriptors = g_nFrameBackBufCount;
			stRTVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
			stRTVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

			for (int i = 0; i < c_nMaxGPUCnt; i++)
			{
				GRS_THROW_IF_FAILED(stGPU[i].m_pID3D12Device4->CreateDescriptorHeap(&stRTVHeapDesc, IID_PPV_ARGS(&stGPU[i].m_pIRTVHeap)));
				GRS_SetD3D12DebugNameIndexed(stGPU[i].m_pIRTVHeap.Get(), _T("m_pIRTVHeap"), i);
			}

			for (int i = 0; i < c_nPostPassCnt; i++)
			{
				//Off-Line Render Target View
				GRS_THROW_IF_FAILED(stGPU[c_nSecondGPU].m_pID3D12Device4->CreateDescriptorHeap(
					&stRTVHeapDesc, IID_PPV_ARGS(&pIRTVOffLine[i])));
				GRS_SET_D3D12_DEBUGNAME_COMPTR(pIRTVOffLine[i]);

			}

			//创建渲染目标描述符，以及主显卡的渲染目标资源
			CD3DX12_CLEAR_VALUE   stClearValue(stSwapChainDesc.Format, v4ClearColor);
			stRenderTargetDesc = CD3DX12_RESOURCE_DESC::Tex2D(
				stSwapChainDesc.Format,
				stSwapChainDesc.Width,
				stSwapChainDesc.Height,
				1u, 1u,
				stSwapChainDesc.SampleDesc.Count,
				stSwapChainDesc.SampleDesc.Quality,
				D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
				D3D12_TEXTURE_LAYOUT_UNKNOWN, 0u);

			WCHAR pszDebugName[MAX_PATH] = {};

			CD3DX12_CPU_DESCRIPTOR_HANDLE stRTVHandleMainGPU(stGPU[c_nMainGPU].m_pIRTVHeap->GetCPUDescriptorHandleForHeapStart());
			CD3DX12_CPU_DESCRIPTOR_HANDLE stRTVHandleSecondGPU(stGPU[c_nSecondGPU].m_pIRTVHeap->GetCPUDescriptorHandleForHeapStart());
			for (UINT j = 0; j < g_nFrameBackBufCount; j++)
			{
				//在辅助显卡上输出
				GRS_THROW_IF_FAILED(pISwapChain3->GetBuffer(j, IID_PPV_ARGS(&stGPU[c_nSecondGPU].m_pIRTRes[j])));
				//在主显卡上创建渲染目标纹理资源
				GRS_THROW_IF_FAILED(stGPU[c_nMainGPU].m_pID3D12Device4->CreateCommittedResource(
					&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
					D3D12_HEAP_FLAG_NONE,
					&stRenderTargetDesc,
					D3D12_RESOURCE_STATE_COMMON,
					&stClearValue,
					IID_PPV_ARGS(&stGPU[c_nMainGPU].m_pIRTRes[j])));

				stGPU[c_nMainGPU].m_pID3D12Device4->CreateRenderTargetView(stGPU[c_nMainGPU].m_pIRTRes[j].Get(), nullptr, stRTVHandleMainGPU);
				stRTVHandleMainGPU.Offset(1, stGPU[c_nMainGPU].m_nRTVDescriptorSize);

				stGPU[c_nSecondGPU].m_pID3D12Device4->CreateRenderTargetView(stGPU[c_nSecondGPU].m_pIRTRes[j].Get(), nullptr, stRTVHandleSecondGPU);
				stRTVHandleSecondGPU.Offset(1, stGPU[c_nSecondGPU].m_nRTVDescriptorSize);
			}

			for (int i = 0; i < c_nMaxGPUCnt; i++)
			{
				// 创建每个显卡上的深度蜡板缓冲区
				D3D12_CLEAR_VALUE stDepthOptimizedClearValue = {};
				stDepthOptimizedClearValue.Format = emDSFormat;
				stDepthOptimizedClearValue.DepthStencil.Depth = 1.0f;
				stDepthOptimizedClearValue.DepthStencil.Stencil = 0;

				//使用隐式默认堆创建一个深度蜡板缓冲区，
				//因为基本上深度缓冲区会一直被使用，重用的意义不大
				//所以直接使用隐式堆，图方便
				GRS_THROW_IF_FAILED(stGPU[i].m_pID3D12Device4->CreateCommittedResource(
					&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT)
					, D3D12_HEAP_FLAG_NONE
					, &CD3DX12_RESOURCE_DESC::Tex2D(
						emDSFormat
						, g_iWndWidth
						, g_iWndHeight
						, 1
						, 0
						, 1
						, 0
						, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
					, D3D12_RESOURCE_STATE_DEPTH_WRITE
					, &stDepthOptimizedClearValue
					, IID_PPV_ARGS(&stGPU[i].m_pIDSRes)
				));
				GRS_SetD3D12DebugNameIndexed(stGPU[i].m_pIDSRes.Get(), _T("m_pIDSRes"), i);

				// Create DSV Heap
				D3D12_DESCRIPTOR_HEAP_DESC stDSVHeapDesc = {};
				stDSVHeapDesc.NumDescriptors = 1;
				stDSVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
				stDSVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
				GRS_THROW_IF_FAILED(stGPU[i].m_pID3D12Device4->CreateDescriptorHeap(&stDSVHeapDesc, IID_PPV_ARGS(&stGPU[i].m_pIDSVHeap)));
				GRS_SetD3D12DebugNameIndexed(stGPU[i].m_pIDSVHeap.Get(), _T("m_pIDSVHeap"), i);

				// Create DSV
				D3D12_DEPTH_STENCIL_VIEW_DESC stDepthStencilDesc = {};
				stDepthStencilDesc.Format = emDSFormat;
				stDepthStencilDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
				stDepthStencilDesc.Flags = D3D12_DSV_FLAG_NONE;

				stGPU[i].m_pID3D12Device4->CreateDepthStencilView(stGPU[i].m_pIDSRes.Get()
					, &stDepthStencilDesc
					, stGPU[i].m_pIDSVHeap->GetCPUDescriptorHandleForHeapStart());
			}

			for (UINT k = 0; k < c_nPostPassCnt; k++)
			{
				CD3DX12_CPU_DESCRIPTOR_HANDLE stHRTVOffLine(pIRTVOffLine[k]->GetCPUDescriptorHandleForHeapStart());
				for (int i = 0; i < g_nFrameBackBufCount; i++)
				{
					// Create Second Adapter Off-Line Render Target
					GRS_THROW_IF_FAILED(stGPU[c_nSecondGPU].m_pID3D12Device4->CreateCommittedResource(
						&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
						D3D12_HEAP_FLAG_NONE,
						&stRenderTargetDesc,
						D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
						&stClearValue,
						IID_PPV_ARGS(&pIOffLineRTRes[k][i])));

					GRS_SET_D3D12_DEBUGNAME_INDEXED_COMPTR(pIOffLineRTRes[k], i);
					// Off-Line Render Target View
					stGPU[c_nSecondGPU].m_pID3D12Device4->CreateRenderTargetView(pIOffLineRTRes[k][i].Get(), nullptr, stHRTVOffLine);
					stHRTVOffLine.Offset(1, stGPU[c_nSecondGPU].m_nRTVDescriptorSize);
				}
			}


			// 创建跨适配器的共享资源堆
			{
				D3D12_FEATURE_DATA_D3D12_OPTIONS stOptions = {};
				// 通过检测带有显示输出的显卡是否支持跨显卡资源来决定跨显卡的资源如何创建
				GRS_THROW_IF_FAILED(
					stGPU[c_nSecondGPU].m_pID3D12Device4->CheckFeatureSupport(
						D3D12_FEATURE_D3D12_OPTIONS
						, reinterpret_cast<void*>(&stOptions)
						, sizeof(stOptions)));

				bCrossAdapterTextureSupport = stOptions.CrossAdapterRowMajorTextureSupported;

				UINT64 n64szTexture = 0;
				D3D12_RESOURCE_DESC stCrossAdapterResDesc = {};

				if (bCrossAdapterTextureSupport)
				{
					::OutputDebugString(_T("\n辅助显卡直接支持跨显卡共享纹理!\n"));
					// 如果支持那么直接创建跨显卡纹理类型的资源堆
					// 纹理形式共享的时候，纹理必须是行主续存储的
					stCrossAdapterResDesc = stRenderTargetDesc;
					stCrossAdapterResDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER;
					stCrossAdapterResDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

					D3D12_RESOURCE_ALLOCATION_INFO stTextureInfo
						= stGPU[c_nMainGPU].m_pID3D12Device4->GetResourceAllocationInfo(
							0, 1, &stCrossAdapterResDesc);
					n64szTexture = stTextureInfo.SizeInBytes;
				}
				else
				{
					::OutputDebugString(_T("\n辅助显卡不支持跨显卡共享纹理，接着创建辅助纹理。\n"));
					// 如果不支持纹理类型的资源堆共享，那么就创建Buffer类型的共享资源堆
					D3D12_PLACED_SUBRESOURCE_FOOTPRINT stResLayout = {};
					stGPU[c_nMainGPU].m_pID3D12Device4->GetCopyableFootprints(
						&stRenderTargetDesc, 0, 1, 0, &stResLayout, nullptr, nullptr, nullptr);
					//普通资源字节尺寸大小要64k边界对齐
					n64szTexture = GRS_UPPER(stResLayout.Footprint.RowPitch * stResLayout.Footprint.Height
						, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
					stCrossAdapterResDesc
						= CD3DX12_RESOURCE_DESC::Buffer(n64szTexture, D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER);
				}

				// 创建跨显卡共享的资源堆
				// 这里需要注意的就是，不管怎样，最差情况下两个显卡间至少都支持Buffer类型的资源共享
				// 这里可以理解为共享内存总是可以在任意显卡间共享
				// 只是最普通的方式是以Buffer形式共享，GPU不能当做纹理来访问
				CD3DX12_HEAP_DESC stCrossHeapDesc(
					n64szTexture * g_nFrameBackBufCount,
					D3D12_HEAP_TYPE_DEFAULT,
					0,
					D3D12_HEAP_FLAG_SHARED | D3D12_HEAP_FLAG_SHARED_CROSS_ADAPTER);

				GRS_THROW_IF_FAILED(stGPU[c_nMainGPU].m_pID3D12Device4->CreateHeap(&stCrossHeapDesc
					, IID_PPV_ARGS(&stGPU[c_nMainGPU].m_pICrossAdapterHeap)));

				HANDLE hHeap = nullptr;
				GRS_THROW_IF_FAILED(stGPU[c_nMainGPU].m_pID3D12Device4->CreateSharedHandle(
					stGPU[c_nMainGPU].m_pICrossAdapterHeap.Get(),
					nullptr,
					GENERIC_ALL,
					nullptr,
					&hHeap));

				HRESULT hrOpenSharedHandle = stGPU[c_nSecondGPU].m_pID3D12Device4->OpenSharedHandle(
					hHeap
					, IID_PPV_ARGS(&stGPU[c_nSecondGPU].m_pICrossAdapterHeap));

				// 先关闭句柄，再判定是否共享成功
				::CloseHandle(hHeap);

				GRS_THROW_IF_FAILED(hrOpenSharedHandle);

				// 以定位方式在共享堆上创建每个显卡上的资源
				for (UINT i = 0; i < g_nFrameBackBufCount; i++)
				{
					GRS_THROW_IF_FAILED(stGPU[c_nMainGPU].m_pID3D12Device4->CreatePlacedResource(
						stGPU[c_nMainGPU].m_pICrossAdapterHeap.Get(),
						n64szTexture * i,
						&stCrossAdapterResDesc,
						D3D12_RESOURCE_STATE_COPY_DEST,
						nullptr,
						IID_PPV_ARGS(&stGPU[c_nMainGPU].m_pICrossAdapterResPerFrame[i])));

					GRS_THROW_IF_FAILED(stGPU[c_nSecondGPU].m_pID3D12Device4->CreatePlacedResource(
						stGPU[c_nSecondGPU].m_pICrossAdapterHeap.Get(),
						n64szTexture * i,
						&stCrossAdapterResDesc,
						bCrossAdapterTextureSupport ? D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE : D3D12_RESOURCE_STATE_COPY_SOURCE,
						nullptr,
						IID_PPV_ARGS(&stGPU[c_nSecondGPU].m_pICrossAdapterResPerFrame[i])));

					if (!bCrossAdapterTextureSupport)
					{
						// 如果共享资源仅能共享Buffer形式的资源，那么就在第二个显卡上创建对应的纹理形式的资源
						// 并且将主显卡共享过来的Buffer里的渲染结果纹理，复制到第二个显卡的纹理形式的资源上
						GRS_THROW_IF_FAILED(stGPU[c_nSecondGPU].m_pID3D12Device4->CreateCommittedResource(
							&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
							D3D12_HEAP_FLAG_NONE,
							&stRenderTargetDesc,
							D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
							nullptr,
							IID_PPV_ARGS(&pISecondaryAdapterTexutrePerFrame[i])));
					}
				}

			}
		}

		//-----------------------------------------------------------------------------------------------------------
		// 创建D2D的 Render Target
		{{
			float fDPIx = DEFAULT_DPI;
			float fDPIy = DEFAULT_DPI;
			// 这里需要注意 现在GetDesktopDpi方法已经被否决了，不能再调用了
			// 之前的注释有问题，设置成0会出错，我们这里改成了Windows系统默认的96
			//DisplayInformation::LogicalDpi for Windows Store Apps or GetDpiForWindow for desktop apps.
			//UINT nDPI = ::GetDpiForWindow(hWnd);
			//pID2D1Factory7->GetDesktopDpi(&fDPIx, &fDPIy);
			D2D1_BITMAP_PROPERTIES1 stD2DBMPProperties
				= D2D1::BitmapProperties1(
					D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW
					, D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED)
					, fDPIx
					, fDPIy
				);
				
			for ( UINT n = 0; n < g_nFrameBackBufCount; n++)
			{
				D3D11_RESOURCE_FLAGS stD3D11Flags = { D3D11_BIND_RENDER_TARGET };
				GRS_THROW_IF_FAILED(pID3D11On12Device1->CreateWrappedResource(
					stGPU[c_nSecondGPU].m_pIRTRes[n].Get(),
					&stD3D11Flags,
					D3D12_RESOURCE_STATE_RENDER_TARGET,
					D3D12_RESOURCE_STATE_PRESENT,
					IID_PPV_ARGS(&pID3D11WrappedBackBuffers[n])
				));

				ComPtr<IDXGISurface> pIDXGISurface;
				GRS_THROW_IF_FAILED(pID3D11WrappedBackBuffers[n].As(&pIDXGISurface));
				GRS_THROW_IF_FAILED(pID2D1DeviceContext6->CreateBitmapFromDxgiSurface(
					pIDXGISurface.Get(),
					&stD2DBMPProperties,
					&pID2DRenderTargets[n]
				));
			}
		}}

		// 创建DWrite的字体对象
		{
			// 创建一个画刷，字体输出时即使用该颜色画刷
			GRS_THROW_IF_FAILED(pID2D1DeviceContext6->CreateSolidColorBrush(
				D2D1::ColorF(D2D1::ColorF::Gold)
				, &pID2D1SolidColorBrush));
			GRS_THROW_IF_FAILED(pIDWriteFactory->CreateTextFormat(
				L"微软楷体",
				NULL,
				DWRITE_FONT_WEIGHT_NORMAL,
				DWRITE_FONT_STYLE_NORMAL,
				DWRITE_FONT_STRETCH_NORMAL,
				20,
				L"zh-cn", //中文字库
				&pIDWriteTextFormat
			));
			// 水平方向左对齐、垂直方向居中
			GRS_THROW_IF_FAILED(pIDWriteTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_JUSTIFIED));
			GRS_THROW_IF_FAILED(pIDWriteTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER));
		}
		//-----------------------------------------------------------------------------------------------------------
		
		//8、创建根签名
		{ {
				//First Pass Root Signature
				//这个例子中，第一遍渲染由主显卡完成，所有物体使用相同的根签名，因为渲染过程中需要的参数是一样的
				D3D12_FEATURE_DATA_ROOT_SIGNATURE stFeatureData = {};
				//检测是否支持V1.1版本的根签名
				stFeatureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

				if (FAILED(stGPU[c_nMainGPU].m_pID3D12Device4->CheckFeatureSupport(
					D3D12_FEATURE_ROOT_SIGNATURE
					, &stFeatureData
					, sizeof(stFeatureData))))
				{
					stFeatureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
				}

				CD3DX12_DESCRIPTOR_RANGE1 stDSPRanges[3];
				stDSPRanges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0); //1 Const Buffer View
				stDSPRanges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0); //1 Texture View
				stDSPRanges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0);// 1 Sampler

				CD3DX12_ROOT_PARAMETER1 stRootParameters[3];
				stRootParameters[0].InitAsDescriptorTable(1, &stDSPRanges[0], D3D12_SHADER_VISIBILITY_ALL); //CBV是所有Shader可见
				stRootParameters[1].InitAsDescriptorTable(1, &stDSPRanges[1], D3D12_SHADER_VISIBILITY_PIXEL);//SRV仅PS可见
				stRootParameters[2].InitAsDescriptorTable(1, &stDSPRanges[2], D3D12_SHADER_VISIBILITY_PIXEL);//SAMPLE仅PS可见

				CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC stRootSignatureDesc;
				stRootSignatureDesc.Init_1_1(_countof(stRootParameters)
					, stRootParameters
					, 0
					, nullptr
					, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

				ComPtr<ID3DBlob> pISignatureBlob;
				ComPtr<ID3DBlob> pIErrorBlob;

				GRS_THROW_IF_FAILED(D3DX12SerializeVersionedRootSignature(&stRootSignatureDesc
					, stFeatureData.HighestVersion
					, &pISignatureBlob
					, &pIErrorBlob));

				GRS_THROW_IF_FAILED(
					stGPU[c_nMainGPU].m_pID3D12Device4->CreateRootSignature(0
						, pISignatureBlob->GetBufferPointer()
						, pISignatureBlob->GetBufferSize()
						, IID_PPV_ARGS(&stGPU[c_nMainGPU].m_pIRS)));

				GRS_SET_D3D12_DEBUGNAME_COMPTR(stGPU[c_nMainGPU].m_pIRS);

				//Second Pass Root Signature
				//创建渲染Quad的根签名对象，注意这个根签名在辅助显卡上用
				// 因为我们把所有的渲染目标纹理的SRV都创建在了一个SRV堆的连续位置上，实际只使用一个，所以要和Noise Texture分离一下
				CD3DX12_DESCRIPTOR_RANGE1 stDRQuad[4];
				stDRQuad[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);	// 1 Const Buffer
				stDRQuad[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);    // 1 Texture 
				stDRQuad[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);    // 1 Noise Texture 
				stDRQuad[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0);// 1 Sampler

				//后处理其实就是图像处理，所以资源仅设定Pixel Shader可见
				CD3DX12_ROOT_PARAMETER1 stRPQuad[4];
				stRPQuad[0].InitAsDescriptorTable(1, &stDRQuad[0], D3D12_SHADER_VISIBILITY_PIXEL);//CBV仅PS可见
				stRPQuad[1].InitAsDescriptorTable(1, &stDRQuad[1], D3D12_SHADER_VISIBILITY_PIXEL);//SRV仅PS可见
				stRPQuad[2].InitAsDescriptorTable(1, &stDRQuad[2], D3D12_SHADER_VISIBILITY_PIXEL);//SAMPLE仅PS可见
				stRPQuad[3].InitAsDescriptorTable(1, &stDRQuad[3], D3D12_SHADER_VISIBILITY_PIXEL);//SAMPLE仅PS可见

				CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC stRSQuadDesc;
				stRSQuadDesc.Init_1_1(_countof(stRPQuad)
					, stRPQuad
					, 0
					, nullptr
					, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

				pISignatureBlob.Reset();
				pIErrorBlob.Reset();

				GRS_THROW_IF_FAILED(D3DX12SerializeVersionedRootSignature(&stRSQuadDesc
					, stFeatureData.HighestVersion
					, &pISignatureBlob
					, &pIErrorBlob));

				GRS_THROW_IF_FAILED(
					stGPU[c_nSecondGPU].m_pID3D12Device4->CreateRootSignature(0
						, pISignatureBlob->GetBufferPointer()
						, pISignatureBlob->GetBufferSize()
						, IID_PPV_ARGS(&stGPU[c_nSecondGPU].m_pIRS)));

				GRS_SET_D3D12_DEBUGNAME_COMPTR(stGPU[c_nSecondGPU].m_pIRS);

				//-----------------------------------------------------------------------------------------------------
				// Create Third Pass Root Signature
				CD3DX12_DESCRIPTOR_RANGE1 stDRThridPass[2];
				stDRThridPass[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
				stDRThridPass[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0);

				CD3DX12_ROOT_PARAMETER1 stRPThirdPass[2];
				stRPThirdPass[0].InitAsDescriptorTable(1, &stDRThridPass[0], D3D12_SHADER_VISIBILITY_PIXEL);	//CBV所有Shader可见
				stRPThirdPass[1].InitAsDescriptorTable(1, &stDRThridPass[1], D3D12_SHADER_VISIBILITY_PIXEL);	//SRV仅PS可见

				CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC stRSThirdPass;
				stRSThirdPass.Init_1_1(_countof(stRPThirdPass)
					, stRPThirdPass
					, 0
					, nullptr
					, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

				pISignatureBlob.Reset();
				pIErrorBlob.Reset();

				GRS_THROW_IF_FAILED(D3DX12SerializeVersionedRootSignature(&stRSThirdPass
					, stFeatureData.HighestVersion
					, &pISignatureBlob
					, &pIErrorBlob));

				GRS_THROW_IF_FAILED(stGPU[c_nSecondGPU].m_pID3D12Device4->CreateRootSignature(0
					, pISignatureBlob->GetBufferPointer()
					, pISignatureBlob->GetBufferSize()
					, IID_PPV_ARGS(&pIRSPostPass)));

				GRS_SET_D3D12_DEBUGNAME_COMPTR(pIRSPostPass);
			}}

		//9、编译Shader创建渲染管线状态对象
		{ {
#if defined(_DEBUG)
				// Enable better shader debugging with the graphics debugging tools.
				UINT nShaderCompileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
				UINT nShaderCompileFlags = 0;
#endif
				//编译为行矩阵形式
				nShaderCompileFlags |= D3DCOMPILE_PACK_MATRIX_ROW_MAJOR;

				TCHAR pszShaderFileName[MAX_PATH] = {};

				ComPtr<ID3DBlob> pIVSCode;
				ComPtr<ID3DBlob> pIPSCode;
				ComPtr<ID3DBlob> pIErrMsg;
				CHAR pszErrMsg[MAX_PATH] = {};
				HRESULT hr = S_OK;

				StringCchPrintf(pszShaderFileName
					, MAX_PATH
					, _T("%s12-D2DWriteOnD3D12\\Shader\\12-D2DWriteOnD3D12.hlsl")
					, g_pszAppPath);

				hr = D3DCompileFromFile(pszShaderFileName, nullptr, nullptr
					, "VSMain", "vs_5_0", nShaderCompileFlags, 0, &pIVSCode, &pIErrMsg);
				if (FAILED(hr))
				{
					StringCchPrintfA(pszErrMsg
						, MAX_PATH
						, "\n%s\n"
						, (CHAR*)pIErrMsg->GetBufferPointer());
					::OutputDebugStringA(pszErrMsg);
					throw CGRSCOMException(hr);
				}

				pIErrMsg.Reset();

				hr = D3DCompileFromFile(pszShaderFileName, nullptr, nullptr
					, "PSMain", "ps_5_0", nShaderCompileFlags, 0, &pIPSCode, &pIErrMsg);
				if (FAILED(hr))
				{
					StringCchPrintfA(pszErrMsg
						, MAX_PATH
						, "\n%s\n"
						, (CHAR*)pIErrMsg->GetBufferPointer());
					::OutputDebugStringA(pszErrMsg);
					throw CGRSCOMException(hr);
				}

				// 我们多添加了一个法线的定义，但目前Shader中我们并没有使用
				D3D12_INPUT_ELEMENT_DESC stIALayoutSphere[] =
				{
					{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
					{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
					{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
				};

				// 创建 graphics pipeline state object (PSO)对象
				D3D12_GRAPHICS_PIPELINE_STATE_DESC stPSODesc = {};
				stPSODesc.InputLayout = { stIALayoutSphere, _countof(stIALayoutSphere) };
				stPSODesc.pRootSignature = stGPU[c_nMainGPU].m_pIRS.Get();
				stPSODesc.VS = CD3DX12_SHADER_BYTECODE(pIVSCode.Get());
				stPSODesc.PS = CD3DX12_SHADER_BYTECODE(pIPSCode.Get());
				stPSODesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				stPSODesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				stPSODesc.SampleMask = UINT_MAX;
				stPSODesc.SampleDesc.Count = 1;
				stPSODesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				stPSODesc.NumRenderTargets = 1;
				stPSODesc.RTVFormats[0] = emRTFormat;
				stPSODesc.DSVFormat = emDSFormat;
				stPSODesc.DepthStencilState.StencilEnable = FALSE;
				stPSODesc.DepthStencilState.DepthEnable = TRUE;			//打开深度缓冲				
				stPSODesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;//启用深度缓存写入功能
				stPSODesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;     //深度测试函数（该值为普通的深度测试，即较小值写入）

				GRS_THROW_IF_FAILED(
					stGPU[c_nMainGPU].m_pID3D12Device4->CreateGraphicsPipelineState(
						&stPSODesc
						, IID_PPV_ARGS(&stGPU[c_nMainGPU].m_pIPSO)));

				GRS_SET_D3D12_DEBUGNAME_COMPTR(stGPU[c_nMainGPU].m_pIPSO);

				// Second Pass Pipeline State Object
				// 用于渲染单位矩形的管道状态对象，在本例中是后处理
				StringCchPrintf(pszShaderFileName, MAX_PATH, _T("%s12-D2DWriteOnD3D12\\Shader\\12-QuadVS.hlsl"), g_pszAppPath);

				pIVSCode.Reset();
				pIPSCode.Reset();
				pIErrMsg.Reset();

				hr = D3DCompileFromFile(pszShaderFileName, nullptr, nullptr
					, "VSMain", "vs_5_0", nShaderCompileFlags, 0, &pIVSCode, &pIErrMsg);
				if (FAILED(hr))
				{
					if (nullptr != pIErrMsg)
					{
						StringCchPrintfA(pszErrMsg, MAX_PATH, "\n%s\n", (CHAR*)pIErrMsg->GetBufferPointer());
						::OutputDebugStringA(pszErrMsg);
					}
					throw CGRSCOMException(hr);
				}

				pIErrMsg.Reset();
				StringCchPrintf(pszShaderFileName, MAX_PATH
					, _T("%s12-D2DWriteOnD3D12\\Shader\\12-WaterColourPS.hlsl")
					, g_pszAppPath);

				hr = D3DCompileFromFile(pszShaderFileName, nullptr, nullptr
					, "PSMain", "ps_5_0", nShaderCompileFlags, 0, &pIPSCode, &pIErrMsg);
				if (FAILED(hr))
				{
					if (nullptr != pIErrMsg)
					{
						StringCchPrintfA(pszErrMsg, MAX_PATH, "\n%s\n", (CHAR*)pIErrMsg->GetBufferPointer());
						::OutputDebugStringA(pszErrMsg);
					}
					throw CGRSCOMException(hr);
				}
				// 我们多添加了一个法线的定义，但目前Shader中我们并没有使用
				D3D12_INPUT_ELEMENT_DESC stIALayoutQuad[] =
				{
					{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
					{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
				};

				// 创建 graphics pipeline state object (PSO)对象
				D3D12_GRAPHICS_PIPELINE_STATE_DESC stPSOQuadDesc = {};
				stPSOQuadDesc.InputLayout = { stIALayoutQuad, _countof(stIALayoutQuad) };
				stPSOQuadDesc.pRootSignature = stGPU[c_nSecondGPU].m_pIRS.Get();
				stPSOQuadDesc.VS = CD3DX12_SHADER_BYTECODE(pIVSCode.Get());
				stPSOQuadDesc.PS = CD3DX12_SHADER_BYTECODE(pIPSCode.Get());
				stPSOQuadDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				stPSOQuadDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				stPSOQuadDesc.SampleMask = UINT_MAX;
				stPSOQuadDesc.SampleDesc.Count = 1;
				stPSOQuadDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				stPSOQuadDesc.NumRenderTargets = 1;
				stPSOQuadDesc.RTVFormats[0] = emRTFormat;
				stPSOQuadDesc.DepthStencilState.StencilEnable = FALSE;
				stPSOQuadDesc.DepthStencilState.DepthEnable = FALSE;

				GRS_THROW_IF_FAILED(
					stGPU[c_nSecondGPU].m_pID3D12Device4->CreateGraphicsPipelineState(
						&stPSOQuadDesc
						, IID_PPV_ARGS(&stGPU[c_nSecondGPU].m_pIPSO)));
				GRS_SET_D3D12_DEBUGNAME_COMPTR(stGPU[c_nSecondGPU].m_pIPSO);

				//---------------------------------------------------------------------------------------------------
				// Create Gaussian blur in the vertical direction PSO
				pIPSCode.Reset();
				pIErrMsg.Reset();

				StringCchPrintf(pszShaderFileName
					, MAX_PATH
					, _T("%s12-D2DWriteOnD3D12\\Shader\\12-GaussianBlurPS.hlsl")
					, g_pszAppPath);

				//后处理渲染，只编译PS就可以了，VS就用之前的QuadVS即可，都是后处理主要玩PS
				hr = D3DCompileFromFile(pszShaderFileName, nullptr, nullptr
					, "PSSimpleBlurV", "ps_5_0", nShaderCompileFlags, 0, &pIPSCode, &pIErrMsg);
				if (FAILED(hr))
				{
					if (nullptr != pIErrMsg)
					{
						StringCchPrintfA(pszErrMsg, MAX_PATH, "\n%s\n", (CHAR*)pIErrMsg->GetBufferPointer());
						::OutputDebugStringA(pszErrMsg);
					}
					throw CGRSCOMException(hr);
				}

				// 创建 graphics pipeline state object (PSO)对象
				D3D12_GRAPHICS_PIPELINE_STATE_DESC stPSOThirdPassDesc = {};
				stPSOThirdPassDesc.InputLayout = { stIALayoutQuad, _countof(stIALayoutQuad) };
				stPSOThirdPassDesc.pRootSignature = pIRSPostPass.Get();
				stPSOThirdPassDesc.VS = CD3DX12_SHADER_BYTECODE(pIVSCode.Get());
				stPSOThirdPassDesc.PS = CD3DX12_SHADER_BYTECODE(pIPSCode.Get());
				stPSOThirdPassDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				stPSOThirdPassDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				stPSOThirdPassDesc.SampleMask = UINT_MAX;
				stPSOThirdPassDesc.SampleDesc.Count = 1;
				stPSOThirdPassDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				stPSOThirdPassDesc.NumRenderTargets = 1;
				stPSOThirdPassDesc.RTVFormats[0] = emRTFormat;
				stPSOThirdPassDesc.DepthStencilState.StencilEnable = FALSE;
				stPSOThirdPassDesc.DepthStencilState.DepthEnable = FALSE;

				GRS_THROW_IF_FAILED(
					stGPU[c_nSecondGPU].m_pID3D12Device4->CreateGraphicsPipelineState(
						&stPSOThirdPassDesc
						, IID_PPV_ARGS(&pIPSOPostPass[c_nPostPass0])));
				GRS_SET_D3D12_DEBUGNAME_COMPTR(pIPSOPostPass[c_nPostPass0]);

				//  Create Gaussian blur in the horizontal direction PSO
				pIPSCode.Reset();
				pIErrMsg.Reset();

				//后处理渲染，只编译PS就可以了，VS就用之前的QuadVS即可，都是后处理主要玩PS
				hr = D3DCompileFromFile(pszShaderFileName, nullptr, nullptr
					, "PSSimpleBlurU", "ps_5_0", nShaderCompileFlags, 0, &pIPSCode, &pIErrMsg);
				if (FAILED(hr))
				{
					if (nullptr != pIErrMsg)
					{
						StringCchPrintfA(pszErrMsg, MAX_PATH, "\n%s\n", (CHAR*)pIErrMsg->GetBufferPointer());
						::OutputDebugStringA(pszErrMsg);
					}
					throw CGRSCOMException(hr);
				}

				stPSOThirdPassDesc.PS = CD3DX12_SHADER_BYTECODE(pIPSCode.Get());

				GRS_THROW_IF_FAILED(
					stGPU[c_nSecondGPU].m_pID3D12Device4->CreateGraphicsPipelineState(
						&stPSOThirdPassDesc
						, IID_PPV_ARGS(&pIPSOPostPass[c_nPostPass1])));
				GRS_SET_D3D12_DEBUGNAME_COMPTR(pIPSOPostPass[c_nPostPass1]);
				//---------------------------------------------------------------------------------------------------

				// DoNothing PS PSO
				pIPSCode.Reset();
				pIErrMsg.Reset();

		}}

		//10、准备参数并启动多个渲染线程
		{ {
				USES_CONVERSION;
				// 球体个性参数
				StringCchPrintf(g_stThread[g_nThdSphere].m_pszDDSFile, MAX_PATH, _T("%sAssets\\Earth_512.dds"), g_pszAppPath);
				StringCchPrintfA(g_stThread[g_nThdSphere].m_pszMeshFile, MAX_PATH, "%sAssets\\sphere.txt", T2A(g_pszAppPath));
				g_stThread[g_nThdSphere].m_v4ModelPos = XMFLOAT4(2.0f, 2.0f, 0.0f, 1.0f);

				// 立方体个性参数
				StringCchPrintf(g_stThread[g_nThdCube].m_pszDDSFile, MAX_PATH, _T("%sAssets\\Cube.dds"), g_pszAppPath);
				StringCchPrintfA(g_stThread[g_nThdCube].m_pszMeshFile, MAX_PATH, "%sAssets\\Cube.txt", T2A(g_pszAppPath));
				g_stThread[g_nThdCube].m_v4ModelPos = XMFLOAT4(-2.0f, 2.0f, 0.0f, 1.0f);

				// 平板个性参数
				StringCchPrintf(g_stThread[g_nThdPlane].m_pszDDSFile, MAX_PATH, _T("%sAssets\\Plane.dds"), g_pszAppPath);
				StringCchPrintfA(g_stThread[g_nThdPlane].m_pszMeshFile, MAX_PATH, "%sAssets\\Plane.txt", T2A(g_pszAppPath));
				g_stThread[g_nThdPlane].m_v4ModelPos = XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f);

				// 多线程渲染时，第一遍渲染主要在主显卡上完成，所以为每个线程创建主显卡关联的命令列表
				// 物体的共性参数，也就是各线程的共性参数
				for (int i = 0; i < g_nMaxThread; i++)
				{
					g_stThread[i].m_nThreadIndex = i;		//记录序号

					//创建每个线程需要的命令列表和复制命令队列
					GRS_THROW_IF_FAILED(
						stGPU[c_nMainGPU].m_pID3D12Device4->CreateCommandAllocator(
							D3D12_COMMAND_LIST_TYPE_DIRECT
							, IID_PPV_ARGS(&g_stThread[i].m_pICMDAlloc)));
					GRS_SetD3D12DebugNameIndexed(g_stThread[i].m_pICMDAlloc, _T("pIThreadCmdAlloc"), i);

					GRS_THROW_IF_FAILED(
						stGPU[c_nMainGPU].m_pID3D12Device4->CreateCommandList(
							0, D3D12_COMMAND_LIST_TYPE_DIRECT
							, g_stThread[i].m_pICMDAlloc
							, nullptr
							, IID_PPV_ARGS(&g_stThread[i].m_pICMDList)));
					GRS_SetD3D12DebugNameIndexed(g_stThread[i].m_pICMDList, _T("pIThreadCmdList"), i);

					g_stThread[i].m_dwMainThreadID = ::GetCurrentThreadId();
					g_stThread[i].m_hMainThread = ::GetCurrentThread();
					g_stThread[i].m_hEventRun = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);
					g_stThread[i].m_hEventRenderOver = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);
					g_stThread[i].m_pID3D12Device4 = stGPU[c_nMainGPU].m_pID3D12Device4.Get();
					g_stThread[i].m_pIRTVHeap = stGPU[c_nMainGPU].m_pIRTVHeap.Get();
					g_stThread[i].m_pIDSVHeap = stGPU[c_nMainGPU].m_pIDSVHeap.Get();
					g_stThread[i].m_pIRS = stGPU[c_nMainGPU].m_pIRS.Get();
					g_stThread[i].m_pIPSO = stGPU[c_nMainGPU].m_pIPSO.Get();

					arHWaited.Add(g_stThread[i].m_hEventRenderOver); //添加到被等待队列里

					//以暂停方式创建线程
					g_stThread[i].m_hThisThread = (HANDLE)_beginthreadex(nullptr,
						0, RenderThread, (void*)&g_stThread[i],
						CREATE_SUSPENDED, (UINT*)&g_stThread[i].m_dwThisThreadID);

					//然后判断线程创建是否成功
					if (nullptr == g_stThread[i].m_hThisThread
						|| reinterpret_cast<HANDLE>(-1) == g_stThread[i].m_hThisThread)
					{
						throw CGRSCOMException(HRESULT_FROM_WIN32(GetLastError()));
					}

					arHSubThread.Add(g_stThread[i].m_hThisThread);
				}

				//逐一启动线程
				for (int i = 0; i < g_nMaxThread; i++)
				{
					::ResumeThread(g_stThread[i].m_hThisThread);
				}
			}}

		//11、加载DDS噪声纹理，注意用于后处理，所以加载到第二个显卡上
		{ {
				TCHAR pszNoiseTexture[MAX_PATH] = {};
				StringCchPrintf(pszNoiseTexture, MAX_PATH, _T("%sAssets\\GaussianNoise256.dds"), g_pszAppPath);
				std::unique_ptr<uint8_t[]>			pbDDSData;
				std::vector<D3D12_SUBRESOURCE_DATA> stArSubResources;
				DDS_ALPHA_MODE						emAlphaMode = DDS_ALPHA_MODE_UNKNOWN;
				bool								bIsCube = false;

				GRS_THROW_IF_FAILED(LoadDDSTextureFromFile(
					stGPU[c_nSecondGPU].m_pID3D12Device4.Get()
					, pszNoiseTexture
					, pINoiseTexture.GetAddressOf()
					, pbDDSData
					, stArSubResources
					, SIZE_MAX
					, &emAlphaMode
					, &bIsCube));
				GRS_SET_D3D12_DEBUGNAME_COMPTR(pINoiseTexture);

				UINT64 n64szUpSphere = GetRequiredIntermediateSize(
					pINoiseTexture.Get()
					, 0
					, static_cast<UINT>(stArSubResources.size()));

				D3D12_RESOURCE_DESC stTXDesc = pINoiseTexture->GetDesc();

				GRS_THROW_IF_FAILED(
					stGPU[c_nSecondGPU].m_pID3D12Device4->CreateCommittedResource(
						&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD)
						, D3D12_HEAP_FLAG_NONE
						, &CD3DX12_RESOURCE_DESC::Buffer(n64szUpSphere)
						, D3D12_RESOURCE_STATE_GENERIC_READ
						, nullptr
						, IID_PPV_ARGS(&pINoiseTextureUpload)));
				GRS_SET_D3D12_DEBUGNAME_COMPTR(pINoiseTextureUpload);

				//执行两个Copy动作将纹理上传到默认堆中
				UpdateSubresources(stGPU[c_nSecondGPU].m_pICMDList.Get()
					, pINoiseTexture.Get()
					, pINoiseTextureUpload.Get()
					, 0
					, 0
					, static_cast<UINT>(stArSubResources.size())
					, stArSubResources.data());

				//同步
				stGPU[c_nSecondGPU].m_pICMDList->ResourceBarrier(1
					, &CD3DX12_RESOURCE_BARRIER::Transition(pINoiseTexture.Get()
						, D3D12_RESOURCE_STATE_COPY_DEST
						, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

			}}
		//12、建辅助显卡用来渲染或后处理用的矩形框
		{
			ST_GRS_VERTEX_QUAD stVertexQuad[] =
			{	//(   x,     y,    z,    w   )  (  u,    v   )
				{ { -1.0f,  1.0f, 0.0f, 1.0f }, { 0.0f, 0.0f } },	// Top left.
				{ {  1.0f,  1.0f, 0.0f, 1.0f }, { 1.0f, 0.0f } },	// Top right.
				{ { -1.0f, -1.0f, 0.0f, 1.0f }, { 0.0f, 1.0f } },	// Bottom left.
				{ {  1.0f, -1.0f, 0.0f, 1.0f }, { 1.0f, 1.0f } },	// Bottom right.
			};

			const UINT nszVBQuad = sizeof(stVertexQuad);

			GRS_THROW_IF_FAILED(
				stGPU[c_nSecondGPU].m_pID3D12Device4->CreateCommittedResource(
					&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
					D3D12_HEAP_FLAG_NONE,
					&CD3DX12_RESOURCE_DESC::Buffer(nszVBQuad),
					D3D12_RESOURCE_STATE_COPY_DEST,
					nullptr,
					IID_PPV_ARGS(&pIVBQuad)));

			// 这次我们特意演示了如何将顶点缓冲上传到默认堆上的方式，与纹理上传默认堆实际是一样的
			GRS_THROW_IF_FAILED(
				stGPU[c_nSecondGPU].m_pID3D12Device4->CreateCommittedResource(
					&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
					D3D12_HEAP_FLAG_NONE,
					&CD3DX12_RESOURCE_DESC::Buffer(nszVBQuad),
					D3D12_RESOURCE_STATE_GENERIC_READ,
					nullptr,
					IID_PPV_ARGS(&pIVBQuadUpload)));

			D3D12_SUBRESOURCE_DATA stVBDataQuad = {};
			stVBDataQuad.pData = reinterpret_cast<UINT8*>(stVertexQuad);
			stVBDataQuad.RowPitch = nszVBQuad;
			stVBDataQuad.SlicePitch = stVBDataQuad.RowPitch;

			UpdateSubresources<1>(
				stGPU[c_nSecondGPU].m_pICMDList.Get()
				, pIVBQuad.Get()
				, pIVBQuadUpload.Get()
				, 0
				, 0
				, 1
				, &stVBDataQuad);

			stGPU[c_nSecondGPU].m_pICMDList->ResourceBarrier(
				1
				, &CD3DX12_RESOURCE_BARRIER::Transition(pIVBQuad.Get()
					, D3D12_RESOURCE_STATE_COPY_DEST
					, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

			pstVBVQuad.BufferLocation = pIVBQuad->GetGPUVirtualAddress();
			pstVBVQuad.StrideInBytes = sizeof(ST_GRS_VERTEX_QUAD);
			pstVBVQuad.SizeInBytes = sizeof(stVertexQuad);

			// 执行命令，将噪声纹理以及矩形顶点缓冲上传到第二个显卡的默认堆上
			GRS_THROW_IF_FAILED(stGPU[c_nSecondGPU].m_pICMDList->Close());
			ID3D12CommandList* ppRenderCommandLists[] = { stGPU[c_nSecondGPU].m_pICMDList.Get() };
			stGPU[c_nSecondGPU].m_pICMDQueue->ExecuteCommandLists(_countof(ppRenderCommandLists), ppRenderCommandLists);

			n64CurrentFenceValue = n64FenceValue;
			GRS_THROW_IF_FAILED(stGPU[c_nSecondGPU].m_pICMDQueue->Signal(
				stGPU[c_nSecondGPU].m_pIFence.Get()
				, n64CurrentFenceValue));

			n64FenceValue++;
			if (stGPU[c_nSecondGPU].m_pIFence->GetCompletedValue() < n64CurrentFenceValue)
			{
				GRS_THROW_IF_FAILED(
					stGPU[c_nSecondGPU].m_pIFence->SetEventOnCompletion(n64CurrentFenceValue, stGPU[c_nSecondGPU].m_hEventFence));
				WaitForSingleObject(stGPU[c_nSecondGPU].m_hEventFence, INFINITE);
			}

			//-----------------------------------华丽丽的割一下----------------------------------------------------------
			// 创建辅助显卡上的常量缓冲区
			GRS_THROW_IF_FAILED(stGPU[c_nSecondGPU].m_pID3D12Device4->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD)
				, D3D12_HEAP_FLAG_NONE
				, &CD3DX12_RESOURCE_DESC::Buffer(szSecondPassCB) //注意缓冲尺寸设置为256边界对齐大小
				, D3D12_RESOURCE_STATE_GENERIC_READ
				, nullptr
				, IID_PPV_ARGS(&pICBResSecondPass)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pICBResSecondPass);

			GRS_THROW_IF_FAILED(pICBResSecondPass->Map(0, nullptr, reinterpret_cast<void**>(&pstCBSecondPass)));

			// Create SRV Heap
			D3D12_DESCRIPTOR_HEAP_DESC stDescriptorHeapDesc = {};
			stDescriptorHeapDesc.NumDescriptors = g_nFrameBackBufCount + 2; //1 Const Bufer + 1 Texture + g_nFrameBackBufCount * Texture
			stDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			stDescriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

			GRS_THROW_IF_FAILED(stGPU[c_nSecondGPU].m_pID3D12Device4->CreateDescriptorHeap(
				&stDescriptorHeapDesc
				, IID_PPV_ARGS(&stGPU[c_nSecondGPU].m_pISRVHeap)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(stGPU[c_nSecondGPU].m_pISRVHeap);

			CD3DX12_CPU_DESCRIPTOR_HANDLE stSrvHandle(
				stGPU[c_nSecondGPU].m_pISRVHeap->GetCPUDescriptorHandleForHeapStart()
			);

			// Create Third Pass SRV Heap
			stDescriptorHeapDesc.NumDescriptors = g_nFrameBackBufCount;
			//为每一个渲染目标纹理创建一个SRV，这样只需要设置时便宜到对应SRV即可
			for (int i = 0; i < c_nPostPassCnt; i++)
			{
				GRS_THROW_IF_FAILED(
					stGPU[c_nSecondGPU].m_pID3D12Device4->CreateDescriptorHeap(
						&stDescriptorHeapDesc, IID_PPV_ARGS(&pISRVHeapPostPass[i])));
				GRS_SET_D3D12_DEBUGNAME_COMPTR(pISRVHeapPostPass[i]);
			}

			//-------------------------------------------------------------------------------------------------------
			//Create Cross Adapter Texture SRV
			D3D12_SHADER_RESOURCE_VIEW_DESC stSRVDesc = {};
			stSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			stSRVDesc.Format = emRTFormat;
			stSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			stSRVDesc.Texture2D.MipLevels = 1;

			for (int i = 0; i < g_nFrameBackBufCount; i++)
			{
				if (!bCrossAdapterTextureSupport)
				{
					// 使用复制的渲染目标纹理作为Shader的纹理资源
					stGPU[c_nSecondGPU].m_pID3D12Device4->CreateShaderResourceView(
						pISecondaryAdapterTexutrePerFrame[i].Get()
						, &stSRVDesc
						, stSrvHandle);
				}
				else
				{
					// 直接使用共享的渲染目标复制的纹理作为纹理资源
					stGPU[c_nSecondGPU].m_pID3D12Device4->CreateShaderResourceView(
						stGPU[c_nSecondGPU].m_pICrossAdapterResPerFrame[i].Get()
						, &stSRVDesc
						, stSrvHandle);
				}

				stSrvHandle.Offset(1, stGPU[c_nSecondGPU].m_nSRVDescriptorSize);
			}

			//Create CBV
			D3D12_CONSTANT_BUFFER_VIEW_DESC stCBVDesc = {};
			stCBVDesc.BufferLocation = pICBResSecondPass->GetGPUVirtualAddress();
			stCBVDesc.SizeInBytes = static_cast<UINT>(szSecondPassCB);

			stGPU[c_nSecondGPU].m_pID3D12Device4->CreateConstantBufferView(&stCBVDesc, stSrvHandle);

			stSrvHandle.Offset(1, stGPU[c_nSecondGPU].m_nSRVDescriptorSize);

			// Create Noise SRV
			D3D12_RESOURCE_DESC stTXDesc = pINoiseTexture->GetDesc();
			stSRVDesc.Format = stTXDesc.Format;
			//创建噪声纹理描述符
			stGPU[c_nSecondGPU].m_pID3D12Device4->CreateShaderResourceView(pINoiseTexture.Get(), &stSRVDesc, stSrvHandle);

			//-------------------------------------------------------------------------------------------------------
			// Create Third Pass SRV
			stSRVDesc.Format = emRTFormat;
			for (int k = 0; k < c_nPostPassCnt; k++)
			{
				CD3DX12_CPU_DESCRIPTOR_HANDLE stThirdPassSrvHandle(pISRVHeapPostPass[k]->GetCPUDescriptorHandleForHeapStart());
				for (int i = 0; i < g_nFrameBackBufCount; i++)
				{
					stGPU[c_nSecondGPU].m_pID3D12Device4->CreateShaderResourceView(
						pIOffLineRTRes[k][i].Get()
						, &stSRVDesc
						, stThirdPassSrvHandle);
					stThirdPassSrvHandle.Offset(1, stGPU[c_nSecondGPU].m_nSRVDescriptorSize);
				}
			}
			//-------------------------------------------------------------------------------------------------------

			// 创建Sample Descriptor Heap 采样器描述符堆
			stDescriptorHeapDesc.NumDescriptors = 1;  //只有一个Sample
			stDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
			GRS_THROW_IF_FAILED(
				stGPU[c_nSecondGPU].m_pID3D12Device4->CreateDescriptorHeap(
					&stDescriptorHeapDesc, IID_PPV_ARGS(&stGPU[c_nSecondGPU].m_pISampleHeap)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(stGPU[c_nSecondGPU].m_pISampleHeap);

			// 创建Sample View 实际就是采样器
			D3D12_SAMPLER_DESC stSamplerDesc = {};
			stSamplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
			stSamplerDesc.MinLOD = 0;
			stSamplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
			stSamplerDesc.MipLODBias = 0.0f;
			stSamplerDesc.MaxAnisotropy = 1;
			stSamplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
			stSamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			stSamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			stSamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;

			stGPU[c_nSecondGPU].m_pID3D12Device4->CreateSampler(
				&stSamplerDesc
				, stGPU[c_nSecondGPU].m_pISampleHeap->GetCPUDescriptorHandleForHeapStart());

			//-------------------------------------------------------------------------------------------------------
			// Create Third Pass Sample Heap
			GRS_THROW_IF_FAILED(
				stGPU[c_nSecondGPU].m_pID3D12Device4->CreateDescriptorHeap(
					&stDescriptorHeapDesc, IID_PPV_ARGS(&pISampleHeapPostPass)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pISampleHeapPostPass);

			// Create Third Pass Sample
			stGPU[c_nSecondGPU].m_pID3D12Device4->CreateSampler(
				&stSamplerDesc
				, pISampleHeapPostPass->GetCPUDescriptorHandleForHeapStart());


		}

		//显示窗口，准备开始消息循环渲染
		ShowWindow(hWnd, nCmdShow);
		UpdateWindow(hWnd);

		UINT nStates = 0; //初始状态为0
		DWORD dwRet = 0;
		CAtlArray<ID3D12CommandList*> arCmdList;
		CAtlArray<ID3D12DescriptorHeap*> arDesHeaps;
		BOOL bExit = FALSE;

		ULONGLONG n64tmFrameStart = ::GetTickCount64();
		ULONGLONG n64tmCurrent = n64tmFrameStart;
		//计算旋转角度需要的变量
		double dModelRotationYAngle = 0.0f;

		//--------------------------------------------------------------------------------------------------------------
		// 渲染循环中需要的D2D、DWrite相关变量定义
		// 目前我们设定输出文本框为40像素高度，差不多也就是一行
		float fTextOffsetX = 10.0f;
		D2D1_RECT_F stD2DTextRect = D2D1::RectF(fTextOffsetX, 0.0f, static_cast<float>(g_iWndWidth) - fTextOffsetX ,40.0f);
		WCHAR pszUIString[MAX_PATH] = {};
		//--------------------------------------------------------------------------------------------------------------

		//起始的时候关闭一下两个命令列表，因为我们在开始渲染的时候都需要先reset它们，为了防止报错故先Close
		GRS_THROW_IF_FAILED(pICMDListBefore->Close());
		GRS_THROW_IF_FAILED(pICMDListLater->Close());
		GRS_THROW_IF_FAILED(pICMDListCopy->Close());
		GRS_THROW_IF_FAILED(pICMDListPostPass->Close());

		SetTimer(hWnd, WM_USER + 100, 1, nullptr); //这句为了保证MsgWaitForMultipleObjects 在 wait for all 为True时 能够及时返回

		//开始消息循环，并在其中不断渲染
		while (!bExit)
		{
			//主线程进入等待
			dwRet = ::MsgWaitForMultipleObjects(static_cast<DWORD>(arHWaited.GetCount()), arHWaited.GetData(), TRUE, INFINITE, QS_ALLINPUT);
			dwRet -= WAIT_OBJECT_0;

			if ( 0 == dwRet )
			{//表示被等待的事件和消息都是有通知的状态了，先处理渲染，然后处理消息
				switch (nStates)
				{
				case 0://状态0，表示等到各子线程加载资源完毕，此时执行一次命令列表完成各子线程要求的资源上传的第二个Copy命令
				{// 该状态只会执行一次

					arCmdList.RemoveAll();
					//执行各线程创建于主显卡的命令列表
					arCmdList.Add(g_stThread[g_nThdSphere].m_pICMDList);
					arCmdList.Add(g_stThread[g_nThdCube].m_pICMDList);
					arCmdList.Add(g_stThread[g_nThdPlane].m_pICMDList);

					stGPU[c_nMainGPU].m_pICMDQueue->ExecuteCommandLists(static_cast<UINT>(arCmdList.GetCount()), arCmdList.GetData());

					//---------------------------------------------------------------------------------------------
					//开始同步GPU与CPU的执行，先记录围栏标记值
					n64CurrentFenceValue = n64FenceValue;
					GRS_THROW_IF_FAILED(stGPU[c_nMainGPU].m_pICMDQueue->Signal(stGPU[c_nMainGPU].m_pIFence.Get(), n64CurrentFenceValue));
					GRS_THROW_IF_FAILED(stGPU[c_nMainGPU].m_pIFence->SetEventOnCompletion(n64CurrentFenceValue, stGPU[c_nMainGPU].m_hEventFence));

					++n64FenceValue;

					nStates = 1;

					arHWaited.RemoveAll();
					arHWaited.Add(stGPU[c_nMainGPU].m_hEventFence);
				}
				break;
				case 1:// 状态1 等到命令队列执行结束（也即CPU等到GPU执行结束），开始新一轮渲染
				{

					//OnUpdate()
					{
						//关于时间的基本运算都放在了主线程中
						//真实的引擎或程序中建议时间值也作为一个每帧更新的参数从主线程获取并传给各子线程
						n64tmCurrent = ::GetTickCount();
						//计算旋转的角度：旋转角度(弧度) = 时间(秒) * 角速度(弧度/秒)
						//下面这句代码相当于经典游戏消息循环中的OnUpdate函数中需要做的事情
						dModelRotationYAngle += ((n64tmCurrent - n64tmFrameStart) / 1000.0f) * g_fPalstance;

						//旋转角度是2PI周期的倍数，去掉周期数，只留下相对0弧度开始的小于2PI的弧度即可
						if (dModelRotationYAngle > XM_2PI)
						{
							dModelRotationYAngle = fmod(dModelRotationYAngle, XM_2PI);
						}

						//计算 World 矩阵 这里是个旋转矩阵
						XMStoreFloat4x4(&g_mxWorld, XMMatrixRotationY(static_cast<float>(dModelRotationYAngle)));
						//计算 视矩阵 view * 裁剪矩阵 projection
						XMStoreFloat4x4(&g_mxVP
							, XMMatrixMultiply(XMMatrixLookAtLH(XMLoadFloat3(&g_f3EyePos)
								, XMLoadFloat3(&g_f3LockAt)
								, XMLoadFloat3(&g_f3HeapUp))
								, XMMatrixPerspectiveFovLH(XM_PIDIV4
									, (FLOAT)g_iWndWidth / (FLOAT)g_iWndHeight, 0.1f, 1000.0f)));
					}

					//更新第二遍渲染的常量缓冲区，可以放在OnUpdate里
					{
						pstCBSecondPass->m_nFun = g_nFunNO;
						pstCBSecondPass->m_fQuatLevel = g_fQuatLevel;
						pstCBSecondPass->m_fWaterPower = g_fWaterPower;
					}

					//OnRender()
					{
						GRS_THROW_IF_FAILED(pICMDAllocBefore->Reset());
						GRS_THROW_IF_FAILED(pICMDListBefore->Reset(pICMDAllocBefore.Get(), stGPU[c_nMainGPU].m_pIPSO.Get()));

						GRS_THROW_IF_FAILED(pICMDAllocLater->Reset());
						GRS_THROW_IF_FAILED(pICMDListLater->Reset(pICMDAllocLater.Get(), stGPU[c_nMainGPU].m_pIPSO.Get()));

						nCurrentFrameIndex = pISwapChain3->GetCurrentBackBufferIndex();

						//渲染前处理
						{
							pICMDListBefore->ResourceBarrier(1
								, &CD3DX12_RESOURCE_BARRIER::Transition(
									stGPU[c_nMainGPU].m_pIRTRes[nCurrentFrameIndex].Get()
									, D3D12_RESOURCE_STATE_COMMON
									, D3D12_RESOURCE_STATE_RENDER_TARGET)
							);

							//偏移描述符指针到指定帧缓冲视图位置
							CD3DX12_CPU_DESCRIPTOR_HANDLE stRTVHandle(
								stGPU[c_nMainGPU].m_pIRTVHeap->GetCPUDescriptorHandleForHeapStart()
								, nCurrentFrameIndex
								, stGPU[c_nMainGPU].m_nRTVDescriptorSize);
							CD3DX12_CPU_DESCRIPTOR_HANDLE stDSVHandle(
								stGPU[c_nMainGPU].m_pIDSVHeap->GetCPUDescriptorHandleForHeapStart());

							//设置渲染目标
							pICMDListBefore->OMSetRenderTargets(1, &stRTVHandle, FALSE, &stDSVHandle);

							pICMDListBefore->RSSetViewports(1, &g_stViewPort);
							pICMDListBefore->RSSetScissorRects(1, &g_stScissorRect);

							pICMDListBefore->ClearRenderTargetView(stRTVHandle, faClearColor, 0, nullptr);
							pICMDListBefore->ClearDepthStencilView(stDSVHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
						}

						nStates = 2;

						arHWaited.RemoveAll();
						arHWaited.Add(g_stThread[g_nThdSphere].m_hEventRenderOver);
						arHWaited.Add(g_stThread[g_nThdCube].m_hEventRenderOver);
						arHWaited.Add(g_stThread[g_nThdPlane].m_hEventRenderOver);

						//通知各线程开始渲染
						//注意：将时间值传递到各子渲染线程，都以主线程时间节点为准，这样场景的时间轴就同步了
						for (int i = 0; i < g_nMaxThread; i++)
						{
							g_stThread[i].m_nCurrentFrameIndex = nCurrentFrameIndex;
							g_stThread[i].m_nStartTime = n64tmFrameStart;
							g_stThread[i].m_nCurrentTime = n64tmCurrent;

							SetEvent(g_stThread[i].m_hEventRun);
						}

					}
				}
				break;
				case 2:// 状态2 表示所有的渲染命令列表都记录完成了，开始后处理和执行命令列表
				{
					// 1 First Pass
					{
						pICMDListLater->ResourceBarrier(1
							, &CD3DX12_RESOURCE_BARRIER::Transition(
								stGPU[c_nMainGPU].m_pIRTRes[nCurrentFrameIndex].Get()
								, D3D12_RESOURCE_STATE_RENDER_TARGET
								, D3D12_RESOURCE_STATE_COMMON));

						//关闭命令列表，可以去执行了
						GRS_THROW_IF_FAILED(pICMDListBefore->Close());
						GRS_THROW_IF_FAILED(pICMDListLater->Close());

						arCmdList.RemoveAll();
						//执行命令列表（注意命令列表排队组合的方式）
						arCmdList.Add(pICMDListBefore.Get());
						arCmdList.Add(g_stThread[g_nThdSphere].m_pICMDList);
						arCmdList.Add(g_stThread[g_nThdCube].m_pICMDList);
						arCmdList.Add(g_stThread[g_nThdPlane].m_pICMDList);
						arCmdList.Add(pICMDListLater.Get());

						stGPU[c_nMainGPU].m_pICMDQueue->ExecuteCommandLists(static_cast<UINT>(arCmdList.GetCount()), arCmdList.GetData());

						//主显卡同步
						n64CurrentFenceValue = n64FenceValue;

						GRS_THROW_IF_FAILED(stGPU[c_nMainGPU].m_pICMDQueue->Signal(stGPU[c_nMainGPU].m_pIFence.Get(), n64CurrentFenceValue));

						n64FenceValue++;
					}

					// Copy Main GPU Render Target Texture To Cross Adapter Heap Resource 
					// 记录主显卡的渲染结果复制到共享纹理资源中的命令列表
					{
						pICMDAllocCopy->Reset();
						pICMDListCopy->Reset(pICMDAllocCopy.Get(), nullptr);

						if (bCrossAdapterTextureSupport)
						{
							// 如果适配器支持跨适配器行主纹理，只需将该纹理复制到跨适配器纹理中。
							pICMDListCopy->CopyResource(
								stGPU[c_nMainGPU].m_pICrossAdapterResPerFrame[nCurrentFrameIndex].Get()
								, stGPU[c_nMainGPU].m_pIRTRes[nCurrentFrameIndex].Get());
						}
						else
						{
							// 如果适配器不支持跨适配器行主纹理，则将纹理复制为缓冲区，
							// 以便可以显式地管理纹理行间距。使用呈现目标指定的内存布局将中间呈现目标复制到共享缓冲区中。
							// 这相当于从Texture复制数据到Buffer中，也是一个很重要的技巧
							D3D12_RESOURCE_DESC stRenderTargetDesc = stGPU[c_nMainGPU].m_pIRTRes[nCurrentFrameIndex]->GetDesc();
							D3D12_PLACED_SUBRESOURCE_FOOTPRINT stRenderTargetLayout = {};

							stGPU[c_nMainGPU].m_pID3D12Device4->GetCopyableFootprints(
								&stRenderTargetDesc, 0, 1, 0, &stRenderTargetLayout, nullptr, nullptr, nullptr);

							CD3DX12_TEXTURE_COPY_LOCATION dest(
								stGPU[c_nMainGPU].m_pICrossAdapterResPerFrame[nCurrentFrameIndex].Get()
								, stRenderTargetLayout);
							CD3DX12_TEXTURE_COPY_LOCATION src(
								stGPU[c_nMainGPU].m_pIRTRes[nCurrentFrameIndex].Get()
								, 0);
							CD3DX12_BOX box(0, 0, g_iWndWidth, g_iWndHeight);

							pICMDListCopy->CopyTextureRegion(&dest, 0, 0, 0, &src, &box);
						}

						GRS_THROW_IF_FAILED(pICMDListCopy->Close());

						// 使用主显卡上的复制命令队列完成渲染目标资源到共享资源间的复制
						// 通过调用命令队列的Wait命令实现同一个GPU上的各命令队列之间的等待同步 
						GRS_THROW_IF_FAILED(pICmdQueueCopy->Wait(stGPU[c_nMainGPU].m_pIFence.Get(), n64CurrentFenceValue));

						arCmdList.RemoveAll();
						arCmdList.Add(pICMDListCopy.Get());
						pICmdQueueCopy->ExecuteCommandLists(static_cast<UINT>(arCmdList.GetCount()), arCmdList.GetData());

						n64CurrentFenceValue = n64FenceValue;
						// 复制命令的信号设置在共享的围栏对象上这样使得第二个显卡的命令队列可以在这个围栏上等待
						// 从而完成不同GPU命令队列间的同步等待
						GRS_THROW_IF_FAILED(pICmdQueueCopy->Signal(stGPU[c_nMainGPU].m_pISharedFence.Get(), n64CurrentFenceValue));
						n64FenceValue++;
					}

					// 2 Second Pass
					// 开始辅助显卡的渲染，通常是后处理，比如运动模糊等，我们这里直接就是把画面绘制出来
					{
						// 使用辅助显卡上的主命令队列执行命令列表
						// 辅助显卡上的主命令队列通过等待共享的围栏对象最终完成了与主显卡之间的同步
						GRS_THROW_IF_FAILED(stGPU[c_nSecondGPU].m_pICMDQueue->Wait(stGPU[c_nSecondGPU].m_pISharedFence.Get(), n64CurrentFenceValue));

						stGPU[c_nSecondGPU].m_pICMDAlloc->Reset();
						stGPU[c_nSecondGPU].m_pICMDList->Reset(stGPU[c_nSecondGPU].m_pICMDAlloc.Get(), stGPU[c_nSecondGPU].m_pIPSO.Get());

						if (!bCrossAdapterTextureSupport)
						{//当不能跨显卡共享纹理时，要执行多一遍COPY，相当于经典的两遍COPY中的第二遍COPY
							// 将共享堆中的缓冲区复制到辅助适配器可以从中取样的纹理中。
							D3D12_RESOURCE_BARRIER stResBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
								pISecondaryAdapterTexutrePerFrame[nCurrentFrameIndex].Get(),
								D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
								D3D12_RESOURCE_STATE_COPY_DEST);

							stGPU[c_nSecondGPU].m_pICMDList->ResourceBarrier(1, &stResBarrier);

							D3D12_RESOURCE_DESC stSecondaryAdapterTexture
								= pISecondaryAdapterTexutrePerFrame[nCurrentFrameIndex]->GetDesc();
							D3D12_PLACED_SUBRESOURCE_FOOTPRINT stTextureLayout = {};

							stGPU[c_nSecondGPU].m_pID3D12Device4->GetCopyableFootprints(
								&stSecondaryAdapterTexture, 0, 1, 0, &stTextureLayout, nullptr, nullptr, nullptr);

							CD3DX12_TEXTURE_COPY_LOCATION dest(
								pISecondaryAdapterTexutrePerFrame[nCurrentFrameIndex].Get(), 0);
							CD3DX12_TEXTURE_COPY_LOCATION src(
								stGPU[c_nSecondGPU].m_pICrossAdapterResPerFrame[nCurrentFrameIndex].Get()
								, stTextureLayout);
							CD3DX12_BOX box(0, 0, g_iWndWidth, g_iWndHeight);

							//这个Copy动作可以理解为一个纹理从Upload堆上传到Default堆
							//当显卡都支持跨显卡共享纹理时，这个复制动作就可以省却了，这样提高了效率
							//目前越新越高级的显卡基本都支持跨显卡共享纹理，所以就越高效
							//比如我现在的辅助显卡是Intel UHD G630就支持跨显卡共享纹理，就不会有这个多余的复制动作
							stGPU[c_nSecondGPU].m_pICMDList->CopyTextureRegion(&dest, 0, 0, 0, &src, &box);

							stResBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
							stResBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
							stGPU[c_nSecondGPU].m_pICMDList->ResourceBarrier(1, &stResBarrier);
						}

						//------------------------------------------------------------------------------------------------------
						// Second Pass 使用辅助显卡开始做后处理，也就是图像处理，这时各种滤镜就可以大显身手了
						stGPU[c_nSecondGPU].m_pICMDList->SetGraphicsRootSignature(stGPU[c_nSecondGPU].m_pIRS.Get());
						stGPU[c_nSecondGPU].m_pICMDList->SetPipelineState(stGPU[c_nSecondGPU].m_pIPSO.Get());

						arDesHeaps.RemoveAll();
						arDesHeaps.Add(stGPU[c_nSecondGPU].m_pISRVHeap.Get());
						arDesHeaps.Add(stGPU[c_nSecondGPU].m_pISampleHeap.Get());
						stGPU[c_nSecondGPU].m_pICMDList->SetDescriptorHeaps(static_cast<UINT>(arDesHeaps.GetCount()), arDesHeaps.GetData());

						stGPU[c_nSecondGPU].m_pICMDList->RSSetViewports(1, &g_stViewPort);
						stGPU[c_nSecondGPU].m_pICMDList->RSSetScissorRects(1, &g_stScissorRect);

						if (0 == g_nUsePSID)
						{//没有高斯模糊的情况，直接输出
							D3D12_RESOURCE_BARRIER stRTBarriersFourthPass = CD3DX12_RESOURCE_BARRIER::Transition(
								stGPU[c_nSecondGPU].m_pIRTRes[nCurrentFrameIndex].Get()
								, D3D12_RESOURCE_STATE_PRESENT
								, D3D12_RESOURCE_STATE_RENDER_TARGET);
							stGPU[c_nSecondGPU].m_pICMDList->ResourceBarrier(1, &stRTBarriersFourthPass);

							CD3DX12_CPU_DESCRIPTOR_HANDLE stRTV4Handle(
								stGPU[c_nSecondGPU].m_pIRTVHeap->GetCPUDescriptorHandleForHeapStart()
								, nCurrentFrameIndex
								, stGPU[c_nSecondGPU].m_nRTVDescriptorSize);

							stGPU[c_nSecondGPU].m_pICMDList->OMSetRenderTargets(1, &stRTV4Handle, false, nullptr);
							float f4ClearColor[] = { 1.0f,0.0f,1.0f,1.0f }; //故意使用不同的清除色，查看是否有“露底”的问题
							stGPU[c_nSecondGPU].m_pICMDList->ClearRenderTargetView(stRTV4Handle, f4ClearColor, 0, nullptr);
						}
						else
						{
							D3D12_RESOURCE_BARRIER stRTBarriersSecondPass = CD3DX12_RESOURCE_BARRIER::Transition(
								pIOffLineRTRes[c_nPostPass0][nCurrentFrameIndex].Get()
								, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
								, D3D12_RESOURCE_STATE_RENDER_TARGET);

							stGPU[c_nSecondGPU].m_pICMDList->ResourceBarrier(1, &stRTBarriersSecondPass);

							CD3DX12_CPU_DESCRIPTOR_HANDLE stRTVHadnleSecondPass(
								pIRTVOffLine[c_nPostPass0]->GetCPUDescriptorHandleForHeapStart()
								, nCurrentFrameIndex
								, stGPU[c_nSecondGPU].m_nRTVDescriptorSize);
							stGPU[c_nSecondGPU].m_pICMDList->OMSetRenderTargets(1, &stRTVHadnleSecondPass, false, nullptr);
							stGPU[c_nSecondGPU].m_pICMDList->ClearRenderTargetView(stRTVHadnleSecondPass, v4ClearColor, 0, nullptr);
						}


						// CBV
						CD3DX12_GPU_DESCRIPTOR_HANDLE stCBVGPUHandle(
							stGPU[c_nSecondGPU].m_pISRVHeap->GetGPUDescriptorHandleForHeapStart()
							, g_nFrameBackBufCount
							, stGPU[c_nSecondGPU].m_nSRVDescriptorSize);

						stGPU[c_nSecondGPU].m_pICMDList->SetGraphicsRootDescriptorTable(0, stCBVGPUHandle);

						// Render Target to Texture SRV
						CD3DX12_GPU_DESCRIPTOR_HANDLE stSRVGPUHandle(
							stGPU[c_nSecondGPU].m_pISRVHeap->GetGPUDescriptorHandleForHeapStart()
							, nCurrentFrameIndex
							, stGPU[c_nSecondGPU].m_nSRVDescriptorSize);

						stGPU[c_nSecondGPU].m_pICMDList->SetGraphicsRootDescriptorTable(1, stSRVGPUHandle);

						// Noise Texture SRV
						CD3DX12_GPU_DESCRIPTOR_HANDLE stNoiseSRVGPUHandle(
							stGPU[c_nSecondGPU].m_pISRVHeap->GetGPUDescriptorHandleForHeapStart()
							, g_nFrameBackBufCount + 1
							, stGPU[c_nSecondGPU].m_nSRVDescriptorSize);

						stGPU[c_nSecondGPU].m_pICMDList->SetGraphicsRootDescriptorTable(2, stNoiseSRVGPUHandle);

						stGPU[c_nSecondGPU].m_pICMDList->SetGraphicsRootDescriptorTable(3
							, stGPU[c_nSecondGPU].m_pISampleHeap->GetGPUDescriptorHandleForHeapStart());

						// 开始绘制矩形
						stGPU[c_nSecondGPU].m_pICMDList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
						stGPU[c_nSecondGPU].m_pICMDList->IASetVertexBuffers(0, 1, &pstVBVQuad);
						// Draw Call!
						stGPU[c_nSecondGPU].m_pICMDList->DrawInstanced(4, 1, 0, 0);

						if (0 == g_nUsePSID)
						{// 没有高斯模糊的情况，直接输出
							// 切换渲染目标纹理的状态，从渲染状态（In 即写入状态）变成呈现状态（Out 即读出状态）
							// 因为后面我们用了D2D和DWrite来输出2D对象或文本，所以不用切换了
							// D2D关联的D3D11on12设备内部会帮我们做这个操作
							//stGPU[c_nSecondGPU].m_pICMDList->ResourceBarrier(1
							//	, &CD3DX12_RESOURCE_BARRIER::Transition(
							//		stGPU[c_nSecondGPU].m_pIRTRes[nCurrentFrameIndex].Get()
							//		, D3D12_RESOURCE_STATE_RENDER_TARGET
							//		, D3D12_RESOURCE_STATE_PRESENT));
						}
						else
						{
							// 第二个显卡上的第一遍渲染目标纹理做状态切换
							stGPU[c_nSecondGPU].m_pICMDList->ResourceBarrier(1
								, &CD3DX12_RESOURCE_BARRIER::Transition(
									pIOffLineRTRes[c_nPostPass0][nCurrentFrameIndex].Get()
									, D3D12_RESOURCE_STATE_RENDER_TARGET
									, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
						}
						GRS_THROW_IF_FAILED(stGPU[c_nSecondGPU].m_pICMDList->Close());

					}
					// 调用高斯模糊效果（两遍 Post Pass）
					// 3-4 Third & Fourth Pass
					if (1 == g_nUsePSID)
					{
						pICMDAllocPostPass->Reset();
						pICMDListPostPass->Reset(pICMDAllocPostPass.Get(), pIPSOPostPass[c_nPostPass0].Get());

						//公共命令统一设置后，就不变了，因为后面两趟渲染是一个整体
						pICMDListPostPass->SetGraphicsRootSignature(pIRSPostPass.Get());
						pICMDListPostPass->RSSetViewports(1, &g_stViewPort);
						pICMDListPostPass->RSSetScissorRects(1, &g_stScissorRect);
						pICMDListPostPass->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
						pICMDListPostPass->IASetVertexBuffers(0, 1, &pstVBVQuad);

						//-------------------------------------------------------------------------------------------------------
						// 3 Thrid Pass
						// 使用高斯模糊
						pICMDListPostPass->SetPipelineState(pIPSOPostPass[c_nPostPass0].Get());
					
						arDesHeaps.RemoveAll();
						arDesHeaps.Add(pISRVHeapPostPass[c_nPostPass0].Get());
						arDesHeaps.Add(pISampleHeapPostPass.Get());

						pICMDListPostPass->SetDescriptorHeaps(static_cast<UINT>(arDesHeaps.GetCount()), arDesHeaps.GetData());

						D3D12_RESOURCE_BARRIER stRTBarriersThirdPass = CD3DX12_RESOURCE_BARRIER::Transition(
							pIOffLineRTRes[c_nPostPass1][nCurrentFrameIndex].Get()
							, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
							, D3D12_RESOURCE_STATE_RENDER_TARGET);

						pICMDListPostPass->ResourceBarrier(1, &stRTBarriersThirdPass);

						CD3DX12_CPU_DESCRIPTOR_HANDLE stRTVHandleThirdPass(
							pIRTVOffLine[c_nPostPass1]->GetCPUDescriptorHandleForHeapStart()
							, nCurrentFrameIndex
							, stGPU[c_nSecondGPU].m_nRTVDescriptorSize);
						pICMDListPostPass->OMSetRenderTargets(1, &stRTVHandleThirdPass, false, nullptr);
						pICMDListPostPass->ClearRenderTargetView(stRTVHandleThirdPass, v4ClearColor, 0, nullptr);

						//将第二遍渲染的结果作为纹理
						CD3DX12_GPU_DESCRIPTOR_HANDLE stSRV3Handle(
							pISRVHeapPostPass[c_nPostPass0]->GetGPUDescriptorHandleForHeapStart()
							, nCurrentFrameIndex
							, stGPU[c_nSecondGPU].m_nSRVDescriptorSize);

						pICMDListPostPass->SetGraphicsRootDescriptorTable(0, stSRV3Handle);
						pICMDListPostPass->SetGraphicsRootDescriptorTable(1, pISampleHeapPostPass->GetGPUDescriptorHandleForHeapStart());

						// Draw Call!
						pICMDListPostPass->DrawInstanced(4, 1, 0, 0);

						// 设置好同步围栏
						pICMDListPostPass->ResourceBarrier(1
							, &CD3DX12_RESOURCE_BARRIER::Transition(
								pIOffLineRTRes[c_nPostPass1][nCurrentFrameIndex].Get()
								, D3D12_RESOURCE_STATE_RENDER_TARGET
								, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

						//-------------------------------------------------------------------------------------------------------	
						// 4 Fourth Pass
						// 高斯模糊
						pICMDListPostPass->SetPipelineState(pIPSOPostPass[c_nPostPass1].Get());

						arDesHeaps.RemoveAll();
						arDesHeaps.Add(pISRVHeapPostPass[c_nPostPass1].Get());
						arDesHeaps.Add(pISampleHeapPostPass.Get());

						pICMDListPostPass->SetDescriptorHeaps(static_cast<UINT>(arDesHeaps.GetCount()), arDesHeaps.GetData());

						D3D12_RESOURCE_BARRIER stRTBarriersFourthPass = CD3DX12_RESOURCE_BARRIER::Transition(
							stGPU[c_nSecondGPU].m_pIRTRes[nCurrentFrameIndex].Get()
							, D3D12_RESOURCE_STATE_PRESENT
							, D3D12_RESOURCE_STATE_RENDER_TARGET);
						pICMDListPostPass->ResourceBarrier(1, &stRTBarriersFourthPass);

						CD3DX12_CPU_DESCRIPTOR_HANDLE stRTV4Handle(
							stGPU[c_nSecondGPU].m_pIRTVHeap->GetCPUDescriptorHandleForHeapStart()
							, nCurrentFrameIndex
							, stGPU[c_nSecondGPU].m_nRTVDescriptorSize);
						pICMDListPostPass->OMSetRenderTargets(1, &stRTV4Handle, false, nullptr);
						float f4ClearColor[] = { 1.0f,0.0f,1.0f,1.0f }; //故意使用不同的清除色，查看是否有“露底”的问题
						pICMDListPostPass->ClearRenderTargetView(stRTV4Handle, f4ClearColor, 0, nullptr);

						//将第三遍渲染的结果作为纹理
						CD3DX12_GPU_DESCRIPTOR_HANDLE stSRV4Handle(
							pISRVHeapPostPass[c_nPostPass1]->GetGPUDescriptorHandleForHeapStart()
							, nCurrentFrameIndex
							, stGPU[c_nSecondGPU].m_nSRVDescriptorSize);

						pICMDListPostPass->SetGraphicsRootDescriptorTable(0, stSRV4Handle);
						pICMDListPostPass->SetGraphicsRootDescriptorTable(1, pISampleHeapPostPass->GetGPUDescriptorHandleForHeapStart());
						// Draw Call!
						pICMDListPostPass->DrawInstanced(4, 1, 0, 0);

						// 切换渲染目标纹理的状态，从渲染状态（In 即写入状态）变成呈现状态（Out 即读出状态）
						// 因为后面我们用了D2D和DWrite来输出2D对象或文本，所以不用切换了，D2D关联的D3D11on12设备内部会帮我们做这个操作
						//pICMDListPostPass->ResourceBarrier(1
						//	, &CD3DX12_RESOURCE_BARRIER::Transition(
						//		stGPU[c_nSecondGPU].m_pIRTRes[nCurrentFrameIndex].Get()
						//		, D3D12_RESOURCE_STATE_RENDER_TARGET
						//		, D3D12_RESOURCE_STATE_PRESENT));

						GRS_THROW_IF_FAILED(pICMDListPostPass->Close());
						//-------------------------------------------------------------------------------------------------------
					}

					arCmdList.RemoveAll();
					arCmdList.Add(stGPU[c_nSecondGPU].m_pICMDList.Get());

					if (1 == g_nUsePSID)
					{
						arCmdList.Add(pICMDListPostPass.Get());
					}

					stGPU[c_nSecondGPU].m_pICMDQueue->ExecuteCommandLists(static_cast<UINT>(arCmdList.GetCount()), arCmdList.GetData());

					//-------------------------------------------------------------------------------------------------------
					// OnUIRender
					// 调用D2D、DWrite显示文字信息，这里相当于OnUIRender()
					{
						if ( 1 == g_nFunNO )
						{
							StringCchPrintfW(pszUIString, MAX_PATH, _T("水彩画效果渲染：开启；"));
						}
						else
						{
							StringCchPrintfW(pszUIString, MAX_PATH, _T("水彩画效果渲染：关闭；"));
						}

						if (1 == g_nUsePSID)
						{
							StringCchPrintfW(pszUIString, MAX_PATH, _T("%s高斯模糊后处理渲染：开启。"),pszUIString);
						}
						else
						{
							StringCchPrintfW(pszUIString, MAX_PATH, _T("%s高斯模糊后处理渲染：关闭。"), pszUIString);
						}

						pID3D11On12Device1->AcquireWrappedResources(pID3D11WrappedBackBuffers[nCurrentFrameIndex].GetAddressOf(), 1);

						pID2D1DeviceContext6->SetTarget(pID2DRenderTargets[nCurrentFrameIndex].Get());
						pID2D1DeviceContext6->BeginDraw();
						pID2D1DeviceContext6->SetTransform(D2D1::Matrix3x2F::Identity());
						pID2D1DeviceContext6->DrawTextW(
							pszUIString,
							_countof(pszUIString) - 1,
							pIDWriteTextFormat.Get(),
							&stD2DTextRect,
							pID2D1SolidColorBrush.Get()
						);
						GRS_THROW_IF_FAILED(pID2D1DeviceContext6->EndDraw());

						pID3D11On12Device1->ReleaseWrappedResources(pID3D11WrappedBackBuffers[nCurrentFrameIndex].GetAddressOf(), 1);

						pID3D11DeviceContext4->Flush();
					}
					//-------------------------------------------------------------------------------------------------------

					// 执行Present命令最终呈现画面
					GRS_THROW_IF_FAILED(pISwapChain3->Present(1, 0));

					n64CurrentFenceValue = n64FenceValue;
					GRS_THROW_IF_FAILED(stGPU[c_nSecondGPU].m_pICMDQueue->Signal(stGPU[c_nSecondGPU].m_pIFence.Get(), n64CurrentFenceValue));
					GRS_THROW_IF_FAILED(stGPU[c_nSecondGPU].m_pIFence->SetEventOnCompletion(n64CurrentFenceValue, stGPU[c_nSecondGPU].m_hEventFence));

					n64FenceValue++;

					// 状态又跳回1 开始下一轮渲染
					nStates = 1;

					// 开始渲染后只需要在辅助显卡的事件句柄上等待即可
					// 因为辅助显卡已经在GPU侧与主显卡通过Wait命令进行了同步
					arHWaited.RemoveAll();
					arHWaited.Add(stGPU[c_nSecondGPU].m_hEventFence);

					//更新下帧开始时间为上一帧开始时间
					n64tmFrameStart = n64tmCurrent;
				}
				break;
				default:// 不可能的情况，但为了避免讨厌的编译警告或意外情况保留这个default
				{
					bExit = TRUE;
				}
				break;
				}

				//处理消息
				while (::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE | PM_NOYIELD))
				{
					if (WM_QUIT != msg.message)
					{
						::TranslateMessage(&msg);
						::DispatchMessage(&msg);
					}
					else
					{
						bExit = TRUE;
					}
				}
			}
			else
			{//出错的情况，退出
				bExit = TRUE;
			}

			//检测一下线程的活动情况，如果有线程已经退出了，就退出循环
			dwRet = WaitForMultipleObjects(static_cast<DWORD>(arHSubThread.GetCount()), arHSubThread.GetData(), FALSE, 0);
			dwRet -= WAIT_OBJECT_0;
			if (dwRet >= 0 && dwRet < g_nMaxThread)
			{//有线程的句柄已经成为有信号状态，即对应线程已经退出，这说明发生了错误
				bExit = TRUE;
			}
		}

		::KillTimer(hWnd, WM_USER + 100);

	}
	catch (CGRSCOMException & e)
	{//发生了COM异常
		e;
	}

	try
	{//注意不要和上面的异常合并，因为主线程有可能发生异常直接到了异常处理块，而子线程根本没有接到退出通知
		// 通知子线程退出
		for (int i = 0; i < g_nMaxThread; i++)
		{
			::PostThreadMessage(g_stThread[i].m_dwThisThreadID, WM_QUIT, 0, 0);
		}

		// 等待所有子线程退出
		DWORD dwRet = WaitForMultipleObjects(static_cast<DWORD>(arHSubThread.GetCount()), arHSubThread.GetData(), TRUE, INFINITE);
		// 清理所有子线程资源
		for (int i = 0; i < g_nMaxThread; i++)
		{
			::CloseHandle(g_stThread[i].m_hThisThread);
			::CloseHandle(g_stThread[i].m_hEventRenderOver);
			GRS_SAFE_RELEASE(g_stThread[i].m_pICMDList);
			GRS_SAFE_RELEASE(g_stThread[i].m_pICMDAlloc);
		}
	}
	catch (CGRSCOMException & e)
	{//发生了异常
		e;
	}
	::CoUninitialize();
	return 0;
}

UINT __stdcall RenderThread(void* pParam)
{
	ST_GRS_THREAD_PARAMS* pThdPms = static_cast<ST_GRS_THREAD_PARAMS*>(pParam);
	try
	{
		if (nullptr == pThdPms)
		{//参数异常，抛异常终止线程
			throw CGRSCOMException(E_INVALIDARG);
		}

		SIZE_T								szMVPBuf = GRS_UPPER(sizeof(ST_GRS_MVP), 256);
		SIZE_T								szPerObjDataBuf = GRS_UPPER(sizeof(ST_GRS_PEROBJECT_CB), 256);

		ComPtr<ID3D12Resource>				pITexture;
		ComPtr<ID3D12Resource>				pITextureUpload;
		ComPtr<ID3D12Resource>				pIVB;
		ComPtr<ID3D12Resource>				pIIB;
		ComPtr<ID3D12Resource>			    pICBWVP;
		ComPtr<ID3D12Resource>				pICBPerObjData;
		ComPtr<ID3D12DescriptorHeap>		pISRVHeap;
		ComPtr<ID3D12DescriptorHeap>		pISampleHeap;

		ST_GRS_MVP* pMVPBufModule = nullptr;
		ST_GRS_PEROBJECT_CB* pPerObjBuf = nullptr;
		D3D12_VERTEX_BUFFER_VIEW			stVBV = {};
		D3D12_INDEX_BUFFER_VIEW				stIBV = {};
		UINT								nIndexCnt = 0;
		XMMATRIX							mxPosModule = XMMatrixTranslationFromVector(XMLoadFloat4(&pThdPms->m_v4ModelPos));  //当前渲染物体的位置
		// Mesh Value
		ST_GRS_VERTEX*						pstVertices = nullptr;
		UINT*								pnIndices = nullptr;
		UINT								nVertexCnt = 0;
		// DDS Value
		std::unique_ptr<uint8_t[]>			pbDDSData;
		std::vector<D3D12_SUBRESOURCE_DATA> stArSubResources;
		DDS_ALPHA_MODE						emAlphaMode = DDS_ALPHA_MODE_UNKNOWN;
		bool								bIsCube = false;

		UINT								nSRVDescriptorSize = 0;
		UINT								nRTVDescriptorSize = 0;
		//1、加载DDS纹理
		{
			nRTVDescriptorSize = pThdPms->m_pID3D12Device4->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
			nSRVDescriptorSize = pThdPms->m_pID3D12Device4->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			GRS_THROW_IF_FAILED(LoadDDSTextureFromFile(
				pThdPms->m_pID3D12Device4
				, pThdPms->m_pszDDSFile
				, pITexture.GetAddressOf()
				, pbDDSData
				, stArSubResources
				, SIZE_MAX
				, &emAlphaMode
				, &bIsCube));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pITexture);

			UINT64 n64szUpSphere = GetRequiredIntermediateSize(
				pITexture.Get()
				, 0
				, static_cast<UINT>(stArSubResources.size()));

			D3D12_RESOURCE_DESC stTXDesc = pITexture->GetDesc();

			GRS_THROW_IF_FAILED(pThdPms->m_pID3D12Device4->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD)
				, D3D12_HEAP_FLAG_NONE
				, &CD3DX12_RESOURCE_DESC::Buffer(n64szUpSphere)
				, D3D12_RESOURCE_STATE_GENERIC_READ
				, nullptr
				, IID_PPV_ARGS(&pITextureUpload)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pITextureUpload);

			//执行两个Copy动作将纹理上传到默认堆中
			UpdateSubresources(pThdPms->m_pICMDList
				, pITexture.Get()
				, pITextureUpload.Get()
				, 0
				, 0
				, static_cast<UINT>(stArSubResources.size())
				, stArSubResources.data());

			//同步
			pThdPms->m_pICMDList->ResourceBarrier(1
				, &CD3DX12_RESOURCE_BARRIER::Transition(pITexture.Get()
					, D3D12_RESOURCE_STATE_COPY_DEST
					, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
		}

		//2、创建描述符堆
		{
			D3D12_DESCRIPTOR_HEAP_DESC stSRVHeapDesc = {};
			stSRVHeapDesc.NumDescriptors = 2; // 1 CBV + 1 SRV
			stSRVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			stSRVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

			GRS_THROW_IF_FAILED(pThdPms->m_pID3D12Device4->CreateDescriptorHeap(
				&stSRVHeapDesc
				, IID_PPV_ARGS(&pISRVHeap)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pISRVHeap);

			D3D12_RESOURCE_DESC stTXDesc = pITexture->GetDesc();

			//创建Texture 的 SRV
			D3D12_SHADER_RESOURCE_VIEW_DESC stSRVDesc = {};
			stSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			stSRVDesc.Format = stTXDesc.Format;
			stSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			stSRVDesc.Texture2D.MipLevels = 1;

			CD3DX12_CPU_DESCRIPTOR_HANDLE stCbvSrvHandle(
				pISRVHeap->GetCPUDescriptorHandleForHeapStart()
				, 1
				, nSRVDescriptorSize);

			pThdPms->m_pID3D12Device4->CreateShaderResourceView(
				pITexture.Get()
				, &stSRVDesc
				, stCbvSrvHandle);
		}

		//3、创建常量缓冲 
		{
			GRS_THROW_IF_FAILED(pThdPms->m_pID3D12Device4->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD)
				, D3D12_HEAP_FLAG_NONE
				, &CD3DX12_RESOURCE_DESC::Buffer(szMVPBuf) //注意缓冲尺寸设置为256边界对齐大小
				, D3D12_RESOURCE_STATE_GENERIC_READ
				, nullptr
				, IID_PPV_ARGS(&pICBWVP)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pICBWVP);

			// Map 之后就不再Unmap了 直接复制数据进去 这样每帧都不用map-copy-unmap浪费时间了
			GRS_THROW_IF_FAILED(pICBWVP->Map(0, nullptr, reinterpret_cast<void**>(&pMVPBufModule)));

			// 创建CBV
			D3D12_CONSTANT_BUFFER_VIEW_DESC stCBVDesc = {};
			stCBVDesc.BufferLocation = pICBWVP->GetGPUVirtualAddress();
			stCBVDesc.SizeInBytes = static_cast<UINT>(szMVPBuf);

			CD3DX12_CPU_DESCRIPTOR_HANDLE stCbvSrvHandle(pISRVHeap->GetCPUDescriptorHandleForHeapStart());
			pThdPms->m_pID3D12Device4->CreateConstantBufferView(&stCBVDesc, stCbvSrvHandle);
		}

		//4、创建Sample
		{
			D3D12_DESCRIPTOR_HEAP_DESC stSamplerHeapDesc = {};
			stSamplerHeapDesc.NumDescriptors = 1;
			stSamplerHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
			stSamplerHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

			GRS_THROW_IF_FAILED(pThdPms->m_pID3D12Device4->CreateDescriptorHeap(
				&stSamplerHeapDesc
				, IID_PPV_ARGS(&pISampleHeap)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pISampleHeap);

			D3D12_SAMPLER_DESC stSamplerDesc = {};
			stSamplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
			stSamplerDesc.MinLOD = 0;
			stSamplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
			stSamplerDesc.MipLODBias = 0.0f;
			stSamplerDesc.MaxAnisotropy = 1;
			stSamplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
			stSamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			stSamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			stSamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;

			pThdPms->m_pID3D12Device4->CreateSampler(&stSamplerDesc
				, pISampleHeap->GetCPUDescriptorHandleForHeapStart());
		}

		//5、加载网格数据
		{
			LoadMeshVertex(pThdPms->m_pszMeshFile
				, nVertexCnt
				, pstVertices
				, pnIndices);
			nIndexCnt = nVertexCnt;

			//创建 Vertex Buffer 仅使用Upload隐式堆
			GRS_THROW_IF_FAILED(pThdPms->m_pID3D12Device4->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD)
				, D3D12_HEAP_FLAG_NONE
				, &CD3DX12_RESOURCE_DESC::Buffer(nVertexCnt * sizeof(ST_GRS_VERTEX))
				, D3D12_RESOURCE_STATE_GENERIC_READ
				, nullptr
				, IID_PPV_ARGS(&pIVB)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIVB);


			UINT8* pVertexDataBegin = nullptr;
			CD3DX12_RANGE stReadRange(0, 0);

			//使用map-memcpy-unmap大法将数据传至顶点缓冲对象
			GRS_THROW_IF_FAILED(pIVB->Map(0
				, &stReadRange
				, reinterpret_cast<void**>(&pVertexDataBegin)));
			memcpy(pVertexDataBegin, pstVertices, nVertexCnt * sizeof(ST_GRS_VERTEX));
			pIVB->Unmap(0, nullptr);

			//创建 Index Buffer 仅使用Upload隐式堆
			GRS_THROW_IF_FAILED(pThdPms->m_pID3D12Device4->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD)
				, D3D12_HEAP_FLAG_NONE
				, &CD3DX12_RESOURCE_DESC::Buffer(nIndexCnt * sizeof(UINT))
				, D3D12_RESOURCE_STATE_GENERIC_READ
				, nullptr
				, IID_PPV_ARGS(&pIIB)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIIB);

			UINT8* pIndexDataBegin = nullptr;
			GRS_THROW_IF_FAILED(pIIB->Map(0
				, &stReadRange
				, reinterpret_cast<void**>(&pIndexDataBegin)));
			memcpy(pIndexDataBegin, pnIndices, nIndexCnt * sizeof(UINT));
			pIIB->Unmap(0, nullptr);

			//创建Vertex Buffer View
			stVBV.BufferLocation = pIVB->GetGPUVirtualAddress();
			stVBV.StrideInBytes = sizeof(ST_GRS_VERTEX);
			stVBV.SizeInBytes = nVertexCnt * sizeof(ST_GRS_VERTEX);

			//创建Index Buffer View
			stIBV.BufferLocation = pIIB->GetGPUVirtualAddress();
			stIBV.Format = DXGI_FORMAT_R32_UINT;
			stIBV.SizeInBytes = nIndexCnt * sizeof(UINT);

			::HeapFree(::GetProcessHeap(), 0, pstVertices);
			::HeapFree(::GetProcessHeap(), 0, pnIndices);
		}

		//6、设置事件对象 通知并切回主线程 完成资源的第二个Copy命令
		{
			GRS_THROW_IF_FAILED(pThdPms->m_pICMDList->Close());
			//第一次通知主线程本线程加载资源完毕
			::SetEvent(pThdPms->m_hEventRenderOver); // 设置信号，通知主线程本线程资源加载完毕
		}

		// 仅用于球体跳动物理特效的变量
		float fJmpSpeed = 0.003f; //跳动速度
		float fUp = 1.0f;
		float fRawYPos = pThdPms->m_v4ModelPos.y;
		XMMATRIX xmMWVP;
		CAtlArray<ID3D12DescriptorHeap*> arDesHeaps;
		DWORD dwRet = 0;
		BOOL  bQuit = FALSE;
		MSG   msg = {};

		//7、渲染循环
		while (!bQuit)
		{
			// 等待主线程通知开始渲染，同时仅接收主线程Post过来的消息，目前就是为了等待WM_QUIT消息
			dwRet = ::MsgWaitForMultipleObjects(1, &pThdPms->m_hEventRun, FALSE, INFINITE, QS_ALLPOSTMESSAGE);
			switch (dwRet - WAIT_OBJECT_0)
			{
			case 0:
			{
				// OnUpdate()
				// 准备MWVP矩阵
				{
					//==============================================================================
					//使用主线程更新的统一帧时间更新下物体的物理状态等，这里仅演示球体上下跳动的样子
					//启发式的向大家说明利用多线程渲染时，物理变换可以在不同的线程中，并且在不同的CPU上并行的执行
					if (g_nThdSphere == pThdPms->m_nThreadIndex)
					{
						if (pThdPms->m_v4ModelPos.y >= 2.0f * fRawYPos)
						{
							fUp = -0.2f;
							pThdPms->m_v4ModelPos.y = 2.0f * fRawYPos;
						}

						if (pThdPms->m_v4ModelPos.y <= fRawYPos)
						{
							fUp = 0.2f;
							pThdPms->m_v4ModelPos.y = fRawYPos;
						}

						pThdPms->m_v4ModelPos.y
							+= fUp * fJmpSpeed * static_cast<float>(pThdPms->m_nCurrentTime - pThdPms->m_nStartTime);

						mxPosModule = XMMatrixTranslationFromVector(XMLoadFloat4(&pThdPms->m_v4ModelPos));
					}
					//==============================================================================

					// Module * World
					xmMWVP = XMMatrixMultiply(mxPosModule, XMLoadFloat4x4(&g_mxWorld));

					// (Module * World) * View * Projection
					xmMWVP = XMMatrixMultiply(xmMWVP, XMLoadFloat4x4(&g_mxVP));

					XMStoreFloat4x4(&pMVPBufModule->m_MVP, xmMWVP);
				}

				// OnRender()
				//命令分配器先Reset一下，刚才已经执行过了一个复制纹理的命令
				GRS_THROW_IF_FAILED(pThdPms->m_pICMDAlloc->Reset());
				//Reset命令列表，并重新指定命令分配器和PSO对象
				GRS_THROW_IF_FAILED(pThdPms->m_pICMDList->Reset(pThdPms->m_pICMDAlloc, pThdPms->m_pIPSO));

				//---------------------------------------------------------------------------------------------
				//设置对应的渲染目标和视裁剪框(这是渲染子线程必须要做的步骤，基本也就是所谓多线程渲染的核心秘密所在了)
				{
					CD3DX12_CPU_DESCRIPTOR_HANDLE stRTVHandle(
						pThdPms->m_pIRTVHeap->GetCPUDescriptorHandleForHeapStart()
						, pThdPms->m_nCurrentFrameIndex
						, nRTVDescriptorSize);
					CD3DX12_CPU_DESCRIPTOR_HANDLE stDSVHandle(
						pThdPms->m_pIDSVHeap->GetCPUDescriptorHandleForHeapStart());
					//设置渲染目标
					pThdPms->m_pICMDList->OMSetRenderTargets(1, &stRTVHandle, FALSE, &stDSVHandle);
					pThdPms->m_pICMDList->RSSetViewports(1, &g_stViewPort);
					pThdPms->m_pICMDList->RSSetScissorRects(1, &g_stScissorRect);
				}
				//---------------------------------------------------------------------------------------------

				//渲染（实质就是记录渲染命令列表）
				{
					pThdPms->m_pICMDList->SetGraphicsRootSignature(pThdPms->m_pIRS);
					pThdPms->m_pICMDList->SetPipelineState(pThdPms->m_pIPSO);

					arDesHeaps.RemoveAll();
					arDesHeaps.Add(pISRVHeap.Get());
					arDesHeaps.Add(pISampleHeap.Get());

					pThdPms->m_pICMDList->SetDescriptorHeaps(static_cast<UINT>(arDesHeaps.GetCount()), arDesHeaps.GetData());

					CD3DX12_GPU_DESCRIPTOR_HANDLE stSRVHandle(pISRVHeap->GetGPUDescriptorHandleForHeapStart());
					//设置CBV
					pThdPms->m_pICMDList->SetGraphicsRootDescriptorTable(0, stSRVHandle);

					//偏移至Texture的View
					stSRVHandle.Offset(1, nSRVDescriptorSize);
					//设置SRV
					pThdPms->m_pICMDList->SetGraphicsRootDescriptorTable(1, stSRVHandle);

					//设置Sample
					pThdPms->m_pICMDList->SetGraphicsRootDescriptorTable(2, pISampleHeap->GetGPUDescriptorHandleForHeapStart());

					//注意我们使用的渲染手法是三角形列表，也就是通常的Mesh网格
					pThdPms->m_pICMDList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
					pThdPms->m_pICMDList->IASetVertexBuffers(0, 1, &stVBV);
					pThdPms->m_pICMDList->IASetIndexBuffer(&stIBV);

					//Draw Call！！！
					pThdPms->m_pICMDList->DrawIndexedInstanced(nIndexCnt, 1, 0, 0, 0);
				}

				//完成渲染（即关闭命令列表，并设置同步对象通知主线程开始执行）
				{
					GRS_THROW_IF_FAILED(pThdPms->m_pICMDList->Close());
					::SetEvent(pThdPms->m_hEventRenderOver); // 设置信号，通知主线程本线程渲染完毕
				}
			}
			break;
			case 1:
			{//处理消息
				while (::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
				{//这里只可能是别的线程发过来的消息，用于更复杂的场景
					if (WM_QUIT != msg.message)
					{
						::TranslateMessage(&msg);
						::DispatchMessage(&msg);
					}
					else
					{
						bQuit = TRUE;
					}
				}
			}
			break;
			case WAIT_TIMEOUT:
				break;
			default:
				break;
			}
		}
	}
	catch (CGRSCOMException & e)
	{
		e;
	}

	return 0;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_DESTROY:
	{
		PostQuitMessage(0);
	}
	break;
	case WM_KEYDOWN:
	{
		USHORT n16KeyCode = (wParam & 0xFF);

		if (VK_SPACE == n16KeyCode)
		{//切换效果
			g_nFunNO += 1;
			if (g_nFunNO >= g_nMaxFunNO)
			{
				g_nFunNO = 0;
			}
		}

		if (VK_TAB == n16KeyCode)
		{
			++g_nUsePSID;
			if (g_nUsePSID > 1)
			{
				g_nUsePSID = 0;
			}
		}

		if (1 == g_nFunNO)
		{//当进行水彩画效果渲染时控制生效
			if ('Q' == n16KeyCode || 'q' == n16KeyCode)
			{//q 增加水彩取色半径
				g_fWaterPower += 1.0f;
				if (g_fWaterPower >= 64.0f)
				{
					g_fWaterPower = 8.0f;
				}
			}

			if ('E' == n16KeyCode || 'e' == n16KeyCode)
			{//e 控制量化参数，范围 2 - 6
				g_fQuatLevel += 1.0f;
				if (g_fQuatLevel > 6.0f)
				{
					g_fQuatLevel = 2.0f;
				}
			}
		}

		if (VK_ADD == n16KeyCode || VK_OEM_PLUS == n16KeyCode)
		{
			//double g_fPalstance = 10.0f * XM_PI / 180.0f;	//物体旋转的角速度，单位：弧度/秒
			g_fPalstance += 10.0f * XM_PI / 180.0f;
			if (g_fPalstance > XM_PI)
			{
				g_fPalstance = XM_PI;
			}
		}

		if (VK_SUBTRACT == n16KeyCode || VK_OEM_MINUS == n16KeyCode)
		{
			g_fPalstance -= 10.0f * XM_PI / 180.0f;
			if (g_fPalstance < 0.0f)
			{
				g_fPalstance = XM_PI / 180.0f;
			}
		}

		//根据用户输入变换
		//XMVECTOR g_f3EyePos = XMVectorSet(0.0f, 5.0f, -10.0f, 0.0f); //眼睛位置
		//XMVECTOR g_f3LockAt = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);  //眼睛所盯的位置
		//XMVECTOR g_f3HeapUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);  //头部正上方位置
		XMFLOAT3 move(0, 0, 0);
		float fMoveSpeed = 2.0f;
		float fTurnSpeed = XM_PIDIV2 * 0.005f;

		if ('w' == n16KeyCode || 'W' == n16KeyCode)
		{
			move.z -= 1.0f;
		}

		if ('s' == n16KeyCode || 'S' == n16KeyCode)
		{
			move.z += 1.0f;
		}

		if ('d' == n16KeyCode || 'D' == n16KeyCode)
		{
			move.x += 1.0f;
		}

		if ('a' == n16KeyCode || 'A' == n16KeyCode)
		{
			move.x -= 1.0f;
		}

		if (fabs(move.x) > 0.1f && fabs(move.z) > 0.1f)
		{
			XMVECTOR vector = XMVector3Normalize(XMLoadFloat3(&move));
			move.x = XMVectorGetX(vector);
			move.z = XMVectorGetZ(vector);
		}

		if (VK_UP == n16KeyCode)
		{
			g_fPitch += fTurnSpeed;
		}

		if (VK_DOWN == n16KeyCode)
		{
			g_fPitch -= fTurnSpeed;
		}

		if (VK_RIGHT == n16KeyCode)
		{
			g_fYaw -= fTurnSpeed;
		}

		if (VK_LEFT == n16KeyCode)
		{
			g_fYaw += fTurnSpeed;
		}

		// Prevent looking too far up or down.
		g_fPitch = min(g_fPitch, XM_PIDIV4);
		g_fPitch = max(-XM_PIDIV4, g_fPitch);

		// Move the camera in model space.
		float x = move.x * -cosf(g_fYaw) - move.z * sinf(g_fYaw);
		float z = move.x * sinf(g_fYaw) - move.z * cosf(g_fYaw);
		g_f3EyePos.x += x * fMoveSpeed;
		g_f3EyePos.z += z * fMoveSpeed;

		// Determine the look direction.
		float r = cosf(g_fPitch);
		g_f3LockAt.x = r * sinf(g_fYaw);
		g_f3LockAt.y = sinf(g_fPitch);
		g_f3LockAt.z = r * cosf(g_fYaw);

		if (VK_ESCAPE == n16KeyCode)
		{//按ESC键还原摄像机位置
			g_f3EyePos = XMFLOAT3(0.0f, 5.0f, -10.0f); //眼睛位置
			g_f3LockAt = XMFLOAT3(0.0f, 0.0f, 0.0f);    //眼睛所盯的位置
			g_f3HeapUp = XMFLOAT3(0.0f, 1.0f, 0.0f);    //头部正上方位置
		}
	}
	break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

BOOL LoadMeshVertex(const CHAR* pszMeshFileName, UINT& nVertexCnt, ST_GRS_VERTEX*& ppVertex, UINT*& ppIndices)
{
	ifstream fin;
	char input;
	BOOL bRet = TRUE;
	try
	{
		fin.open(pszMeshFileName);
		if (fin.fail())
		{
			throw CGRSCOMException(E_FAIL);
		}
		fin.get(input);
		while (input != ':')
		{
			fin.get(input);
		}
		fin >> nVertexCnt;

		fin.get(input);
		while (input != ':')
		{
			fin.get(input);
		}
		fin.get(input);
		fin.get(input);

		ppVertex = (ST_GRS_VERTEX*)HeapAlloc(::GetProcessHeap()
			, HEAP_ZERO_MEMORY
			, nVertexCnt * sizeof(ST_GRS_VERTEX));
		ppIndices = (UINT*)HeapAlloc(::GetProcessHeap()
			, HEAP_ZERO_MEMORY
			, nVertexCnt * sizeof(UINT));

		for (UINT i = 0; i < nVertexCnt; i++)
		{
			fin >> ppVertex[i].m_vPos.x >> ppVertex[i].m_vPos.y >> ppVertex[i].m_vPos.z;
			ppVertex[i].m_vPos.w = 1.0f;
			fin >> ppVertex[i].m_vTex.x >> ppVertex[i].m_vTex.y;
			fin >> ppVertex[i].m_vNor.x >> ppVertex[i].m_vNor.y >> ppVertex[i].m_vNor.z;

			ppIndices[i] = i;
		}
	}
	catch (CGRSCOMException & e)
	{
		e;
		bRet = FALSE;
	}
	return bRet;
}
