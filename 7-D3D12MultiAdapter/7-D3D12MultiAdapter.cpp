#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN // �� Windows ͷ���ų�����ʹ�õ�����
#include <windows.h>
#include <tchar.h>
#include <fstream>  //for ifstream
using namespace std;
#include <wrl.h> //����WTL֧�� ����ʹ��COM
using namespace Microsoft;
using namespace Microsoft::WRL;
#include <atlcoll.h>  //for atl array
#include <strsafe.h>  //for StringCchxxxxx function

#include <dxgi1_6.h>
#include <d3d12.h> //for d3d12
#include <d3dcompiler.h>

#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3dcompiler.lib")

#if defined(_DEBUG)
#include <dxgidebug.h>
#endif

#include <DirectXMath.h>
#include "..\WindowsCommons\d3dx12.h"
#include "..\WindowsCommons\DDSTextureLoader12.h"
using namespace DirectX;

#define GRS_WND_CLASS_NAME _T("Game Window Class")
#define GRS_WND_TITLE	_T("D3D12 MultiAdapter Sample")


#define GRS_SAFE_RELEASE(p) if(p){(p)->Release();(p)=nullptr;}
#define GRS_THROW_IF_FAILED(hr) if (FAILED(hr)){ throw CGRSCOMException(hr); }

//�¶���ĺ�������ȡ������
#define GRS_UPPER_DIV(A,B) ((UINT)(((A)+((B)-1))/(B)))
//���������ϱ߽�����㷨 �ڴ�����г��� ���ס
#define GRS_UPPER(A,B) ((UINT)(((A)+((B)-1))&~(B - 1)))

//------------------------------------------------------------------------------------------------------------
// Ϊ�˵��Լ�����������������ͺ궨�壬Ϊÿ���ӿڶ����������ƣ�����鿴�������
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

inline void GRS_SetDXGIDebugName(ID3D12Object*, LPCWSTR)
{
}
inline void GRS_SetDXGIDebugNameIndexed(ID3D12Object*, LPCWSTR, UINT)
{
}

#endif

#define GRS_SET_DXGI_DEBUGNAME(x)						GRS_SetDXGIDebugName(x, L#x)
#define GRS_SET_DXGI_DEBUGNAME_INDEXED(x, n)			GRS_SetDXGIDebugNameIndexed(x[n], L#x, n)

#define GRS_SET_DXGI_DEBUGNAME_COMPTR(x)				GRS_SetDXGIDebugName(x.Get(), L#x)
#define GRS_SET_DXGI_DEBUGNAME_INDEXED_COMPTR(x, n)		GRS_SetDXGIDebugNameIndexed(x[n].Get(), L#x, n)
//------------------------------------------------------------------------------------------------------------

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

// ����ṹ
struct ST_GRS_VERTEX_MODULE
{
	XMFLOAT4 m_vPos;		//Position
	XMFLOAT2 m_vTex;		//Texcoord
	XMFLOAT3 m_vNor;		//Normal
};

// �����Կ�����Ⱦ��λ���εĶ���ṹ
struct ST_GRS_VERTEX_QUAD
{
	XMFLOAT4 m_vPos;		//Position
	XMFLOAT2 m_vTex;		//Texcoord
};

const UINT g_nFrameBackBufCount = 3u;
// �Կ���������
struct ST_GRS_GPU_PARAMS
{
	UINT								m_nIndex;
	UINT								m_nszRTV;
	UINT								m_nszSRVCBVUAV;

	ComPtr<ID3D12Device4>				m_pID3DDevice;
	ComPtr<ID3D12CommandQueue>			m_pICmdQueue;
	ComPtr<ID3D12DescriptorHeap>		m_pIDHRTV;
	ComPtr<ID3D12Resource>				m_pIRTRes[g_nFrameBackBufCount];
	ComPtr<ID3D12CommandAllocator>		m_pICmdAllocPerFrame[g_nFrameBackBufCount];
	ComPtr<ID3D12GraphicsCommandList>	m_pICmdList;
	ComPtr<ID3D12Heap>					m_pICrossAdapterHeap;
	ComPtr<ID3D12Resource>				m_pICrossAdapterResPerFrame[g_nFrameBackBufCount];
	ComPtr<ID3D12Fence>					m_pIFence;
	ComPtr<ID3D12Fence>					m_pISharedFence;
	ComPtr<ID3D12Resource>				m_pIDSTex;
	ComPtr<ID3D12DescriptorHeap>		m_pIDHDSVTex;
};

// ����������
struct ST_GRS_MVP
{
	XMFLOAT4X4 m_MVP;			//�����Model-view-projection(MVP)����.
};

// �����������
struct ST_GRS_MODULE_PARAMS
{
	UINT								nIndex;
	XMFLOAT4							v4ModelPos;
	const TCHAR*						pszDDSFile;
	const CHAR*							pszMeshFile;
	UINT								nVertexCnt;
	UINT								nIndexCnt;
	ComPtr<ID3D12Resource>				pITexture;
	ComPtr<ID3D12Resource>				pITextureUpload;
	ComPtr<ID3D12DescriptorHeap>		pISampleHp;
	ComPtr<ID3D12Resource>				pIVertexBuffer;
	ComPtr<ID3D12Resource>				pIIndexsBuffer;
	ComPtr<ID3D12DescriptorHeap>		pISRVCBVHp;
	ComPtr<ID3D12Resource>			    pIConstBuffer;
	ComPtr<ID3D12CommandAllocator>		pIBundleAlloc;
	ComPtr<ID3D12GraphicsCommandList>	pIBundle;
	ST_GRS_MVP*							pMVPBuf;
	D3D12_VERTEX_BUFFER_VIEW			stVertexBufferView;
	D3D12_INDEX_BUFFER_VIEW				stIndexsBufferView;
};


//��ʼ��Ĭ���������λ��
XMFLOAT3 g_f3EyePos = XMFLOAT3(0.0f, 5.0f, -10.0f);  //�۾�λ��
XMFLOAT3 g_f3LockAt = XMFLOAT3(0.0f, 0.0f, 0.0f);    //�۾�������λ��
XMFLOAT3 g_f3HeapUp = XMFLOAT3(0.0f, 1.0f, 0.0f);    //ͷ�����Ϸ�λ��

float g_fYaw = 0.0f;								// ����Z�����ת��.
float g_fPitch = 0.0f;								// ��XZƽ�����ת��
double g_fPalstance = 10.0f * XM_PI / 180.0f;		//������ת�Ľ��ٶȣ���λ������/��


LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
BOOL LoadMeshVertex(const CHAR*pszMeshFileName, UINT&nVertexCnt, ST_GRS_VERTEX_MODULE*&ppVertex, UINT*&ppIndices);

