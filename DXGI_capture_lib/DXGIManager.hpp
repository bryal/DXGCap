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


enum CaptureSource {
	CSUndefined,
	CSMonitor1,
	CSMonitor2,
	CSDesktop
};

class DuplicatedOutput {
public:
	DuplicatedOutput(ID3D11Device* device,
		ID3D11DeviceContext* context,
		IDXGIOutput1* output,
		IDXGIOutputDuplication* output_dup);

	void get_desc(DXGI_OUTPUT_DESC& desc);
	HRESULT acquire_next_frame(IDXGISurface1** pD3D11Texture2D);
	void release_frame();

	bool is_primary();

private:
	CComPtr<ID3D11Device> m_device;
	CComPtr<ID3D11DeviceContext> m_device_context;
	CComPtr<IDXGIOutput1> m_output;
	CComPtr<IDXGIOutputDuplication> m_dxgi_output_dup;
};

class DXGIManager {
public:
	DXGIManager();
	~DXGIManager();
	void set_capture_source(CaptureSource type);
	CaptureSource get_capture_source();

	HRESULT get_output_rect(RECT& rc);
	HRESULT get_output_bits(BYTE* pBits, RECT& rcDest);
private:
	HRESULT init();
	vector<DuplicatedOutput> get_output_duplication();
private:
	CComPtr<IDXGIFactory1> m_factory;
	vector<DuplicatedOutput> m_outputs;
	bool m_initialized;
	CaptureSource m_capture_rect;
	RECT m_current_output;
	BYTE* m_buf;

	CComPtr<IWICImagingFactory> m_spWICFactory;
};