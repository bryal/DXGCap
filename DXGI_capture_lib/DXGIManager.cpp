#include "DXGIManager.hpp"


DuplicatedOutput::DuplicatedOutput(ID3D11Device* device,
	ID3D11DeviceContext* context,
	IDXGIOutput1* output,
	IDXGIOutputDuplication* output_dup):
		m_device(device),
		m_device_context(context),
		m_output(output),
		m_dxgi_output_dup(output_dup) { }

void DuplicatedOutput::get_desc(DXGI_OUTPUT_DESC& desc) {
	m_output->GetDesc(&desc);
}

HRESULT DuplicatedOutput::acquire_next_frame(IDXGISurface1** pDXGISurface) {
	DXGI_OUTDUPL_FRAME_INFO fi;
	CComPtr<IDXGIResource> spDXGIResource;
	HRESULT hr = m_dxgi_output_dup->AcquireNextFrame(20, &fi, &spDXGIResource);
	if (FAILED(hr)) {
		printf("m_DXGIOutputDuplication->AcquireNextFrame failed with hr=0x%08x\n", hr);
		return hr;
	}

	CComQIPtr<ID3D11Texture2D> spTextureResource = spDXGIResource;

	D3D11_TEXTURE2D_DESC desc;
	spTextureResource->GetDesc(&desc);

	D3D11_TEXTURE2D_DESC texDesc;
	ZeroMemory(&texDesc, sizeof(texDesc));
	texDesc.Width = desc.Width;
	texDesc.Height = desc.Height;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Usage = D3D11_USAGE_STAGING;
	texDesc.Format = desc.Format;
	texDesc.BindFlags = 0;
	texDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	texDesc.MiscFlags = 0;

	CComPtr<ID3D11Texture2D> spD3D11Texture2D = NULL;
	hr = m_device->CreateTexture2D(&texDesc, NULL, &spD3D11Texture2D);
	if (FAILED(hr)) {
		return hr;
	}

	m_device_context->CopyResource(spD3D11Texture2D, spTextureResource);

	CComQIPtr<IDXGISurface1> spDXGISurface = spD3D11Texture2D;

	*pDXGISurface = spDXGISurface.Detach();

	return hr;
}

void DuplicatedOutput::release_frame() {
	m_dxgi_output_dup->ReleaseFrame();
}

bool DuplicatedOutput::is_primary() {
	DXGI_OUTPUT_DESC outdesc;
	m_output->GetDesc(&outdesc);

	MONITORINFO mi;
	mi.cbSize = sizeof(MONITORINFO);
	GetMonitorInfo(outdesc.Monitor, &mi);
	if (mi.dwFlags & MONITORINFOF_PRIMARY) {
		return true;
	}
	return false;
}

DXGIManager::DXGIManager(): m_capture_source(0) {
	SetRect(&m_output_rect, 0, 0, 0, 0);
	init();
}

DXGIManager::~DXGIManager() {
	if (m_buf) {
		delete[] m_buf;
		m_buf = NULL;
	}
}

void DXGIManager::set_capture_source(UINT16 cs) {
	m_capture_source = cs;
}
UINT16 DXGIManager::get_capture_source() {
	return m_capture_source;
}

void DXGIManager::init() {
	CComPtr<IDXGIFactory1> factory;
	TRY_EXCEPT(CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)(&factory)));

	// Getting all adapters
	vector<CComPtr<IDXGIAdapter1>> vAdapters;

	CComPtr<IDXGIAdapter1> spAdapter;
	for (int i = 0; factory->EnumAdapters1(i, &spAdapter) != DXGI_ERROR_NOT_FOUND; i++) {
		vAdapters.push_back(spAdapter);
		spAdapter.Release();
	}

	// Iterating over all adapters to get all outputs
	for (auto AdapterIter = vAdapters.begin(); AdapterIter != vAdapters.end(); AdapterIter++) {
		vector<CComPtr<IDXGIOutput>> vOutputs;

		CComPtr<IDXGIOutput> spDXGIOutput;
		for (int i = 0; (*AdapterIter)->EnumOutputs(i, &spDXGIOutput) != DXGI_ERROR_NOT_FOUND; i++) {
			DXGI_OUTPUT_DESC outputDesc;
			spDXGIOutput->GetDesc(&outputDesc);

			printf("Display output found. DeviceName=%ls  AttachedToDesktop=%d Rotation=%d DesktopCoordinates={(%d,%d),(%d,%d)}\n",
				outputDesc.DeviceName,
				outputDesc.AttachedToDesktop,
				outputDesc.Rotation,
				outputDesc.DesktopCoordinates.left,
				outputDesc.DesktopCoordinates.top,
				outputDesc.DesktopCoordinates.right,
				outputDesc.DesktopCoordinates.bottom);

			if (outputDesc.AttachedToDesktop) {
				vOutputs.push_back(spDXGIOutput);
			}

			spDXGIOutput.Release();
		}

		if (vOutputs.size() == 0) {
			continue;
		}

		// Creating device for each adapter that has the output
		CComPtr<ID3D11Device> spD3D11Device;
		CComPtr<ID3D11DeviceContext> spD3D11DeviceContext;
		D3D_FEATURE_LEVEL fl = D3D_FEATURE_LEVEL_9_1;
		TRY_EXCEPT(D3D11CreateDevice((*AdapterIter),
			D3D_DRIVER_TYPE_UNKNOWN,
			NULL, 0, NULL, 0,
			D3D11_SDK_VERSION,
			&spD3D11Device,
			&fl,
			&spD3D11DeviceContext));

		for (auto out_it = vOutputs.begin(); out_it != vOutputs.end(); out_it++) {
			CComQIPtr<IDXGIOutput1> spDXGIOutput1 = *out_it;
			CComQIPtr<IDXGIDevice1> spDXGIDevice = spD3D11Device;
			if (!spDXGIOutput1 || !spDXGIDevice) {
				continue;
			}

			CComPtr<IDXGIOutputDuplication> spDuplicatedOutput;
			HRESULT hr = spDXGIOutput1->DuplicateOutput(spDXGIDevice, &spDuplicatedOutput);
			if (FAILED(hr)) {
				continue;
			}

			m_out_dups.push_back(
				DuplicatedOutput(spD3D11Device,
					spD3D11DeviceContext,
					spDXGIOutput1,
					spDuplicatedOutput));
		}
	}

	return;
}

