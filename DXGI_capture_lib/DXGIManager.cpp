#include "DXGIManager.hpp"

// TODO: add tests!!


vector<CComPtr<IDXGIOutput>> get_adapter_outputs(IDXGIAdapter1* adapter) {
	vector<CComPtr<IDXGIOutput>> out_vec;
	for (UINT16 i = 0; ; i++) {
		CComPtr<IDXGIOutput> output;
		HRESULT hr = adapter->EnumOutputs(i, &output);
		if (FAILED(hr)) {
			break;
		}

		DXGI_OUTPUT_DESC out_desc;
		output->GetDesc(&out_desc);
		if (out_desc.AttachedToDesktop) {
			out_vec.push_back(output);
		}

		printf("Display output found. Device:%ls Rotation:%d Coordinates:(%d,%d) (%d,%d)\n",
			out_desc.DeviceName,
			out_desc.Rotation,
			out_desc.DesktopCoordinates.left, out_desc.DesktopCoordinates.top,
			out_desc.DesktopCoordinates.right, out_desc.DesktopCoordinates.bottom);

		output.Release();
	}

	return out_vec;
}

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
	TRY_RETURN(m_dxgi_output_dup->AcquireNextFrame(50, &frame_info, &frame_resource));

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
	DXGI_OUTPUT_DESC output_desc;
	m_output->GetDesc(&output_desc);
	MONITORINFO monitor_info;
	monitor_info.cbSize = sizeof(MONITORINFO);
	GetMonitorInfo(output_desc.Monitor, &monitor_info);

	return monitor_info.dwFlags & MONITORINFOF_PRIMARY;
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
	vector<CComPtr<IDXGIAdapter1>> adapters;

	CComPtr<IDXGIAdapter1> spAdapter;
	for (UINT16 i = 0; factory->EnumAdapters1(i, &spAdapter) != DXGI_ERROR_NOT_FOUND; i++) {
		adapters.push_back(spAdapter);
		spAdapter.Release();
	}

	// Iterating over all adapters to get all outputs
	for (auto& adapter : adapters) {
		vector<CComPtr<IDXGIOutput>> outputs = get_adapter_outputs(adapter);
		if (outputs.size() == 0) {
			continue;
		}

		// Creating device for each adapter that has the output
		CComPtr<ID3D11Device> d3d11_device;
		CComPtr<ID3D11DeviceContext> device_context;
		D3D_FEATURE_LEVEL feature_level = D3D_FEATURE_LEVEL_9_1;
		TRY_EXCEPT(D3D11CreateDevice(adapter,
			D3D_DRIVER_TYPE_UNKNOWN,
			NULL, 0, NULL, 0,
			D3D11_SDK_VERSION,
			&d3d11_device,
			&feature_level,
			&device_context));

		for (CComQIPtr<IDXGIOutput1> output : outputs) {
			CComQIPtr<IDXGIDevice1> dxgi_device = d3d11_device;

			CComPtr<IDXGIOutputDuplication> duplicated_output;
			HRESULT hr = output->DuplicateOutput(dxgi_device, &duplicated_output);
			if (FAILED(hr)) {
				continue;
			}

			m_out_dups.push_back(
				DuplicatedOutput(d3d11_device,
					device_context,
					output,
					duplicated_output));
		}
	}
}

RECT DXGIManager::get_output_rect() {
	DuplicatedOutput output = get_output_duplication();
	DXGI_OUTPUT_DESC output_desc = output.get_desc();
	return output_desc.DesktopCoordinates;
}

vector<BYTE> DXGIManager::get_output_data() {
	DuplicatedOutput output_dup = get_output_duplication();
	DXGI_OUTPUT_DESC output_desc = output_dup.get_desc();
	RECT output_rect = output_desc.DesktopCoordinates;
	UINT32 output_width = output_rect.right - output_rect.left;
	UINT32 output_height = output_rect.bottom - output_rect.top;

	UINT32 buf_size = output_width * output_height * PIXEL_SIZE;
	vector<BYTE> out_buf;
	out_buf.resize(buf_size); // Required since modifying the raw vector.data() does not change size

	CComPtr<IDXGISurface1> surface;
	TRY_EXCEPT(output_dup.acquire_next_frame(&surface));

	DXGI_MAPPED_RECT map;
	surface->Map(&map, DXGI_MAP_READ);

	OffsetRect(&output_rect, -output_rect.left, -output_rect.top);

	UINT32 dwMapPitchPixels = map.Pitch / PIXEL_SIZE;

	switch (output_desc.Rotation) {
	case DXGI_MODE_ROTATION_IDENTITY: {
		// Just copying
		UINT32 dwStripe = output_width * PIXEL_SIZE;
		for (UINT32 i = 0; i<output_height; i++) {
			memcpy_s(out_buf.data() + (output_rect.left + (i + output_rect.top)*output_width) * 4, dwStripe, map.pBits + i*map.Pitch, dwStripe);
		}
	}
	break;
	case DXGI_MODE_ROTATION_ROTATE90: {
		// Rotating at 90 degrees
		UINT32* pSrc = (UINT32*)map.pBits;
		UINT32* pDst = (UINT32*)out_buf.data();
		for (UINT32 j = 0; j<output_height; j++) {
			for (UINT32 i = 0; i<output_width; i++) {
				*(pDst + (output_rect.left + (j + output_rect.top)*output_width) + i) = *(pSrc + j + dwMapPitchPixels*(output_width - i - 1));
			}
		}
	}
	break;
	case DXGI_MODE_ROTATION_ROTATE180: {
		// Rotating at 180 degrees
		UINT32* pSrc = (UINT32*)map.pBits;
		UINT32* pDst = (UINT32*)out_buf.data();
		for (UINT32 j = 0; j<output_height; j++) {
			for (UINT32 i = 0; i<output_width; i++) {
				*(pDst + (output_rect.left + (j + output_rect.top)*output_width) + i) = *(pSrc + (output_width - i - 1) + dwMapPitchPixels*(output_height - j - 1));
			}
		}
	}
	break;
	case DXGI_MODE_ROTATION_ROTATE270: {
		// Rotating at 270 degrees
		UINT32* pSrc = (UINT32*)map.pBits;
		UINT32* pDst = (UINT32*)out_buf.data();
		for (UINT32 j = 0; j<output_height; j++) {
			for (UINT32 i = 0; i<output_width; i++) {
				*(pDst + (output_rect.left + (j + output_rect.top)*output_width) + i) = *(pSrc + (output_height - j - 1) + dwMapPitchPixels*i);
			}
		}
	}
	break;
	}

	surface->Unmap();

	output_dup.release_frame();

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
		if (out_it->is_primary()) {
			out_it++;
		}
		for (UINT32 i = 1; out_it != m_out_dups.end(); i++, out_it++) {
			if (i >= m_capture_source && !out_it->is_primary()) {
				break;
			}
		}
	}
	return *out_it;
}