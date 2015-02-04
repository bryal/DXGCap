// DXGI_capture_lib.cpp : The entry point for the application.
//
// NOTES:
// The first time that `AcquireNextFrame` is called, the resulting frame is very likely to be all
// black, with no error. Following frames are not though. Why? I don't know. However, since this lib
// will not just take a single screenshot, but rather stream the screen, this doesn't really matter.
//
// There appears to be a huge memory leak (400mb fluxuation every second) somewhere in `get_frame`,
// `CopyResource` seems to be the culprit, but I can't find anything. Further, this behaviour only
// occour for one of my monitors, and not at all on my laptop. Might have something to do with AMD GPUs.

#include "DXGIManager.hpp"
#include <functional>
#include <chrono>
#include <thread>

// TODO: add tests!!

// TODO: Add member vars for frame buf and buf size, to avoid malloc for every frame


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
DuplicatedOutput::~DuplicatedOutput() {
	m_device.Release();
	m_device_context.Release();
	m_output.Release();
	m_dxgi_output_dup.Release();
}

DXGI_OUTPUT_DESC DuplicatedOutput::get_desc() {
	DXGI_OUTPUT_DESC desc;
	m_output->GetDesc(&desc);
	return desc;
}

// Returns status of AcquireNextFrame. May well be DXGI_ERROR_ACCESS_LOST due to mode change.
HRESULT DuplicatedOutput::get_frame(IDXGISurface1** out_surface) {
	IDXGIResource* frame_resource;
	DXGI_OUTDUPL_FRAME_INFO frame_info;
	TRY_RETURN(m_dxgi_output_dup->AcquireNextFrame(500, &frame_info, &frame_resource));

	ID3D11Texture2D* frame_texture;
	frame_resource->QueryInterface(__uuidof(ID3D11Texture2D),
		reinterpret_cast<void **>(&frame_texture));
	frame_resource->Release();

	D3D11_TEXTURE2D_DESC texture_desc;
	frame_texture->GetDesc(&texture_desc);

	// Configure the description to make the texture readable
	texture_desc.Usage = D3D11_USAGE_STAGING;
	texture_desc.BindFlags = 0;
	texture_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	texture_desc.MiscFlags = 0;

	ID3D11Texture2D* readable_texture;
	TRY_PANIC(m_device->CreateTexture2D(&texture_desc, NULL, &readable_texture));
	m_device_context->CopyResource(readable_texture, frame_texture);
	frame_texture->Release();

	IDXGISurface1* texture_surface;
	readable_texture->QueryInterface(__uuidof(IDXGISurface1),
		reinterpret_cast<void **>(&texture_surface));
	readable_texture->Release();

	*out_surface = texture_surface;
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

DXGIManager::DXGIManager(): m_capture_source(0), m_frame_buf(NULL), m_frame_buf_size(0) {
	SetRect(&m_output_rect, 0, 0, 0, 0);
}

DXGIManager::~DXGIManager() {
	free(m_frame_buf);
}

void DXGIManager::set_capture_source(UINT16 cs) {
	m_capture_source = cs;
	update_output();
}
UINT16 DXGIManager::get_capture_source() {
	return m_capture_source;
}

void DXGIManager::setup() {
	update_output();
}

void DXGIManager::gather_output_duplications() {
	clear_output_duplications();

	CComPtr<IDXGIFactory1> factory;
	TRY_PANIC(CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)(&factory)));

	// Getting all adapters
	vector<CComPtr<IDXGIAdapter1>> adapters;
	CComPtr<IDXGIAdapter1> n_adapter;
	for (UINT16 i = 0; factory->EnumAdapters1(i, &n_adapter) != DXGI_ERROR_NOT_FOUND; i++) {
		adapters.push_back(n_adapter);
		n_adapter.Release();
	}
	printf("n_adapters: %d\n", adapters.size());
	// Iterating over all adapters to get all outputs
	for (auto& adapter : adapters) {
		vector<CComPtr<IDXGIOutput>> outputs = get_adapter_outputs(adapter);
		if (outputs.size() == 0) {
			continue;
		}
		printf("n_outputs: %d\n", outputs.size());

		// Creating device for each adapter that has the output
		CComPtr<ID3D11Device> d3d11_device;
		CComPtr<ID3D11DeviceContext> device_context;
		D3D_FEATURE_LEVEL feature_level = D3D_FEATURE_LEVEL_9_1;
		TRY_PANIC(D3D11CreateDevice(adapter,
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
				printf("Duplication failed: 0x%x\n", hr);
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

void DXGIManager::update_output() {
	// Will retry getting specified output. Will exit after too many failed tries (0.8s/try).
	for (uint8_t i = 0; i < UPDATE_ALLOWED_TRIES; i++) {
		gather_output_duplications();
		printf("os: %d\n", m_out_dups.size());
		m_output_duplication = get_output_duplication();
		if (m_output_duplication == NULL) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		} else {
			return;
		}
	}
	printf("Failed to get output after %d tries. Giving up.\n"
		"Make sure youre monitors are plugged in and available\n", UPDATE_ALLOWED_TRIES);
	std::exit(-1);
}

bool DXGIManager::update_buffer_allocation() {
	RECT output_rect = get_output_rect();
	size_t output_width = output_rect.right - output_rect.left;
	size_t output_height = output_rect.bottom - output_rect.top;
	size_t buf_size = output_width * output_height * PIXEL_SIZE;
	if (m_frame_buf_size != buf_size) {
		m_frame_buf_size = buf_size;
		if (m_frame_buf != NULL) {
			free(m_frame_buf);
		}
		m_frame_buf = (BYTE*)malloc(m_frame_buf_size);
		return true;
	}
	return false;
}

RECT DXGIManager::get_output_rect() {
	DXGI_OUTPUT_DESC output_desc = m_output_duplication->get_desc();
	return output_desc.DesktopCoordinates;
}

// TODO: change to return HRESULT instead of throw esception, matches better with rest of the code
size_t DXGIManager::get_output_data(BYTE** out_buf) {
	DXGI_OUTPUT_DESC output_desc = m_output_duplication->get_desc();
	RECT output_rect = output_desc.DesktopCoordinates;
	size_t output_width = output_rect.right - output_rect.left;
	size_t output_height = output_rect.bottom - output_rect.top;

	update_buffer_allocation();

	IDXGISurface1* frame_surface;
	while (true) {
		HRESULT hr = m_output_duplication->get_frame(&frame_surface);
		if (hr == DXGI_ERROR_ACCESS_LOST) {
			// Access lost, get new DuplicatedOutput before trying again
			printf("Access lost\n");
			update_output();
		} else if (FAILED(hr)) {
			m_output_duplication->release_frame();
			throw hr;
		} else {
			break;
		}
	}
	
	DXGI_MAPPED_RECT mapped_surface;
	TRY_EXCEPT(frame_surface->Map(&mapped_surface, DXGI_MAP_READ));

	// Set origin of `output_rect` to (0, 0)
	OffsetRect(&output_rect, -output_rect.left, -output_rect.top);

	// The Pitch may contain padding, wherefore this is not necessarily pixels per row
	size_t map_pitch_n_pixels = mapped_surface.Pitch / PIXEL_SIZE;

	// Get address offset for source pixel from destination row and column
	// Order: 90deg, 180, 270
	std::function<size_t(size_t, size_t)> ofsetters [] = {
		// [&](size_t row, size_t col) { return row * map_pitch_n_pixels + col; },
		[&](size_t row, size_t col) { return (output_width-1-col) * map_pitch_n_pixels; },
		[&](size_t row, size_t col) {
			return (output_height-1-row) * map_pitch_n_pixels + (output_width-col-1); },
		[&](size_t row, size_t col) {
			return col * map_pitch_n_pixels + (output_height-row-1); }};

	if (output_desc.Rotation == DXGI_MODE_ROTATION_IDENTITY) {
		// Plain copy by byte
		size_t out_row_size = output_width * PIXEL_SIZE;
		for (size_t row_n = 0; row_n < output_height; row_n++) {
			memcpy(m_frame_buf + row_n * out_row_size,
				mapped_surface.pBits + row_n * mapped_surface.Pitch,
				out_row_size);
		}
	} else if (output_desc.Rotation != DXGI_MODE_ROTATION_UNSPECIFIED) {
		auto& ofsetter = ofsetters[output_desc.Rotation - 2]; // 90deg = 2 -> 0
		PIXEL* src_pixels = (PIXEL*)mapped_surface.pBits;
		PIXEL* dest_pixels = (PIXEL*)m_frame_buf;
		for (size_t dst_row = 0; dst_row < output_height; dst_row++) {
			for (size_t dst_col = 0; dst_col < output_width; dst_col ++) {
				*(dest_pixels + dst_row * output_width + dst_col) =
					*(src_pixels + ofsetter(dst_row, dst_col));
			}
		}
	} else {
		throw -1;
	}

	frame_surface->Unmap();
	frame_surface->Release();
	m_output_duplication->release_frame();
	
	*out_buf = m_frame_buf;
	return m_frame_buf_size;
}

void DXGIManager::clear_output_duplications() {
	m_output_duplication = NULL;
	m_out_dups.clear();
}

// If there are no output duplications, or specified monitor is not found, return NULL
DuplicatedOutput* DXGIManager::get_output_duplication() {
	size_t n_out_dups = m_out_dups.size();
	if (n_out_dups == 0) {
		return NULL;
	}
	size_t i = 0;
	if (m_capture_source == 0) { // Find the primary output
		for (; i < n_out_dups; i++) {
			if (m_out_dups[i].is_primary()) {
				break;
			}
		}
	} else { // Find the m_capture_source:th, non-primary output
		if (m_out_dups[i].is_primary()) {
			i++;
		}
		for (size_t j = 1; i < n_out_dups; j++, i++) {
			if (j >= m_capture_source && !m_out_dups[i].is_primary()) {
				break;
			}
		}
	}
	if (i >= n_out_dups) {
		return NULL;
	}
	return &m_out_dups[i];
}