int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR    lpCmdLine, int nCmdShow)
{
	::CoInitialize(nullptr);  //for WIC & COM
	try
	{
		int iWndWidth = 1024;
		int iWndHeight = 768;
		HWND hWnd = nullptr;
		MSG	msg = {};

		UINT nDXGIFactoryFlags = 0U;
		
		UINT nCurrentFrameIndex = 0u;
		DXGI_FORMAT emRTFmt = DXGI_FORMAT_R8G8B8A8_UNORM;
		DXGI_FORMAT	emDSFmt = DXGI_FORMAT_D24_UNORM_S8_UINT;

		CD3DX12_VIEWPORT					stViewPort(0.0f, 0.0f, static_cast<float>(iWndWidth), static_cast<float>(iWndHeight));
		CD3DX12_RECT						stScissorRect(0, 0, static_cast<LONG>(iWndWidth), static_cast<LONG>(iWndHeight));

		ComPtr<IDXGIFactory5>	pIDXGIFactory5;
		ComPtr<IDXGISwapChain1>	pISwapChain1;
		ComPtr<IDXGISwapChain3>	pISwapChain3;

		const UINT nMaxGPUParams = 2;			//Ŀǰ��֧������GPU������Ⱦ
		const UINT nIDGPUMain = 0;
		const UINT nIDGPUSecondary = 1;
		ST_GRS_GPU_PARAMS stGPUParams[nMaxGPUParams] = {};

		// �����������
		const UINT							nMaxObject = 3;
		const UINT							nSphere = 0;
		const UINT							nCube = 1;
		const UINT							nPlane = 2;
		ST_GRS_MODULE_PARAMS				stModuleParams[nMaxObject] = {};

		// ������������С�϶��뵽256Bytes�߽�
		SIZE_T								szMVPBuf = GRS_UPPER(sizeof(ST_GRS_MVP), 256);
		
		// ���ڸ�����Ⱦ����ĸ���������С�����������������б�����
		ComPtr<ID3D12CommandQueue> pICmdQueueCopy;
		ComPtr<ID3D12CommandAllocator> pICmdAllocCopyPerFrame[g_nFrameBackBufCount];
		ComPtr<ID3D12GraphicsCommandList> pICmdListCopy;

		//������ֱ�ӹ�����Դʱ�����ڸ����Կ��ϴ�����������Դ�����ڸ������Կ���Ⱦ�������
		ComPtr<ID3D12Resource>	pISecondaryAdapterTexutrePerFrame[g_nFrameBackBufCount];
		BOOL bCrossAdapterTextureSupport = FALSE;
		CD3DX12_RESOURCE_DESC stRenderTargetDesc = {};
		const float arf4ClearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

		ComPtr<ID3DBlob>					pIVSMain;
		ComPtr<ID3DBlob>					pIPSMain;
		ComPtr<ID3D12RootSignature>			pIRSMain;
		ComPtr<ID3D12PipelineState>			pIPSOMain;

		ComPtr<ID3DBlob>					pIVSQuad;
		ComPtr<ID3DBlob>					pIPSQuad;
		ComPtr<ID3D12RootSignature>			pIRSQuad;
		ComPtr<ID3D12PipelineState>			pIPSOQuad;

		ComPtr<ID3D12Resource>				pIVBQuad;
		ComPtr<ID3D12Resource>				pIVBQuadUpload;
		D3D12_VERTEX_BUFFER_VIEW			pstVBVQuad;

		ComPtr<ID3D12DescriptorHeap>		pIDHSRVSecondary;
		ComPtr<ID3D12DescriptorHeap>		pIDHSampleSecondary;

		HANDLE hFenceEvent = nullptr;

		//1����������
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
			wcex.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);		//��ֹ���ĵı����ػ�
			wcex.lpszClassName = GRS_WND_CLASS_NAME;
			RegisterClassEx(&wcex);

			DWORD dwWndStyle = WS_OVERLAPPED | WS_SYSMENU;
			RECT rtWnd = { 0, 0, iWndWidth, iWndHeight };
			AdjustWindowRect(&rtWnd, dwWndStyle, FALSE);

			// ���㴰�ھ��е���Ļ����
			INT posX = (GetSystemMetrics(SM_CXSCREEN) - rtWnd.right - rtWnd.left) / 2;
			INT posY = (GetSystemMetrics(SM_CYSCREEN) - rtWnd.bottom - rtWnd.top) / 2;

			hWnd = CreateWindowW(GRS_WND_CLASS_NAME, GRS_WND_TITLE, dwWndStyle
				, posX, posY, rtWnd.right - rtWnd.left, rtWnd.bottom - rtWnd.top
				, nullptr, nullptr, hInstance, nullptr);

			if (!hWnd)
			{
				throw CGRSCOMException(HRESULT_FROM_WIN32(GetLastError()));
			}

			ShowWindow(hWnd, nCmdShow);
			UpdateWindow(hWnd);
		}

		//2������ʾ��ϵͳ�ĵ���֧��
		{
#if defined(_DEBUG)
			ComPtr<ID3D12Debug> debugController;
			if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
			{
				debugController->EnableDebugLayer();
				// �򿪸��ӵĵ���֧��
				nDXGIFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
			}
#endif
		}

		//3������DXGI Factory����
		{
			GRS_THROW_IF_FAILED(CreateDXGIFactory2(nDXGIFactoryFlags, IID_PPV_ARGS(&pIDXGIFactory5)));
			GRS_SET_DXGI_DEBUGNAME_COMPTR(pIDXGIFactory5);
			// �ر�ALT+ENTER���л�ȫ���Ĺ��ܣ���Ϊ����û��ʵ��OnSize�����������ȹر�
			GRS_THROW_IF_FAILED(pIDXGIFactory5->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER));
		}

		//4��ö�������������豸
		{//�ж������Կ��ķ�������DirectX12 ʾ���еķ�����ͬ������ʹ�õ���΢׼ȷһЩ
		 //���������õľ������0Ĭ���Ǽ���1�Ƕ���Ȼ�����δ���

			D3D12_FEATURE_DATA_ARCHITECTURE stArchitecture = {};
			IDXGIAdapter1*	pIAdapterTmp	= nullptr;
			ID3D12Device4*	pID3DDeviceTmp	= nullptr;
			IDXGIOutput*	pIOutput		= nullptr;
			HRESULT			hrEnumOutput	= S_OK;

			for ( UINT nAdapterIndex = 0; DXGI_ERROR_NOT_FOUND != pIDXGIFactory5->EnumAdapters1(nAdapterIndex, &pIAdapterTmp); ++ nAdapterIndex)
			{
				DXGI_ADAPTER_DESC1 stAdapterDesc = {};
				pIAdapterTmp->GetDesc1(&stAdapterDesc);

				if (stAdapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
				{//�������������������豸
					GRS_SAFE_RELEASE(pIAdapterTmp);
					continue;
				}

				//��һ�ַ�����ͨ���ж��Ǹ��Կ��������
				hrEnumOutput = pIAdapterTmp->EnumOutputs(0, &pIOutput);

				if ( SUCCEEDED(hrEnumOutput) && nullptr != pIOutput)
				{//��������������ʾ�����ͨ���Ǽ��ԣ���ԱʼǱ��������
					//���ǽ����Գ�ΪMain Device����Ϊ�������������������
					GRS_THROW_IF_FAILED(D3D12CreateDevice(pIAdapterTmp, D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&stGPUParams[nIDGPUMain].m_pID3DDevice)));
				}
				else
				{//������ʾ����ģ�ͨ���Ƕ��ԣ���ԱʼǱ��������
					//�����ö����������������Ⱦ����Ȼ��������Ⱦ������������ῴ������ʹ�õ��ǹ����Դ������
					GRS_THROW_IF_FAILED(D3D12CreateDevice(pIAdapterTmp, D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&stGPUParams[nIDGPUSecondary].m_pID3DDevice)));
				}
			
				GRS_SAFE_RELEASE(pIOutput);


				//�ڶ����ж������Կ��ķ��������ǿ�˭��UMA��˭���ǣ������֮ǰ�Ľ̳�ʾ�����Ѿ���ϸ�����

				//GRS_THROW_IF_FAILED(D3D12CreateDevice(pIAdapterTmp, D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&pID3DDeviceTmp)));
				//GRS_THROW_IF_FAILED(pID3DDeviceTmp->CheckFeatureSupport(D3D12_FEATURE_ARCHITECTURE
				//	, &stArchitecture, sizeof(D3D12_FEATURE_DATA_ARCHITECTURE)));
				////��������ͨ���ж��Ƿ�UMA�ܹ�������˭��˭��
				//if ( stArchitecture.UMA )
				//{
				//	if ( nullptr == pID3DDeviceSecondary.Get() )
				//	{
				//		pID3DDeviceSecondary.Attach(pID3DDeviceTmp);
				//		pID3DDeviceTmp = nullptr;
				//	}
				//	else
				//	{
				//		//����Կ�̫���ˣ����Լ�����ô�ðɣ���������������
				//	}
				//}
				//else
				//{
				//	if ( nullptr == pID3DDeviceMain.Get() )
				//	{
				//		pID3DDeviceMain.Attach(pID3DDeviceTmp);
				//		pID3DDeviceTmp = nullptr;
				//	}
				//	else
				//	{
				//		//����Կ�̫���ˣ����Լ�����ô�ðɣ���������������
				//	}

				//}

				//GRS_SAFE_RELEASE(pID3DDeviceTmp);

				GRS_SAFE_RELEASE(pIAdapterTmp);
			}

			//---------------------------------------------------------------------------------------------
			if ( nullptr == stGPUParams[nIDGPUMain].m_pID3DDevice.Get() || nullptr == stGPUParams[nIDGPUSecondary].m_pID3DDevice.Get() )
			{// �����Ļ����Ͼ�Ȼû���������ϵ��Կ� �������˳����� ��Ȼ�����ʹ�����������ջ������
				throw CGRSCOMException(E_FAIL);
			}

			GRS_SET_D3D12_DEBUGNAME_COMPTR(stGPUParams[nIDGPUMain].m_pID3DDevice);
			GRS_SET_D3D12_DEBUGNAME_COMPTR(stGPUParams[nIDGPUSecondary].m_pID3DDevice);
		}

		//5�������������
		{
			D3D12_COMMAND_QUEUE_DESC stQueueDesc = {};
			stQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
			for (int i = 0; i < nMaxGPUParams; i++)
			{
				GRS_THROW_IF_FAILED(stGPUParams[i].m_pID3DDevice->CreateCommandQueue(&stQueueDesc, IID_PPV_ARGS(&stGPUParams[i].m_pICmdQueue)));
				GRS_SetD3D12DebugNameIndexed(stGPUParams[i].m_pICmdQueue.Get(), _T("m_pICmdQueue"), i);

				//�������ѭ����ɸ��������ĸ�ֵ��ע�����������ϵ�ÿ���������Ĵ�С������ͬ��GPU����ͬ�����Ե�����ΪGPU�Ĳ���
				//ͬʱ����Щֵһ����ȡ�����洢���Ͳ���ÿ�ε��ú�����������������͵��ô����������������
				stGPUParams[i].m_nIndex = i;
				stGPUParams[i].m_nszRTV = stGPUParams[i].m_pID3DDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
				stGPUParams[i].m_nszSRVCBVUAV = stGPUParams[i].m_pID3DDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			}

			//����������д����ڶ����ϣ���Ϊ���Ե�����ǿ��һЩ
			stQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
			GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pID3DDevice->CreateCommandQueue(&stQueueDesc, IID_PPV_ARGS(&pICmdQueueCopy)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pICmdQueueCopy);
		}

		//6������������
		{
			DXGI_SWAP_CHAIN_DESC1 stSwapChainDesc = {};
			stSwapChainDesc.BufferCount = g_nFrameBackBufCount;
			stSwapChainDesc.Width = iWndWidth;
			stSwapChainDesc.Height = iWndHeight;
			stSwapChainDesc.Format = emRTFmt;
			stSwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			stSwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
			stSwapChainDesc.SampleDesc.Count = 1;

			GRS_THROW_IF_FAILED(pIDXGIFactory5->CreateSwapChainForHwnd(
				stGPUParams[nIDGPUSecondary].m_pICmdQueue.Get(),		// ʹ�ýӲ�����ʾ�����Կ������������Ϊ���������������
				hWnd,
				&stSwapChainDesc,
				nullptr,
				nullptr,
				&pISwapChain1
			));
			GRS_SET_DXGI_DEBUGNAME_COMPTR(pISwapChain1);

			//ע��˴�ʹ���˸߰汾��SwapChain�ӿڵĺ���
			GRS_THROW_IF_FAILED(pISwapChain1.As(&pISwapChain3));
			GRS_SET_DXGI_DEBUGNAME_COMPTR(pISwapChain3);

			// ��ȡ��ǰ��һ�������Ƶĺ󻺳����
			nCurrentFrameIndex = pISwapChain3->GetCurrentBackBufferIndex();

			//����RTV(��ȾĿ����ͼ)��������(����ѵĺ���Ӧ������Ϊ������߹̶���СԪ�صĹ̶���С�Դ��)
			D3D12_DESCRIPTOR_HEAP_DESC		stRTVHeapDesc = {};
			stRTVHeapDesc.NumDescriptors	= g_nFrameBackBufCount;
			stRTVHeapDesc.Type				= D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
			stRTVHeapDesc.Flags				= D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

			for (int i = 0; i < nMaxGPUParams; i++)
			{
				GRS_THROW_IF_FAILED(stGPUParams[i].m_pID3DDevice->CreateDescriptorHeap(&stRTVHeapDesc, IID_PPV_ARGS(&stGPUParams[i].m_pIDHRTV)));
				GRS_SetD3D12DebugNameIndexed(stGPUParams[i].m_pIDHRTV.Get(), _T("m_pIDHRTV"), i);
			}
			CD3DX12_CLEAR_VALUE   stClearValue(stSwapChainDesc.Format, arf4ClearColor);
			stRenderTargetDesc = CD3DX12_RESOURCE_DESC::Tex2D(
				stSwapChainDesc.Format,
				stSwapChainDesc.Width,
				stSwapChainDesc.Height,
				1u, 1u,
				stSwapChainDesc.SampleDesc.Count,
				stSwapChainDesc.SampleDesc.Quality,
				D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
				D3D12_TEXTURE_LAYOUT_UNKNOWN, 0u);

			for ( UINT iGPUIndex = 0; iGPUIndex < nMaxGPUParams; iGPUIndex++ )
			{
				CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(stGPUParams[iGPUIndex].m_pIDHRTV->GetCPUDescriptorHandleForHeapStart());

				for (UINT j = 0; j < g_nFrameBackBufCount; j++)
				{
					if ( iGPUIndex == nIDGPUSecondary )
					{
						GRS_THROW_IF_FAILED(pISwapChain3->GetBuffer(j, IID_PPV_ARGS(&stGPUParams[iGPUIndex].m_pIRTRes[j])));
					}
					else
					{
						GRS_THROW_IF_FAILED(stGPUParams[iGPUIndex].m_pID3DDevice->CreateCommittedResource(
							&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
							D3D12_HEAP_FLAG_NONE,
							&stRenderTargetDesc,
							D3D12_RESOURCE_STATE_COMMON,
							&stClearValue,
							IID_PPV_ARGS(&stGPUParams[iGPUIndex].m_pIRTRes[j])));
					}

					stGPUParams[iGPUIndex].m_pID3DDevice->CreateRenderTargetView(stGPUParams[iGPUIndex].m_pIRTRes[j].Get(), nullptr, rtvHandle);
					rtvHandle.Offset(1, stGPUParams[iGPUIndex].m_nszRTV);

					GRS_THROW_IF_FAILED(stGPUParams[iGPUIndex].m_pID3DDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT
						, IID_PPV_ARGS(&stGPUParams[iGPUIndex].m_pICmdAllocPerFrame[j])));

					if ( iGPUIndex == nIDGPUMain )
					{ //�������Կ��ϵĸ�����������õ������������ÿ֡ʹ��һ��������
						GRS_THROW_IF_FAILED( stGPUParams[iGPUIndex].m_pID3DDevice->CreateCommandAllocator( D3D12_COMMAND_LIST_TYPE_COPY
							, IID_PPV_ARGS(&pICmdAllocCopyPerFrame[j])));
					}
				}

				// ����ÿ��GPU�������б�����
				GRS_THROW_IF_FAILED(stGPUParams[iGPUIndex].m_pID3DDevice->CreateCommandList(0
					, D3D12_COMMAND_LIST_TYPE_DIRECT
					, stGPUParams[iGPUIndex].m_pICmdAllocPerFrame[nCurrentFrameIndex].Get()
					, nullptr
					, IID_PPV_ARGS(&stGPUParams[iGPUIndex].m_pICmdList)));

				if ( iGPUIndex == nIDGPUMain )
				{// �����Կ��ϴ������������б�����
					GRS_THROW_IF_FAILED(stGPUParams[iGPUIndex].m_pID3DDevice->CreateCommandList(0
						, D3D12_COMMAND_LIST_TYPE_COPY
						, pICmdAllocCopyPerFrame[nCurrentFrameIndex].Get()
						, nullptr
						, IID_PPV_ARGS(&pICmdListCopy)));

				}

				// ����ÿ���Կ��ϵ�������建����
				D3D12_CLEAR_VALUE stDepthOptimizedClearValue = {};
				stDepthOptimizedClearValue.Format = emDSFmt;
				stDepthOptimizedClearValue.DepthStencil.Depth = 1.0f;
				stDepthOptimizedClearValue.DepthStencil.Stencil = 0;

				//ʹ����ʽĬ�϶Ѵ���һ��������建������
				//��Ϊ��������Ȼ�������һֱ��ʹ�ã����õ����岻������ֱ��ʹ����ʽ�ѣ�ͼ����
				GRS_THROW_IF_FAILED(stGPUParams[iGPUIndex].m_pID3DDevice->CreateCommittedResource(
					&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT)
					, D3D12_HEAP_FLAG_NONE
					, &CD3DX12_RESOURCE_DESC::Tex2D(
						emDSFmt
						, iWndWidth
						, iWndHeight
						, 1
						, 0
						, 1
						, 0
						, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
					, D3D12_RESOURCE_STATE_DEPTH_WRITE
					, &stDepthOptimizedClearValue
					, IID_PPV_ARGS(&stGPUParams[iGPUIndex].m_pIDSTex)
				));
				GRS_SetD3D12DebugNameIndexed(stGPUParams[iGPUIndex].m_pIDSTex.Get(), _T("m_pIDSTex"), iGPUIndex);

				D3D12_DEPTH_STENCIL_VIEW_DESC stDepthStencilDesc = {};
				stDepthStencilDesc.Format = emDSFmt;
				stDepthStencilDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
				stDepthStencilDesc.Flags = D3D12_DSV_FLAG_NONE;

				D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
				dsvHeapDesc.NumDescriptors = 1;
				dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
				dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
				GRS_THROW_IF_FAILED(stGPUParams[iGPUIndex].m_pID3DDevice->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&stGPUParams[iGPUIndex].m_pIDHDSVTex)));
				GRS_SetD3D12DebugNameIndexed(stGPUParams[iGPUIndex].m_pIDHDSVTex.Get(), _T("m_pIDHDSVTex"), iGPUIndex);

				stGPUParams[iGPUIndex].m_pID3DDevice->CreateDepthStencilView(stGPUParams[iGPUIndex].m_pIDSTex.Get()
					, &stDepthStencilDesc
					, stGPUParams[iGPUIndex].m_pIDHDSVTex->GetCPUDescriptorHandleForHeapStart());
			}

		}

		//7���������������Ĺ�����Դ��
		{
		{
			D3D12_FEATURE_DATA_D3D12_OPTIONS stOptions = {};
			// ͨ����������ʾ������Կ��Ƿ�֧�ֿ��Կ���Դ���������Կ�����Դ��δ���
			GRS_THROW_IF_FAILED( stGPUParams[nIDGPUSecondary].m_pID3DDevice->CheckFeatureSupport(
				D3D12_FEATURE_D3D12_OPTIONS, reinterpret_cast<void*>(&stOptions), sizeof(stOptions)));

			bCrossAdapterTextureSupport = stOptions.CrossAdapterRowMajorTextureSupported;

			UINT64 n64szTexture = 0;
			D3D12_RESOURCE_DESC stCrossAdapterResDesc = {};

			if ( bCrossAdapterTextureSupport )
			{
				// ���֧����ôֱ�Ӵ������Կ���Դ��
				stCrossAdapterResDesc = stRenderTargetDesc;
				stCrossAdapterResDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER;
				stCrossAdapterResDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

				D3D12_RESOURCE_ALLOCATION_INFO stTextureInfo = stGPUParams[nIDGPUMain].m_pID3DDevice->GetResourceAllocationInfo(0, 1, &stCrossAdapterResDesc);
				n64szTexture = stTextureInfo.SizeInBytes;
			}
			else
			{
				// �����֧�֣���ô���Ǿ���Ҫ�������Կ��ϴ������ڸ�����Ⱦ�������Դ�ѣ�Ȼ���ٹ����ѵ������Կ���
				D3D12_PLACED_SUBRESOURCE_FOOTPRINT stResLayout = {};
				stGPUParams[nIDGPUMain].m_pID3DDevice->GetCopyableFootprints(&stRenderTargetDesc, 0, 1, 0, &stResLayout, nullptr, nullptr, nullptr);
				n64szTexture = GRS_UPPER(stResLayout.Footprint.RowPitch * stResLayout.Footprint.Height, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
				stCrossAdapterResDesc = CD3DX12_RESOURCE_DESC::Buffer(n64szTexture, D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER);
			}

			// �������Կ���������Դ��
			CD3DX12_HEAP_DESC stCrossHeapDesc(
				n64szTexture * g_nFrameBackBufCount,
				D3D12_HEAP_TYPE_DEFAULT,
				0,
				D3D12_HEAP_FLAG_SHARED | D3D12_HEAP_FLAG_SHARED_CROSS_ADAPTER);

			GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pID3DDevice->CreateHeap(&stCrossHeapDesc
				, IID_PPV_ARGS(&stGPUParams[nIDGPUMain].m_pICrossAdapterHeap)));
		
			HANDLE hHeap = nullptr;
			GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pID3DDevice->CreateSharedHandle(
				stGPUParams[nIDGPUMain].m_pICrossAdapterHeap.Get(),
				nullptr,
				GENERIC_ALL,
				nullptr,
				&hHeap));

			HRESULT hrOpenSharedHandle = stGPUParams[nIDGPUSecondary].m_pID3DDevice->OpenSharedHandle(hHeap
				, IID_PPV_ARGS(&stGPUParams[nIDGPUSecondary].m_pICrossAdapterHeap));

			// �ȹرվ�������ж��Ƿ����ɹ�
			::CloseHandle(hHeap);

			GRS_THROW_IF_FAILED(hrOpenSharedHandle);

			// �Զ�λ��ʽ�ڹ������ϴ���ÿ���Կ��ϵ���Դ
			for ( UINT nFrameIndex = 0; nFrameIndex < g_nFrameBackBufCount; nFrameIndex++ )
			{
				GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pID3DDevice->CreatePlacedResource(
					stGPUParams[nIDGPUMain].m_pICrossAdapterHeap.Get(),
					n64szTexture * nFrameIndex,
					&stCrossAdapterResDesc,
					D3D12_RESOURCE_STATE_COPY_DEST,
					nullptr,
					IID_PPV_ARGS(&stGPUParams[nIDGPUMain].m_pICrossAdapterResPerFrame[nFrameIndex])));

				GRS_THROW_IF_FAILED(stGPUParams[nIDGPUSecondary].m_pID3DDevice->CreatePlacedResource(
					stGPUParams[nIDGPUSecondary].m_pICrossAdapterHeap.Get(),
					n64szTexture * nFrameIndex,
					&stCrossAdapterResDesc,
					bCrossAdapterTextureSupport ? D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE : D3D12_RESOURCE_STATE_COPY_SOURCE,
					nullptr,
					IID_PPV_ARGS(&stGPUParams[nIDGPUSecondary].m_pICrossAdapterResPerFrame[nFrameIndex])));

				if ( ! bCrossAdapterTextureSupport )
				{
					// If the primary adapter's render target must be shared as a buffer,
					// create a texture resource to copy it into on the secondary adapter.
					GRS_THROW_IF_FAILED(stGPUParams[nIDGPUSecondary].m_pID3DDevice->CreateCommittedResource(
						&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
						D3D12_HEAP_FLAG_NONE,
						&stRenderTargetDesc,
						D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
						nullptr,
						IID_PPV_ARGS(&pISecondaryAdapterTexutrePerFrame[nFrameIndex])));
				}
			}

		}
		}
		//8��������ǩ��
		{//��������У���������ʹ����ͬ�ĸ�ǩ������Ϊ��Ⱦ��������Ҫ�Ĳ�����һ����
			D3D12_FEATURE_DATA_ROOT_SIGNATURE stFeatureData = {};

			// ����Ƿ�֧��V1.1�汾�ĸ�ǩ��
			stFeatureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
			if (FAILED(stGPUParams[nIDGPUMain].m_pID3DDevice->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &stFeatureData, sizeof(stFeatureData))))
			{
				stFeatureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
			}

			CD3DX12_DESCRIPTOR_RANGE1 stDSPRanges[3];
			stDSPRanges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
			stDSPRanges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
			stDSPRanges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0);

			CD3DX12_ROOT_PARAMETER1 stRootParameters[3];
			stRootParameters[0].InitAsDescriptorTable(1, &stDSPRanges[0], D3D12_SHADER_VISIBILITY_ALL); //CBV������Shader�ɼ�
			stRootParameters[1].InitAsDescriptorTable(1, &stDSPRanges[1], D3D12_SHADER_VISIBILITY_PIXEL);//SRV��PS�ɼ�
			stRootParameters[2].InitAsDescriptorTable(1, &stDSPRanges[2], D3D12_SHADER_VISIBILITY_PIXEL);//SAMPLE��PS�ɼ�

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

			GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pID3DDevice->CreateRootSignature(0
				, pISignatureBlob->GetBufferPointer()
				, pISignatureBlob->GetBufferSize()
				, IID_PPV_ARGS(&pIRSMain)));

			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIRSMain);

			//------------------------------------------------------------------------------------------------

			// ������ȾQuad�ĸ�ǩ������ע�������ǩ���ڸ����Կ�����
			stFeatureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
			if (FAILED(stGPUParams[nIDGPUSecondary].m_pID3DDevice->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &stFeatureData, sizeof(stFeatureData))))
			{
				stFeatureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
			}

			CD3DX12_DESCRIPTOR_RANGE1 stDRQuad[2];
			stDRQuad[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
			stDRQuad[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0);

			CD3DX12_ROOT_PARAMETER1 stRPQuad[2];
			stRPQuad[0].InitAsDescriptorTable(1, &stDRQuad[0], D3D12_SHADER_VISIBILITY_PIXEL);//SRV��PS�ɼ�
			stRPQuad[1].InitAsDescriptorTable(1, &stDRQuad[1], D3D12_SHADER_VISIBILITY_PIXEL);//SAMPLE��PS�ɼ�

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

			GRS_THROW_IF_FAILED(stGPUParams[nIDGPUSecondary].m_pID3DDevice->CreateRootSignature(0
				, pISignatureBlob->GetBufferPointer()
				, pISignatureBlob->GetBufferSize()
				, IID_PPV_ARGS(&pIRSQuad)));

			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIRSQuad);			
		}

		//9������Shader������Ⱦ����״̬����
		{
#if defined(_DEBUG)
			// Enable better shader debugging with the graphics debugging tools.
			UINT nShaderCompileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
			UINT nShaderCompileFlags = 0;
#endif
			//����Ϊ�о�����ʽ	   
			nShaderCompileFlags |= D3DCOMPILE_PACK_MATRIX_ROW_MAJOR;

			TCHAR pszShaderFileName[] = _T("D:\\Projects_2018_08\\D3D12 Tutorials\\7-D3D12MultiAdapter\\Shader\\TextureModule.hlsl");

			GRS_THROW_IF_FAILED(D3DCompileFromFile(pszShaderFileName, nullptr, nullptr
				, "VSMain", "vs_5_0", nShaderCompileFlags, 0, &pIVSMain, nullptr));
			GRS_THROW_IF_FAILED(D3DCompileFromFile(pszShaderFileName, nullptr, nullptr
				, "PSMain", "ps_5_0", nShaderCompileFlags, 0, &pIPSMain, nullptr));

			// ���Ƕ�������һ�����ߵĶ��壬��ĿǰShader�����ǲ�û��ʹ��
			D3D12_INPUT_ELEMENT_DESC stIALayoutSphere[] =
			{
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
				{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
				{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
			};

			// ���� graphics pipeline state object (PSO)����
			D3D12_GRAPHICS_PIPELINE_STATE_DESC stPSODesc = {};
			stPSODesc.InputLayout = { stIALayoutSphere, _countof(stIALayoutSphere) };
			stPSODesc.pRootSignature = pIRSMain.Get();
			stPSODesc.VS = CD3DX12_SHADER_BYTECODE(pIVSMain.Get());
			stPSODesc.PS = CD3DX12_SHADER_BYTECODE(pIPSMain.Get());
			stPSODesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
			stPSODesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
			stPSODesc.SampleMask = UINT_MAX;
			stPSODesc.SampleDesc.Count = 1;
			stPSODesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			stPSODesc.NumRenderTargets = 1;
			stPSODesc.RTVFormats[0] = emRTFmt;
			stPSODesc.DepthStencilState.StencilEnable = FALSE;
			stPSODesc.DepthStencilState.DepthEnable = TRUE;			//����Ȼ���		
			stPSODesc.DSVFormat = emDSFmt;
			stPSODesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;//������Ȼ���д�빦��
			stPSODesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;     //��Ȳ��Ժ�������ֵΪ��ͨ����Ȳ��ԣ�����Сֵд�룩				

			GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pID3DDevice->CreateGraphicsPipelineState(&stPSODesc
				, IID_PPV_ARGS(&pIPSOMain)));

			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIPSOMain);

			//---------------------------------------------------------------------------------------------------
			//������Ⱦ��λ���εĹܵ�״̬����
			TCHAR pszUnitQuadShader[] = _T("D:\\Projects_2018_08\\D3D12 Tutorials\\7-D3D12MultiAdapter\\Shader\\UnitQuad.hlsl");

			GRS_THROW_IF_FAILED(D3DCompileFromFile(pszUnitQuadShader, nullptr, nullptr
				, "VSMain", "vs_5_0", nShaderCompileFlags, 0, &pIVSQuad, nullptr));
			GRS_THROW_IF_FAILED(D3DCompileFromFile(pszUnitQuadShader, nullptr, nullptr
				, "PSMain", "ps_5_0", nShaderCompileFlags, 0, &pIPSQuad, nullptr));

			// ���Ƕ�������һ�����ߵĶ��壬��ĿǰShader�����ǲ�û��ʹ��
			D3D12_INPUT_ELEMENT_DESC stIALayoutQuad[] =
			{
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
				{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
			};

			// ���� graphics pipeline state object (PSO)����
			D3D12_GRAPHICS_PIPELINE_STATE_DESC stPSOQuadDesc = {};
			stPSOQuadDesc.InputLayout = { stIALayoutQuad, _countof(stIALayoutQuad) };
			stPSOQuadDesc.pRootSignature = pIRSQuad.Get();
			stPSOQuadDesc.VS = CD3DX12_SHADER_BYTECODE(pIVSQuad.Get());
			stPSOQuadDesc.PS = CD3DX12_SHADER_BYTECODE(pIPSQuad.Get());
			stPSOQuadDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
			stPSOQuadDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
			stPSOQuadDesc.SampleMask = UINT_MAX;
			stPSOQuadDesc.SampleDesc.Count = 1;
			stPSOQuadDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			stPSOQuadDesc.NumRenderTargets = 1;
			stPSOQuadDesc.RTVFormats[0] = emRTFmt;
			stPSOQuadDesc.DepthStencilState.StencilEnable = FALSE;
			stPSOQuadDesc.DepthStencilState.DepthEnable = FALSE;

			GRS_THROW_IF_FAILED(stGPUParams[nIDGPUSecondary].m_pID3DDevice->CreateGraphicsPipelineState(&stPSOQuadDesc
				, IID_PPV_ARGS(&pIPSOQuad)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIPSOQuad);
		}

		//10��׼������������
		{
			// ������Բ���
			stModuleParams[nSphere].pszDDSFile = _T("D:\\Projects_2018_08\\D3D12 Tutorials\\7-D3D12MultiAdapter\\Mesh\\sphere.dds");
			stModuleParams[nSphere].pszMeshFile = "D:\\Projects_2018_08\\D3D12 Tutorials\\7-D3D12MultiAdapter\\Mesh\\sphere.txt";
			stModuleParams[nSphere].v4ModelPos = XMFLOAT4(2.0f, 2.0f, 0.0f, 1.0f);

			// ��������Բ���
			stModuleParams[nCube].pszDDSFile = _T("D:\\Projects_2018_08\\D3D12 Tutorials\\7-D3D12MultiAdapter\\Mesh\\Cube.dds");
			stModuleParams[nCube].pszMeshFile = "D:\\Projects_2018_08\\D3D12 Tutorials\\7-D3D12MultiAdapter\\Mesh\\Cube.txt";
			stModuleParams[nCube].v4ModelPos = XMFLOAT4(-2.0f, 2.0f, 0.0f, 1.0f);

			// ƽ����Բ���
			stModuleParams[nPlane].pszDDSFile = _T("D:\\Projects_2018_08\\D3D12 Tutorials\\7-D3D12MultiAdapter\\Mesh\\Plane.dds");
			stModuleParams[nPlane].pszMeshFile = "D:\\Projects_2018_08\\D3D12 Tutorials\\7-D3D12MultiAdapter\\Mesh\\Plane.txt";
			stModuleParams[nPlane].v4ModelPos = XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f);

			// Mesh Value
			ST_GRS_VERTEX_MODULE*				pstVertices = nullptr;
			UINT*								pnIndices = nullptr;
			// DDS Value
			std::unique_ptr<uint8_t[]>			pbDDSData;
			std::vector<D3D12_SUBRESOURCE_DATA> stArSubResources;
			DDS_ALPHA_MODE						emAlphaMode = DDS_ALPHA_MODE_UNKNOWN;
			bool								bIsCube = false;

			// ��������Ĺ��Բ�����Ҳ���Ǹ��̵߳Ĺ��Բ���
			for (int i = 0; i < nMaxObject; i++)
			{
				stModuleParams[i].nIndex = i;

				//����ÿ��Module�������
				GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pID3DDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_BUNDLE
					, IID_PPV_ARGS(&stModuleParams[i].pIBundleAlloc)));
				GRS_SetD3D12DebugNameIndexed(stModuleParams[i].pIBundleAlloc.Get(), _T("pIBundleAlloc"), i);

				GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pID3DDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_BUNDLE
					, stModuleParams[i].pIBundleAlloc.Get(), nullptr, IID_PPV_ARGS(&stModuleParams[i].pIBundle)));
				GRS_SetD3D12DebugNameIndexed(stModuleParams[i].pIBundle.Get(), _T("pIBundle"), i);

				//����DDS
				pbDDSData.release();
				stArSubResources.clear();
				GRS_THROW_IF_FAILED(LoadDDSTextureFromFile(stGPUParams[nIDGPUMain].m_pID3DDevice.Get()
					, stModuleParams[i].pszDDSFile
					, stModuleParams[i].pITexture.GetAddressOf()
					, pbDDSData
					, stArSubResources
					, SIZE_MAX
					, &emAlphaMode
					, &bIsCube));

				GRS_SetD3D12DebugNameIndexed(stModuleParams[i].pITexture.Get(), _T("pITexture"), i);

				UINT64 n64szUpSphere = GetRequiredIntermediateSize(
					stModuleParams[i].pITexture.Get()
					, 0
					, static_cast<UINT>(stArSubResources.size()));
				D3D12_RESOURCE_DESC stTXDesc = stModuleParams[i].pITexture->GetDesc();

				//�����ϴ���
				GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pID3DDevice->CreateCommittedResource(
					&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD)
					, D3D12_HEAP_FLAG_NONE
					, &CD3DX12_RESOURCE_DESC::Buffer(n64szUpSphere)
					, D3D12_RESOURCE_STATE_GENERIC_READ
					, nullptr
					, IID_PPV_ARGS(&stModuleParams[i].pITextureUpload)));
				GRS_SetD3D12DebugNameIndexed(stModuleParams[i].pITextureUpload.Get(), _T("pITextureUpload"), i);

				//�ϴ�DDS
				UpdateSubresources(stGPUParams[nIDGPUMain].m_pICmdList.Get()
					, stModuleParams[i].pITexture.Get()
					, stModuleParams[i].pITextureUpload.Get()
					, 0
					, 0
					, static_cast<UINT>(stArSubResources.size())
					, stArSubResources.data());

				//ͬ��
				stGPUParams[nIDGPUMain].m_pICmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
					stModuleParams[i].pITexture.Get()
					, D3D12_RESOURCE_STATE_COPY_DEST
					, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));


				//����SRV CBV��
				D3D12_DESCRIPTOR_HEAP_DESC stSRVHeapDesc = {};
				stSRVHeapDesc.NumDescriptors = 2; //1 SRV + 1 CBV
				stSRVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
				stSRVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

				GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pID3DDevice->CreateDescriptorHeap(&stSRVHeapDesc
					, IID_PPV_ARGS(&stModuleParams[i].pISRVCBVHp)));
				GRS_SetD3D12DebugNameIndexed(stModuleParams[i].pISRVCBVHp.Get(), _T("pISRVCBVHp"), i);

				//����SRV
				D3D12_SHADER_RESOURCE_VIEW_DESC stSRVDesc = {};
				stSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
				stSRVDesc.Format = stTXDesc.Format;
				stSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
				stSRVDesc.Texture2D.MipLevels = 1;

				CD3DX12_CPU_DESCRIPTOR_HANDLE stCBVSRVHandle(
					stModuleParams[i].pISRVCBVHp->GetCPUDescriptorHandleForHeapStart()
					, 1
					, stGPUParams[nIDGPUMain].m_pID3DDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));

				stGPUParams[nIDGPUMain].m_pID3DDevice->CreateShaderResourceView(stModuleParams[i].pITexture.Get()
					, &stSRVDesc
					, stCBVSRVHandle);

				// ����Sample
				D3D12_DESCRIPTOR_HEAP_DESC stSamplerHeapDesc = {};
				stSamplerHeapDesc.NumDescriptors = 1;
				stSamplerHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
				stSamplerHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

				GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pID3DDevice->CreateDescriptorHeap(&stSamplerHeapDesc
					, IID_PPV_ARGS(&stModuleParams[i].pISampleHp)));
				GRS_SetD3D12DebugNameIndexed(stModuleParams[i].pISampleHp.Get(), _T("pISampleHp"), i);

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

				stGPUParams[nIDGPUMain].m_pID3DDevice->CreateSampler(&stSamplerDesc
					, stModuleParams[i].pISampleHp->GetCPUDescriptorHandleForHeapStart());

				//������������
				LoadMeshVertex(stModuleParams[i].pszMeshFile, stModuleParams[i].nVertexCnt, pstVertices, pnIndices);
				stModuleParams[i].nIndexCnt = stModuleParams[i].nVertexCnt;

				//���� Vertex Buffer ��ʹ��Upload��ʽ��
				GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pID3DDevice->CreateCommittedResource(
					&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD)
					, D3D12_HEAP_FLAG_NONE
					, &CD3DX12_RESOURCE_DESC::Buffer(stModuleParams[i].nVertexCnt * sizeof(ST_GRS_VERTEX_MODULE))
					, D3D12_RESOURCE_STATE_GENERIC_READ
					, nullptr
					, IID_PPV_ARGS(&stModuleParams[i].pIVertexBuffer)));
				GRS_SetD3D12DebugNameIndexed(stModuleParams[i].pIVertexBuffer.Get(), _T("pIVertexBuffer"), i);

				//ʹ��map-memcpy-unmap�󷨽����ݴ������㻺�����
				UINT8* pVertexDataBegin = nullptr;
				CD3DX12_RANGE stReadRange(0, 0);		// We do not intend to read from this resource on the CPU.

				GRS_THROW_IF_FAILED(stModuleParams[i].pIVertexBuffer->Map(0, &stReadRange, reinterpret_cast<void**>(&pVertexDataBegin)));
				memcpy(pVertexDataBegin, pstVertices, stModuleParams[i].nVertexCnt * sizeof(ST_GRS_VERTEX_MODULE));
				stModuleParams[i].pIVertexBuffer->Unmap(0, nullptr);

				//���� Index Buffer ��ʹ��Upload��ʽ��
				GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pID3DDevice->CreateCommittedResource(
					&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD)
					, D3D12_HEAP_FLAG_NONE
					, &CD3DX12_RESOURCE_DESC::Buffer(stModuleParams[i].nIndexCnt * sizeof(UINT))
					, D3D12_RESOURCE_STATE_GENERIC_READ
					, nullptr
					, IID_PPV_ARGS(&stModuleParams[i].pIIndexsBuffer)));
				GRS_SetD3D12DebugNameIndexed(stModuleParams[i].pIIndexsBuffer.Get(), _T("pIIndexsBuffer"), i);

				UINT8* pIndexDataBegin = nullptr;
				GRS_THROW_IF_FAILED(stModuleParams[i].pIIndexsBuffer->Map(0, &stReadRange, reinterpret_cast<void**>(&pIndexDataBegin)));
				memcpy(pIndexDataBegin, pnIndices, stModuleParams[i].nIndexCnt * sizeof(UINT));
				stModuleParams[i].pIIndexsBuffer->Unmap(0, nullptr);

				//����Vertex Buffer View
				stModuleParams[i].stVertexBufferView.BufferLocation = stModuleParams[i].pIVertexBuffer->GetGPUVirtualAddress();
				stModuleParams[i].stVertexBufferView.StrideInBytes = sizeof(ST_GRS_VERTEX_MODULE);
				stModuleParams[i].stVertexBufferView.SizeInBytes = stModuleParams[i].nVertexCnt * sizeof(ST_GRS_VERTEX_MODULE);

				//����Index Buffer View
				stModuleParams[i].stIndexsBufferView.BufferLocation = stModuleParams[i].pIIndexsBuffer->GetGPUVirtualAddress();
				stModuleParams[i].stIndexsBufferView.Format = DXGI_FORMAT_R32_UINT;
				stModuleParams[i].stIndexsBufferView.SizeInBytes = stModuleParams[i].nIndexCnt * sizeof(UINT);

				::HeapFree(::GetProcessHeap(), 0, pstVertices);
				::HeapFree(::GetProcessHeap(), 0, pnIndices);

				// ������������ ע�⻺��ߴ�����Ϊ256�߽�����С
				GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pID3DDevice->CreateCommittedResource(
					&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD)
					, D3D12_HEAP_FLAG_NONE
					, &CD3DX12_RESOURCE_DESC::Buffer(szMVPBuf)
					, D3D12_RESOURCE_STATE_GENERIC_READ
					, nullptr
					, IID_PPV_ARGS(&stModuleParams[i].pIConstBuffer)));
				GRS_SetD3D12DebugNameIndexed(stModuleParams[i].pIConstBuffer.Get(), _T("pIConstBuffer"), i);
				// Map ֮��Ͳ���Unmap�� ֱ�Ӹ������ݽ�ȥ ����ÿ֡������map-copy-unmap�˷�ʱ����
				GRS_THROW_IF_FAILED(stModuleParams[i].pIConstBuffer->Map(0, nullptr, reinterpret_cast<void**>(&stModuleParams[i].pMVPBuf)));
				//---------------------------------------------------------------------------------------------

				// ����CBV
				D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
				cbvDesc.BufferLocation = stModuleParams[i].pIConstBuffer->GetGPUVirtualAddress();
				cbvDesc.SizeInBytes = static_cast<UINT>(szMVPBuf);
				stGPUParams[nIDGPUMain].m_pID3DDevice->CreateConstantBufferView(&cbvDesc, stModuleParams[i].pISRVCBVHp->GetCPUDescriptorHandleForHeapStart());
			}
		}

		//11����¼����������Ⱦ�������
		{
		{
			//	������Ⱦ���������һ����
			for (int i = 0; i < nMaxObject; i++)
			{
				stModuleParams[i].pIBundle->SetGraphicsRootSignature(pIRSMain.Get());
				stModuleParams[i].pIBundle->SetPipelineState(pIPSOMain.Get());

				ID3D12DescriptorHeap* ppHeapsSphere[] = { stModuleParams[i].pISRVCBVHp.Get(),stModuleParams[i].pISampleHp.Get() };
				stModuleParams[i].pIBundle->SetDescriptorHeaps(_countof(ppHeapsSphere), ppHeapsSphere);

				//����SRV
				CD3DX12_GPU_DESCRIPTOR_HANDLE stGPUCBVHandleSphere(stModuleParams[i].pISRVCBVHp->GetGPUDescriptorHandleForHeapStart());
				stModuleParams[i].pIBundle->SetGraphicsRootDescriptorTable(0, stGPUCBVHandleSphere);

				stGPUCBVHandleSphere.Offset(1, stGPUParams[nIDGPUMain].m_pID3DDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
				//����CBV
				stModuleParams[i].pIBundle->SetGraphicsRootDescriptorTable(1, stGPUCBVHandleSphere);

				//����Sample
				stModuleParams[i].pIBundle->SetGraphicsRootDescriptorTable(2, stModuleParams[i].pISampleHp->GetGPUDescriptorHandleForHeapStart());

				//ע������ʹ�õ���Ⱦ�ַ����������б���Ҳ����ͨ����Mesh����
				stModuleParams[i].pIBundle->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				stModuleParams[i].pIBundle->IASetVertexBuffers(0, 1, &stModuleParams[i].stVertexBufferView);
				stModuleParams[i].pIBundle->IASetIndexBuffer(&stModuleParams[i].stIndexsBufferView);

				//Draw Call������
				stModuleParams[i].pIBundle->DrawIndexedInstanced(stModuleParams[i].nIndexCnt, 1, 0, 0, 0);
				stModuleParams[i].pIBundle->Close();
			}
			//End for
		}
		}

		//12���������Կ�������Ⱦ������õľ��ο�
		{
			ST_GRS_VERTEX_QUAD stVertexQuad[] =
			{	//(   x,     y,    z,    w   )  (  u,    v   )
				{ { -1.0f,  1.0f, 0.0f, 1.0f }, { 0.0f, 0.0f } },	// Top left.
				{ {  1.0f,  1.0f, 0.0f, 1.0f }, { 1.0f, 0.0f } },	// Top right.
				{ { -1.0f, -1.0f, 0.0f, 1.0f }, { 0.0f, 1.0f } },	// Bottom left.
				{ {  1.0f, -1.0f, 0.0f, 1.0f }, { 1.0f, 1.0f } },	// Bottom right.
			};

			const UINT nszVBQuad = sizeof(stVertexQuad);

			GRS_THROW_IF_FAILED(stGPUParams[nIDGPUSecondary].m_pID3DDevice->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Buffer(nszVBQuad),
				D3D12_RESOURCE_STATE_COPY_DEST,
				nullptr,
				IID_PPV_ARGS(&pIVBQuad)));

			// �������������ʾ����ν����㻺���ϴ���Ĭ�϶��ϵķ�ʽ���������ϴ�Ĭ�϶�ʵ����һ����
			GRS_THROW_IF_FAILED(stGPUParams[nIDGPUSecondary].m_pID3DDevice->CreateCommittedResource(
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

			UpdateSubresources<1>(stGPUParams[nIDGPUSecondary].m_pICmdList.Get(), pIVBQuad.Get(), pIVBQuadUpload.Get(), 0, 0, 1, &stVBDataQuad);
			stGPUParams[nIDGPUSecondary].m_pICmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(pIVBQuad.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

			pstVBVQuad.BufferLocation = pIVBQuad->GetGPUVirtualAddress();
			pstVBVQuad.StrideInBytes = sizeof(ST_GRS_VERTEX_QUAD);
			pstVBVQuad.SizeInBytes = sizeof(stVertexQuad);
		}

		//13�������ڸ����Կ�����Ⱦ��λ�����õ�SRV�Ѻ�Sample��
		{
			D3D12_DESCRIPTOR_HEAP_DESC stHeapDesc = {};
			stHeapDesc.NumDescriptors = g_nFrameBackBufCount;
			stHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			stHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

			//SRV��
			GRS_THROW_IF_FAILED(stGPUParams[nIDGPUSecondary].m_pID3DDevice->CreateDescriptorHeap(&stHeapDesc, IID_PPV_ARGS(&pIDHSRVSecondary)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIDHSRVSecondary);

			// SRV
			D3D12_SHADER_RESOURCE_VIEW_DESC stSRVDesc = {};
			stSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			stSRVDesc.Format = emRTFmt;
			stSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			stSRVDesc.Texture2D.MipLevels = 1;

			CD3DX12_CPU_DESCRIPTOR_HANDLE stSrvHandle(pIDHSRVSecondary->GetCPUDescriptorHandleForHeapStart());

			for (int i = 0; i < g_nFrameBackBufCount; i++)
			{
				if (!bCrossAdapterTextureSupport)
				{
					// ʹ�ø��Ƶ���ȾĿ��������ΪShader��������Դ
					stGPUParams[nIDGPUSecondary].m_pID3DDevice->CreateShaderResourceView(
						pISecondaryAdapterTexutrePerFrame[i].Get()
						, &stSRVDesc
						, stSrvHandle);
				}
				else
				{
					// ֱ��ʹ�ù�������ȾĿ�긴�Ƶ�������Ϊ������Դ
					stGPUParams[nIDGPUSecondary].m_pID3DDevice->CreateShaderResourceView(
						stGPUParams[nIDGPUSecondary].m_pICrossAdapterResPerFrame[i].Get()
						, &stSRVDesc
						, stSrvHandle);
				}
				stSrvHandle.Offset(1, stGPUParams[nIDGPUSecondary].m_nszSRVCBVUAV);
			}


			// ����Sample Descriptor Heap ��������������
			stHeapDesc.NumDescriptors = 1;  //ֻ��һ��Sample
			stHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
			GRS_THROW_IF_FAILED(stGPUParams[nIDGPUSecondary].m_pID3DDevice->CreateDescriptorHeap(&stHeapDesc, IID_PPV_ARGS(&pIDHSampleSecondary)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIDHSampleSecondary);

			// ����Sample View ʵ�ʾ��ǲ�����
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

			stGPUParams[nIDGPUSecondary].m_pID3DDevice->CreateSampler(&stSamplerDesc, pIDHSampleSecondary->GetCPUDescriptorHandleForHeapStart());
		}

		//14������ͬ���õ�Χ��
		{
			for (int iGPUIndex = 0; iGPUIndex < nMaxGPUParams; iGPUIndex++)
			{
				GRS_THROW_IF_FAILED(stGPUParams[iGPUIndex].m_pID3DDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&stGPUParams[iGPUIndex].m_pIFence)));
			}
			
			// �����Կ��ϴ���һ���ɹ�����Χ������
			GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pID3DDevice->CreateFence(0
				, D3D12_FENCE_FLAG_SHARED | D3D12_FENCE_FLAG_SHARED_CROSS_ADAPTER
				, IID_PPV_ARGS(&stGPUParams[nIDGPUMain].m_pISharedFence)));

			// �������Χ����ͨ�������ʽ
			HANDLE hFenceShared = nullptr;
			GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pID3DDevice->CreateSharedHandle(
				stGPUParams[nIDGPUMain].m_pISharedFence.Get(),
				nullptr,
				GENERIC_ALL,
				nullptr,
				&hFenceShared));

			// �ڸ����Կ��ϴ����Χ��������ɹ���
			HRESULT hrOpenSharedHandleResult = stGPUParams[nIDGPUSecondary].m_pID3DDevice->OpenSharedHandle(hFenceShared
				, IID_PPV_ARGS(&stGPUParams[nIDGPUSecondary].m_pISharedFence));

			// �ȹرվ�������ж��Ƿ����ɹ�
			::CloseHandle(hFenceShared);
			GRS_THROW_IF_FAILED(hrOpenSharedHandleResult);

			hFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
			if (hFenceEvent == nullptr)
			{
				GRS_THROW_IF_FAILED(HRESULT_FROM_WIN32(GetLastError()));
			}
		}

		UINT64 n64fence = 0;
		UINT64 n64FenceValue = 1ui64;		

		//15��ִ�еڶ���Copy����ȴ����е��������ϴ�����Ĭ�϶���
		{
			GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pICmdList->Close());
			ID3D12CommandList* ppRenderCommandLists[] = { stGPUParams[nIDGPUMain].m_pICmdList.Get() };
			stGPUParams[nIDGPUMain].m_pICmdQueue->ExecuteCommandLists(_countof(ppRenderCommandLists), ppRenderCommandLists);

			n64fence = n64FenceValue;
			GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pICmdQueue->Signal(stGPUParams[nIDGPUMain].m_pIFence.Get(), n64fence));
			n64FenceValue++;


			// ��������û������ִ�е�Χ����ǵ����û�о������¼�ȥ�ȴ���ע��ʹ�õ���������ж����ָ��
			if (stGPUParams[nIDGPUMain].m_pIFence->GetCompletedValue() < n64fence)
			{
				GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pIFence->SetEventOnCompletion(n64fence, hFenceEvent));
				WaitForSingleObject(hFenceEvent, INFINITE);
			}

			//�����������Resetһ�£��ղ��Ѿ�ִ�й���һ����������������
			GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pICmdAllocPerFrame[nCurrentFrameIndex]->Reset());
			//Reset�����б���������ָ�������������PSO����
			GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pICmdList->Reset(
				stGPUParams[nIDGPUMain].m_pICmdAllocPerFrame[nCurrentFrameIndex].Get()
				, pIPSOMain.Get()));
		}

		DWORD dwRet = 0;
		BOOL bExit = FALSE;
		HANDLE phWait = CreateWaitableTimer(NULL, FALSE, NULL);
		LARGE_INTEGER liDueTime = {};
		liDueTime.QuadPart = -1i64;//1���ʼ��ʱ
		SetWaitableTimer(phWait, &liDueTime, 1, NULL, NULL, 0);//40ms������
		
		// Time Value ʱ�������
		ULONGLONG n64tmFrameStart = ::GetTickCount64();
		ULONGLONG n64tmCurrent = n64tmFrameStart;
		// ������ת�Ƕ���Ҫ�ı���
		double dModelRotationYAngle = 0.0f;

		XMMATRIX mxRotY;
		//16������ͶӰ����
		XMMATRIX mxProjection = XMMatrixPerspectiveFovLH(XM_PIDIV4, (FLOAT)iWndWidth / (FLOAT)iWndHeight, 0.1f, 1000.0f);

		//17����ʼ��Ϣѭ�����������в�����Ⱦ
		while (!bExit)
		{//��Ϣѭ�������ȴ���ʱֵ����Ϊ0��ͬʱ����ʱ�Ե���Ⱦ���ĳ���ÿ��ѭ������Ⱦ
			dwRet = ::MsgWaitForMultipleObjects(1, &phWait, FALSE, 0, QS_ALLINPUT);
			switch (dwRet - WAIT_OBJECT_0)
			{
			case 0:
			{
			}
			break;
			case WAIT_TIMEOUT:
			{//��ʱ��ʱ�䵽

			}
			break;
			case 1:
			{//������Ϣ
				while (::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
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
			break;
			default:
				break;
			}
			//���� Module ģ�;��� * �Ӿ��� view * �ü����� projection
			{//�൱��OnUpdate
				n64tmCurrent = ::GetTickCount();
				//������ת�ĽǶȣ���ת�Ƕ�(����) = ʱ��(��) * ���ٶ�(����/��)
				dModelRotationYAngle += ((n64tmCurrent - n64tmFrameStart) / 1000.0f) * g_fPalstance;

				n64tmFrameStart = n64tmCurrent;

				//��ת�Ƕ���2PI���ڵı�����ȥ����������ֻ�������0���ȿ�ʼ��С��2PI�Ļ��ȼ���
				if (dModelRotationYAngle > XM_2PI)
				{
					dModelRotationYAngle = fmod(dModelRotationYAngle, XM_2PI);
				}
				mxRotY = XMMatrixRotationY(static_cast<float>(dModelRotationYAngle));

				//ע��ʵ�ʵı任Ӧ���� Module * World * View * Projection
				//�˴�ʵ��World��һ����λ���󣬹�ʡ����
				//��Module��������λ������ת��Ҳ���Խ���ת��������Ϊ����World����
				XMMATRIX xmMVP = XMMatrixMultiply(XMMatrixLookAtLH(XMLoadFloat3(&g_f3EyePos)
					, XMLoadFloat3(&g_f3LockAt)
					, XMLoadFloat3(&g_f3HeapUp))
					, mxProjection);

				//���������MVP
				xmMVP = XMMatrixMultiply(mxRotY, xmMVP);

				for (int i = 0; i < nMaxObject; i++)
				{
					XMMATRIX xmModulePos = XMMatrixTranslation(stModuleParams[i].v4ModelPos.x
						, stModuleParams[i].v4ModelPos.y
						, stModuleParams[i].v4ModelPos.z);

					xmModulePos = XMMatrixMultiply(xmModulePos, xmMVP);

					XMStoreFloat4x4(&stModuleParams[i].pMVPBuf->m_MVP, xmModulePos);
				}
			}

			// ��ʼ��Ⱦ
			{// �൱��OnRender
				// ���Կ���Ⱦ
				{
					stGPUParams[nIDGPUMain].m_pICmdList->SetGraphicsRootSignature(pIRSMain.Get());
					stGPUParams[nIDGPUMain].m_pICmdList->SetPipelineState(pIPSOMain.Get());
					stGPUParams[nIDGPUMain].m_pICmdList->RSSetViewports(1, &stViewPort);
					stGPUParams[nIDGPUMain].m_pICmdList->RSSetScissorRects(1, &stScissorRect);

					stGPUParams[nIDGPUMain].m_pICmdList->ResourceBarrier(1
						, &CD3DX12_RESOURCE_BARRIER::Transition(
							stGPUParams[nIDGPUMain].m_pIRTRes[nCurrentFrameIndex].Get()
							, D3D12_RESOURCE_STATE_COMMON
							, D3D12_RESOURCE_STATE_RENDER_TARGET));

					CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(
						stGPUParams[nIDGPUMain].m_pIDHRTV->GetCPUDescriptorHandleForHeapStart()
						, nCurrentFrameIndex
						, stGPUParams[nIDGPUMain].m_nszRTV);
					CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(stGPUParams[nIDGPUMain].m_pIDHDSVTex->GetCPUDescriptorHandleForHeapStart());

					stGPUParams[nIDGPUMain].m_pICmdList->OMSetRenderTargets(1, &rtvHandle, false, &dsvHandle);
					stGPUParams[nIDGPUMain].m_pICmdList->ClearRenderTargetView(rtvHandle, arf4ClearColor, 0, nullptr);
					stGPUParams[nIDGPUMain].m_pICmdList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
					//==================================================================================================
					//ִ��ʵ�ʵ����������Ⱦ��Draw Call��
					for (int i = 0; i < nMaxObject; i++)
					{
						ID3D12DescriptorHeap* ppHeapsSkybox[] = { stModuleParams[i].pISRVCBVHp.Get(),stModuleParams[i].pISampleHp.Get() };
						stGPUParams[nIDGPUMain].m_pICmdList->SetDescriptorHeaps(_countof(ppHeapsSkybox), ppHeapsSkybox);
						stGPUParams[nIDGPUMain].m_pICmdList->ExecuteBundle(stModuleParams[i].pIBundle.Get());
					}
					//==================================================================================================

					stGPUParams[nIDGPUMain].m_pICmdList->ResourceBarrier(1
						, &CD3DX12_RESOURCE_BARRIER::Transition(
							stGPUParams[nIDGPUMain].m_pIRTRes[nCurrentFrameIndex].Get()
							, D3D12_RESOURCE_STATE_RENDER_TARGET
							, D3D12_RESOURCE_STATE_COMMON));

					GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pICmdList->Close());
				}

				// �����Կ�����Ⱦ������Ƶ�����������Դ��
				{
					if (bCrossAdapterTextureSupport)
					{
						// If cross-adapter row-major textures are supported by the adapter,
						// simply copy the texture into the cross-adapter texture.
						pICmdListCopy->CopyResource(stGPUParams[nIDGPUMain].m_pICrossAdapterResPerFrame[nCurrentFrameIndex].Get()
							, stGPUParams[nIDGPUMain].m_pIRTRes[nCurrentFrameIndex].Get());
					}
					else
					{
						// If cross-adapter row-major textures are not supported by the adapter,
						// the texture will be copied over as a buffer so that the texture row
						// pitch can be explicitly managed.

						// Copy the intermediate render target into the shared buffer using the
						// memory layout prescribed by the render target.
						D3D12_RESOURCE_DESC stRenderTargetDesc = stGPUParams[nIDGPUMain].m_pIRTRes[nCurrentFrameIndex]->GetDesc();
						D3D12_PLACED_SUBRESOURCE_FOOTPRINT stRenderTargetLayout = {};

						stGPUParams[nIDGPUMain].m_pID3DDevice->GetCopyableFootprints(&stRenderTargetDesc, 0, 1, 0, &stRenderTargetLayout, nullptr, nullptr, nullptr);

						CD3DX12_TEXTURE_COPY_LOCATION dest(stGPUParams[nIDGPUMain].m_pICrossAdapterResPerFrame[nCurrentFrameIndex].Get(), stRenderTargetLayout);
						CD3DX12_TEXTURE_COPY_LOCATION src(stGPUParams[nIDGPUMain].m_pIRTRes[nCurrentFrameIndex].Get(), 0);
						CD3DX12_BOX box(0, 0, iWndWidth, iWndHeight);

						pICmdListCopy->CopyTextureRegion(&dest, 0, 0, 0, &src, &box);
					}

					GRS_THROW_IF_FAILED(pICmdListCopy->Close());
				}

				// ��ʼ�����Կ�����Ⱦ��ͨ���Ǻ����������˶�ģ���ȣ���������ֱ�Ӿ��ǰѻ�����Ƴ���
				{
					if (!bCrossAdapterTextureSupport)
					{
						// Copy the buffer in the shared heap into a texture that the secondary
						// adapter can sample from.
						D3D12_RESOURCE_BARRIER stResBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
							pISecondaryAdapterTexutrePerFrame[nCurrentFrameIndex].Get(),
							D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
							D3D12_RESOURCE_STATE_COPY_DEST);

						stGPUParams[nIDGPUSecondary].m_pICmdList->ResourceBarrier(1, &stResBarrier);

						// Copy the shared buffer contents into the texture using the memory
						// layout prescribed by the texture.
						D3D12_RESOURCE_DESC stSecondaryAdapterTexture = pISecondaryAdapterTexutrePerFrame[nCurrentFrameIndex]->GetDesc();
						D3D12_PLACED_SUBRESOURCE_FOOTPRINT stTextureLayout = {};

						stGPUParams[nIDGPUSecondary].m_pID3DDevice->GetCopyableFootprints(&stSecondaryAdapterTexture, 0, 1, 0, &stTextureLayout, nullptr, nullptr, nullptr);

						CD3DX12_TEXTURE_COPY_LOCATION dest(pISecondaryAdapterTexutrePerFrame[nCurrentFrameIndex].Get(), 0);
						CD3DX12_TEXTURE_COPY_LOCATION src(stGPUParams[nIDGPUSecondary].m_pICrossAdapterResPerFrame[nCurrentFrameIndex].Get(), stTextureLayout);
						CD3DX12_BOX box(0, 0, iWndWidth, iWndHeight);

						stGPUParams[nIDGPUSecondary].m_pICmdList->CopyTextureRegion(&dest, 0, 0, 0, &src, &box);

						stResBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
						stResBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
						stGPUParams[nIDGPUSecondary].m_pICmdList->ResourceBarrier(1, &stResBarrier);
					}

					stGPUParams[nIDGPUSecondary].m_pICmdList->SetGraphicsRootSignature(pIRSQuad.Get());
					stGPUParams[nIDGPUSecondary].m_pICmdList->SetPipelineState(pIPSOQuad.Get());
					ID3D12DescriptorHeap* ppHeaps[] = { pIDHSRVSecondary.Get(),pIDHSampleSecondary.Get() };
					stGPUParams[nIDGPUSecondary].m_pICmdList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

					stGPUParams[nIDGPUSecondary].m_pICmdList->RSSetViewports(1, &stViewPort);
					stGPUParams[nIDGPUSecondary].m_pICmdList->RSSetScissorRects(1, &stScissorRect);

					D3D12_RESOURCE_BARRIER stRTSecondaryBarriers = CD3DX12_RESOURCE_BARRIER::Transition(
						stGPUParams[nIDGPUSecondary].m_pIRTRes[nCurrentFrameIndex].Get()
						, D3D12_RESOURCE_STATE_PRESENT
						, D3D12_RESOURCE_STATE_RENDER_TARGET);

					stGPUParams[nIDGPUSecondary].m_pICmdList->ResourceBarrier(1, &stRTSecondaryBarriers);

					CD3DX12_CPU_DESCRIPTOR_HANDLE rtvSecondaryHandle(
						stGPUParams[nIDGPUSecondary].m_pIDHRTV->GetCPUDescriptorHandleForHeapStart()
						, nCurrentFrameIndex
						, stGPUParams[nIDGPUSecondary].m_nszRTV);
					stGPUParams[nIDGPUSecondary].m_pICmdList->OMSetRenderTargets(1, &rtvSecondaryHandle, false, nullptr);
					float f4ClearColor[] = {1.0f,0.0f,0.0f,1.0f}; //����ʹ�������Կ���ȾĿ�겻ͬ�����ɫ���鿴�Ƿ��С�¶�ס�������
					stGPUParams[nIDGPUSecondary].m_pICmdList->ClearRenderTargetView(rtvSecondaryHandle, f4ClearColor, 0, nullptr);

					// ��ʼ���ƾ���
					CD3DX12_GPU_DESCRIPTOR_HANDLE srvHandle(
						pIDHSRVSecondary->GetGPUDescriptorHandleForHeapStart()
						, nCurrentFrameIndex
						, stGPUParams[nIDGPUSecondary].m_nszSRVCBVUAV);
					stGPUParams[nIDGPUSecondary].m_pICmdList->SetGraphicsRootDescriptorTable(0, srvHandle);
					stGPUParams[nIDGPUSecondary].m_pICmdList->SetGraphicsRootDescriptorTable(1, pIDHSampleSecondary->GetGPUDescriptorHandleForHeapStart());

					stGPUParams[nIDGPUSecondary].m_pICmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
					stGPUParams[nIDGPUSecondary].m_pICmdList->IASetVertexBuffers(0, 1, &pstVBVQuad);
					// Draw Call!
					stGPUParams[nIDGPUSecondary].m_pICmdList->DrawInstanced(4, 1, 0, 0);

					// ���ú�ͬ��Χ��
					stGPUParams[nIDGPUSecondary].m_pICmdList->ResourceBarrier(1
						, &CD3DX12_RESOURCE_BARRIER::Transition(
							stGPUParams[nIDGPUSecondary].m_pIRTRes[nCurrentFrameIndex].Get()
							, D3D12_RESOURCE_STATE_RENDER_TARGET
							, D3D12_RESOURCE_STATE_PRESENT));
					GRS_THROW_IF_FAILED(stGPUParams[nIDGPUSecondary].m_pICmdList->Close());
				}

				{// ��һ���������Կ��������������ִ���������б�
					ID3D12CommandList* ppRenderCommandLists[] = { stGPUParams[nIDGPUMain].m_pICmdList.Get() };
					stGPUParams[nIDGPUMain].m_pICmdQueue->ExecuteCommandLists(_countof(ppRenderCommandLists), ppRenderCommandLists);

					n64fence = n64FenceValue;
					GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pICmdQueue->Signal(stGPUParams[nIDGPUMain].m_pIFence.Get(), n64fence));
					n64FenceValue++;
				}

				{// �ڶ�����ʹ�����Կ��ϵĸ���������������ȾĿ����Դ��������Դ��ĸ���
					// ͨ������������е�Wait����ʵ��ͬһ��GPU�ϵĸ��������֮��ĵȴ�ͬ�� 
					GRS_THROW_IF_FAILED(pICmdQueueCopy->Wait(stGPUParams[nIDGPUMain].m_pIFence.Get(), n64fence));

					ID3D12CommandList* ppCopyCommandLists[] = { pICmdListCopy.Get() };
					pICmdQueueCopy->ExecuteCommandLists(_countof(ppCopyCommandLists), ppCopyCommandLists);

					n64fence = n64FenceValue;
					// ����������ź������ڹ�����Χ������������ʹ�õڶ����Կ���������п��������Χ���ϵȴ�
					// �Ӷ���ɲ�ͬGPU������м��ͬ���ȴ�
					GRS_THROW_IF_FAILED(pICmdQueueCopy->Signal(stGPUParams[nIDGPUMain].m_pISharedFence.Get(), n64fence));
					n64FenceValue++;
				}

				{// ��������ʹ�ø����Կ��ϵ����������ִ�������б�
					// �����Կ��ϵ����������ͨ���ȴ�������Χ��������������������Կ�֮���ͬ��
					GRS_THROW_IF_FAILED(stGPUParams[nIDGPUSecondary].m_pICmdQueue->Wait(
						stGPUParams[nIDGPUSecondary].m_pISharedFence.Get()
						, n64fence));
					
					ID3D12CommandList* ppBlurCommandLists[] = { stGPUParams[nIDGPUSecondary].m_pICmdList.Get() };
					stGPUParams[nIDGPUSecondary].m_pICmdQueue->ExecuteCommandLists(_countof(ppBlurCommandLists), ppBlurCommandLists);
				}

				// ִ��Present�������ճ��ֻ���
				GRS_THROW_IF_FAILED(pISwapChain3->Present(1, 0));

				n64fence = n64FenceValue;
				GRS_THROW_IF_FAILED(stGPUParams[nIDGPUSecondary].m_pICmdQueue->Signal(stGPUParams[nIDGPUSecondary].m_pIFence.Get(), n64fence));
				n64FenceValue++;

			
				// �������ֻ��Ҫ�ڸ����Կ���Χ��ͬ�������ϵȴ��������GPU��CPUִ�м��ͬ��
				UINT64 u64CompletedFenceValue = stGPUParams[nIDGPUSecondary].m_pIFence->GetCompletedValue();
				if ( u64CompletedFenceValue < n64fence)
				{
					GRS_THROW_IF_FAILED(stGPUParams[nIDGPUSecondary].m_pIFence->SetEventOnCompletion(
						n64fence
						, hFenceEvent));
					WaitForSingleObject(hFenceEvent, INFINITE);
				}
				// ��ʱ����GPU�ϵ�����Ѿ�ִ������ˣ�������һ֡��������ˣ�׼����ʼ��һ֡����Ⱦ
				
				// ��ȡ֡��
				nCurrentFrameIndex = pISwapChain3->GetCurrentBackBufferIndex();

				stGPUParams[nIDGPUMain].m_pICmdAllocPerFrame[nCurrentFrameIndex]->Reset();
				stGPUParams[nIDGPUMain].m_pICmdList->Reset(
					stGPUParams[nIDGPUMain].m_pICmdAllocPerFrame[nCurrentFrameIndex].Get()
					, pIPSOMain.Get());

				pICmdAllocCopyPerFrame[nCurrentFrameIndex]->Reset();
				pICmdListCopy->Reset(
					pICmdAllocCopyPerFrame[nCurrentFrameIndex].Get()
					, pIPSOMain.Get());
				
				stGPUParams[nIDGPUSecondary].m_pICmdAllocPerFrame[nCurrentFrameIndex]->Reset();
				stGPUParams[nIDGPUSecondary].m_pICmdList->Reset(
					stGPUParams[nIDGPUSecondary].m_pICmdAllocPerFrame[nCurrentFrameIndex].Get()
					, pIPSOQuad.Get());

			}

		}
	}
	catch (CGRSCOMException& e)
	{//������COM�쳣
		e;
	}
