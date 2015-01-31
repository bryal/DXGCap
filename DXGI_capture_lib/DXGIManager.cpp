#include "DXGIManager.hpp"

// TODO: add tests!!

DuplicatedOutput::DuplicatedOutput(ID3D11Device* device,
	ID3D11DeviceContext* context,
	IDXGIOutput1* output,
	IDXGIOutputDuplication* output_dup):
		m_device(device),
		m_device_context(context),
		m_output(output),
		m_dxgi_output_dup(output_dup) { }

DXGI_OUTPUT_DESC DuplicatedOutput::get_desc() {
	DXGI_OUTPUT_DESC desc;
	m_output->GetDesc(&desc);
	return desc;
}

HRESULT DuplicatedOutput::acquire_next_frame(IDXGISurface1** out_surface) {
	DXGI_OUTDUPL_FRAME_INFO frame_info;
	CComPtr<IDXGIResource> frame_resource;
	TRY_RETURN(m_dxgi_output_dup->AcquireNextFrame(20, &frame_info, &frame_resource));

	CComQIPtr<ID3D11Texture2D> frame_texture = frame_resource;
	D3D11_TEXTURE2D_DESC texture_desc;
	frame_texture->GetDesc(&texture_desc);

	// Comfigure the description to make the texture readable
	texture_desc.Usage = D3D11_USAGE_STAGING;
	texture_desc.BindFlags = 0;
	texture_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	texture_desc.MiscFlags = 0;

	CComPtr<ID3D11Texture2D> readable_texture;
	TRY_RETURN(m_device->CreateTexture2D(&texture_desc, NULL, &readable_texture));
	m_device_context->CopyResource(readable_texture, frame_texture);

	CComQIPtr<IDXGISurface1> texture_surface = readable_texture;

	*out_surface = texture_surface.Detach();

	return S_OK;
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

RECT DXGIManager::get_output_rect() {
	DuplicatedOutput output = get_output_duplication();
	DXGI_OUTPUT_DESC outDesc = output.get_desc();

	return outDesc.DesktopCoordinates;
}

vector<BYTE> DXGIManager::get_output_data() {
	RECT output_rect = get_output_rect();

	DWORD dwOutputWidth = output_rect.right - output_rect.left;
	DWORD dwOutputHeight = output_rect.bottom - output_rect.top;

	DWORD buf_size = dwOutputWidth*dwOutputHeight * DEF_PIXEL_SIZE;
	vector<BYTE> out_buf;
	out_buf.reserve(buf_size);

	BYTE* buf = out_buf.data();

	DuplicatedOutput output = get_output_duplication();
	DXGI_OUTPUT_DESC outDesc = output.get_desc();
	RECT rcOutCoords = outDesc.DesktopCoordinates;

	CComPtr<IDXGISurface1> surface;
	TRY_EXCEPT(output.acquire_next_frame(&surface));

	DXGI_MAPPED_RECT map;
	surface->Map(&map, DXGI_MAP_READ);

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
			memcpy_s(buf + (rcDesktop.left + (i + rcDesktop.top)*dwOutputWidth) * 4, dwStripe, map.pBits + i*map.Pitch, dwStripe);
		}
	}
	break;
	case DXGI_MODE_ROTATION_ROTATE90: {
		// Rotating at 90 degrees
		DWORD* pSrc = (DWORD*)map.pBits;
		DWORD* pDst = (DWORD*)buf;
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
		DWORD* pDst = (DWORD*)buf;
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
		DWORD* pDst = (DWORD*)buf;
		for (unsigned int j = 0; j<dwHeight; j++) {
			for (unsigned int i = 0; i<dwWidth; i++) {
				*(pDst + (rcDesktop.left + (j + rcDesktop.top)*dwOutputWidth) + i) = *(pSrc + (dwHeight - j - 1) + dwMapPitchPixels*i);
			}
		}
	}
	break;
	}

	surface->Unmap();

	output.release_frame();

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