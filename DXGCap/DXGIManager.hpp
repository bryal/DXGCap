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

// The default format of B8G8R8A8 gives a pixel-size of 4 bytes
#define PIXEL_SIZE 4
#define PIXEL uint32_t

// printf(#expr " failed with error: %x\n", e); use for debugging
// Try performing the expression with return type HRESULT. If the result is a failure, return it.
#define TRY_RETURN(expr) { \
	HRESULT e = expr; \
	if (FAILED(e)) { \
		return e; \
	} \
}
// -''- If the result is a failure, throw it as exception.
#define TRY_EXCEPT(expr) { \
	HRESULT e = expr; \
	if (FAILED(e)) { \
		throw e; \
	} \
}

using std::vector;


vector<CComPtr<IDXGIOutput>> get_adapter_outputs(IDXGIAdapter1* adapter);

class DuplicatedOutput {
public:
	DuplicatedOutput(ID3D11Device* device,
		ID3D11DeviceContext* context,
		IDXGIOutput1* output,
		IDXGIOutputDuplication* output_dup);
	~DuplicatedOutput();
	DXGI_OUTPUT_DESC get_desc();
	HRESULT get_frame(IDXGISurface1** out_surface);
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
	HRESULT setup();
	void update();
	void set_capture_source(UINT16 cs);
	UINT16 get_capture_source();
	RECT get_output_rect();
	size_t get_output_data(BYTE** out_buf);
private:
	DuplicatedOutput* get_output_duplication();

	DuplicatedOutput* m_output_duplication;
	vector<DuplicatedOutput> m_out_dups;
	UINT16 m_capture_source;
	RECT m_output_rect;
	BYTE* m_frame_buf;
	size_t m_frame_buf_size;
};