void DXGIManager::get_output_rect(RECT& rc) {
	DuplicatedOutput output = get_output_duplication();

	DXGI_OUTPUT_DESC outDesc;
	output.get_desc(outDesc);

	CopyRect(&rc, &outDesc.DesktopCoordinates);
}

vector<BYTE> DXGIManager::get_output_data() {
	RECT output_rect;
	HRESULT hr = get_output_rect(output_rect);
	if (FAILED(hr)) {
		throw hr;
	}

	DWORD dwOutputWidth = output_rect.right - output_rect.left;
	DWORD dwOutputHeight = output_rect.bottom - output_rect.top;

	DWORD buf_size = dwOutputWidth*dwOutputHeight * DEF_PIXEL_SIZE;
	vector<BYTE> out_buf;
	out_buf.reserve(buf_size);

	BYTE* pBuf = out_buf.data();

	DuplicatedOutput output = get_output_duplication();

	DXGI_OUTPUT_DESC outDesc;
	output.get_desc(outDesc);
	RECT rcOutCoords = outDesc.DesktopCoordinates;

	CComPtr<IDXGISurface1> spDXGISurface1;
	hr = output.acquire_next_frame(&spDXGISurface1);
	if (FAILED(hr)) {
		throw hr;
	}

	DXGI_MAPPED_RECT map;
	spDXGISurface1->Map(&map, DXGI_MAP_READ);

	RECT rcDesktop = outDesc.DesktopCoordinates;
	DWORD dwWidth = rcDesktop.right - rcDesktop.left;
	DWORD dwHeight = rcDesktop.bottom - rcDesktop.top;

	OffsetRect(&rcDesktop, -output_rect.left, -output_rect.top);

	DWORD dwMapPitchPixels = map.Pitch / 4;

	switch (outDesc.Rotation) {
	case DXGI_MODE_ROTATION_IDENTITY: {
		// Just copying
		DWORD dwStripe = dwWidth * 4;
		for (unsigned int i = 0; i<dwHeight; i++) {
			memcpy_s(pBuf + (rcDesktop.left + (i + rcDesktop.top)*dwOutputWidth) * 4, dwStripe, map.pBits + i*map.Pitch, dwStripe);
		}
	}
	break;
	case DXGI_MODE_ROTATION_ROTATE90: {
		// Rotating at 90 degrees
		DWORD* pSrc = (DWORD*)map.pBits;
		DWORD* pDst = (DWORD*)pBuf;
		for (unsigned int j = 0; j<dwHeight; j++) {
			for (unsigned int i = 0; i<dwWidth; i++) {
				*(pDst + (rcDesktop.left + (j + rcDesktop.top)*dwOutputWidth) + i) = *(pSrc + j + dwMapPitchPixels*(dwWidth - i - 1));
			}
		}
	}
	break;
	case DXGI_MODE_ROTATION_ROTATE180: {
		// Rotating at 180 degrees
		DWORD* pSrc = (DWORD*)map.pBits;
		DWORD* pDst = (DWORD*)pBuf;
		for (unsigned int j = 0; j<dwHeight; j++) {
			for (unsigned int i = 0; i<dwWidth; i++) {
				*(pDst + (rcDesktop.left + (j + rcDesktop.top)*dwOutputWidth) + i) = *(pSrc + (dwWidth - i - 1) + dwMapPitchPixels*(dwHeight - j - 1));
			}
		}
	}
	break;
	case DXGI_MODE_ROTATION_ROTATE270: {
		// Rotating at 270 degrees
		DWORD* pSrc = (DWORD*)map.pBits;
		DWORD* pDst = (DWORD*)pBuf;
		for (unsigned int j = 0; j<dwHeight; j++) {
			for (unsigned int i = 0; i<dwWidth; i++) {
				*(pDst + (rcDesktop.left + (j + rcDesktop.top)*dwOutputWidth) + i) = *(pSrc + (dwHeight - j - 1) + dwMapPitchPixels*i);
			}
		}
	}
	break;
	}

	spDXGISurface1->Unmap();

	output.release_frame();


	if (FAILED(hr)) {
		throw hr;
	}

	// We have the pBuf filled with current desktop/monitor image.
	return out_buf;
}

DuplicatedOutput DXGIManager::get_output_duplication() {
	auto out_it = m_out_dups.begin();
	if (m_capture_source == 0) { // Find the primary output
		for (; out_it != m_out_dups.end(); out_it++) {
			if (out_it->is_primary()) {
				break;
			}
		}
	} else { // Find the m_capture_source:th, non-primary output
		for (UINT32 i = 0; i < m_capture_source && out_it != m_out_dups.end(); i++) {
			if (!out_it->is_primary()) {
				i++;
			}
			out_it++;
		}
	}
	return *out_it;
}