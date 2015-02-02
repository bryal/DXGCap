// DXGICaptureSample.cpp : Defines the entry point for the console application.
//

#include "DXGIManager.hpp"


// C ABI
extern "C" {
	void init() {
		CoInitialize(NULL);
	}

	void* create_dxgi_manager() {
		DXGIManager* dxgi_manager = new DXGIManager();
		dxgi_manager->setup();
		return (void*)dxgi_manager;
	}
	void delete_dxgi_manager(void* dxgi_manager) {
		DXGIManager* m = (DXGIManager*)(dxgi_manager);
		delete m;
	}

	void get_output_dimensions(void* dxgi_manager, uint32_t* width, uint32_t* height) {
		RECT dimensions = ((DXGIManager*)dxgi_manager)->get_output_rect();
		*width = dimensions.right - dimensions.left;
		*height = dimensions.bottom - dimensions.top;
	}

	bool get_frame_bytes(void* dxgi_manager, uint32_t* o_size, uint8_t** o_bytes) {
		HRESULT hr;
		do {
			hr = S_OK;
			try {

				*o_size = ((DXGIManager*)dxgi_manager)->get_output_data(o_bytes);
			} catch (HRESULT e) {
				hr = e;
			}
		} while (hr == DXGI_ERROR_WAIT_TIMEOUT);
		if (FAILED(hr)) {
			return false;
		}
		return true;
	}
}

int main(int argc, _TCHAR* argv[]) {
	init();

	auto dxgi_manager = create_dxgi_manager();
	// uint32_t width, height;
	// get_output_dimensions(dxgi_manager, &width, &height);

	uint32_t buf_size;
	uint8_t* buf;
	for (uint32_t i = 0; i < 600; i++) {
		get_frame_bytes(dxgi_manager, &buf_size, &buf);
		free(buf);
	}

	delete_dxgi_manager(dxgi_manager);
	
	/*
	printf("Saving capture to capture.bmp\n");

	CComPtr<IWICImagingFactory> spWICFactory;
	TRY_RETURN(spWICFactory.CoCreateInstance(CLSID_WICImagingFactory));

	CComPtr<IWICBitmap> spBitmap;
	TRY_RETURN(spWICFactory->CreateBitmapFromMemory(width, height, GUID_WICPixelFormat32bppBGRA, width * 4, buf_size, buf, &spBitmap));

	CComPtr<IWICStream> spStream;

	TRY_RETURN(spWICFactory->CreateStream(&spStream));
	TRY_RETURN(spStream->InitializeFromFilename(L"capture.bmp", GENERIC_WRITE));

	CComPtr<IWICBitmapEncoder> spEncoder;
	TRY_RETURN(spWICFactory->CreateEncoder(GUID_ContainerFormatBmp, NULL, &spEncoder));
	TRY_RETURN(spEncoder->Initialize(spStream, WICBitmapEncoderNoCache));

	CComPtr<IWICBitmapFrameEncode> spFrame;
	TRY_RETURN(spEncoder->CreateNewFrame(&spFrame, NULL));
	TRY_RETURN(spFrame->Initialize(NULL));
	TRY_RETURN(spFrame->SetSize(width, height));

	WICPixelFormatGUID format;
	TRY_RETURN(spBitmap->GetPixelFormat(&format));
	TRY_RETURN(spFrame->SetPixelFormat(&format));
	TRY_RETURN(spFrame->WriteSource(spBitmap, NULL));
	TRY_RETURN(spFrame->Commit());
	TRY_RETURN(spEncoder->Commit());
	*/

	return 0;
}