#if defined(_DEBUG)
	{
		ComPtr<IDXGIDebug1> dxgiDebug;
		if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug))))
		{
			dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_FLAGS(DXGI_DEBUG_RLO_SUMMARY | DXGI_DEBUG_RLO_IGNORE_INTERNAL));
		}
	}
#endif
	::CoUninitialize();
	return 0;
}


LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	case WM_KEYDOWN:
	{
		USHORT n16KeyCode = (wParam & 0xFF);
		if (VK_SPACE == n16KeyCode)
		{//

		}
		if (VK_ADD == n16KeyCode || VK_OEM_PLUS == n16KeyCode)
		{
			//double g_fPalstance = 10.0f * XM_PI / 180.0f;	//������ת�Ľ��ٶȣ���λ������/��
			g_fPalstance += 10 * XM_PI / 180.0f;
			if (g_fPalstance > XM_PI)
			{
				g_fPalstance = XM_PI;
			}
			//XMMatrixOrthographicOffCenterLH()
		}

		if (VK_SUBTRACT == n16KeyCode || VK_OEM_MINUS == n16KeyCode)
		{
			g_fPalstance -= 10 * XM_PI / 180.0f;
			if (g_fPalstance < 0.0f)
			{
				g_fPalstance = XM_PI / 180.0f;
			}
		}

		//�����û�����任
		//XMVECTOR g_f3EyePos = XMVectorSet(0.0f, 5.0f, -10.0f, 0.0f); //�۾�λ��
		//XMVECTOR g_f3LockAt = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);  //�۾�������λ��
		//XMVECTOR g_f3HeapUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);  //ͷ�����Ϸ�λ��
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

		if (VK_TAB == n16KeyCode)
		{//��Tab����ԭ�����λ��
			g_f3EyePos = XMFLOAT3(0.0f, 0.0f, -10.0f); //�۾�λ��
			g_f3LockAt = XMFLOAT3(0.0f, 0.0f, 0.0f);    //�۾�������λ��
			g_f3HeapUp = XMFLOAT3(0.0f, 1.0f, 0.0f);    //ͷ�����Ϸ�λ��
		}

	}

	break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

BOOL LoadMeshVertex(const CHAR*pszMeshFileName, UINT&nVertexCnt, ST_GRS_VERTEX_MODULE*&ppVertex, UINT*&ppIndices)
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

		ppVertex = (ST_GRS_VERTEX_MODULE*)HeapAlloc(::GetProcessHeap()
			, HEAP_ZERO_MEMORY
			, nVertexCnt * sizeof(ST_GRS_VERTEX_MODULE));
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
	catch (CGRSCOMException& e)
	{
		e;
		bRet = FALSE;
	}
	return bRet;
}