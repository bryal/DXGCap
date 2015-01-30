#pragma once

#include <Windows.h>
#include <atlbase.h>
#include <DXGI1_2.h>
#include <d3d11.h>
#include <Wincodec.h>
#include <vector>
#include <memory>

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d11.lib")

using std::vector;


enum CaptureSource
{
	CSUndefined,
	CSMonitor1,
	CSMonitor2,
	CSDesktop
};

class DuplicatedOutput
{
public:
	DuplicatedOutput(ID3D11Device* device,
		ID3D11DeviceContext* context,
		IDXGIOutput1* output,
		IDXGIOutputDuplication* output_dup);

	HRESULT get_desc(DXGI_OUTPUT_DESC& desc);
	HRESULT acquire_next_frame(IDXGISurface1** pD3D11Texture2D);
	HRESULT release_frame();

	bool is_primary();

private:
	CComPtr<ID3D11Device> m_device;
	CComPtr<ID3D11DeviceContext> m_device_context;
	CComPtr<IDXGIOutput1> m_output;
	CComPtr<IDXGIOutputDuplication> m_dxgi_output_dup;
};

class DXGIManager
{
public:
	DXGIManager();
	~DXGIManager();
	HRESULT SetCaptureSource(CaptureSource type);
	CaptureSource GetCaptureSource();

	HRESULT GetOutputRect(RECT& rc);
	HRESULT GetOutputBits(BYTE* pBits, RECT& rcDest);
private:
	HRESULT Init();
	int GetMonitorCount();
	vector<DuplicatedOutput> GetOutputDuplication();
private:
	CComPtr<IDXGIFactory1> m_spDXGIFactory1;
	vector<DuplicatedOutput> m_vOutputs;
	bool m_bInitialized;
	CaptureSource m_CaptureSource;
	RECT m_rcCurrentOutput;
	BYTE* m_pBuf;

	CComPtr<IWICImagingFactory> m_spWICFactory;
};