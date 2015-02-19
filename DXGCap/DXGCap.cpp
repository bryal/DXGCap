// The MIT License (MIT)
//
// Copyright (c) 2015 Johan Johansson
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

// DXGICaptureSample.cpp : Defines the entry point for the console application.
//

#include "DXGIManager.hpp"


// C ABI
extern "C" {
	__declspec(dllexport)
	void init() {
		CoInitialize(NULL);
	}

	__declspec(dllexport)
	void* create_dxgi_manager() {
		DXGIManager* dxgi_manager = new DXGIManager();
		dxgi_manager->setup();
		return (void*)dxgi_manager;
	}
	__declspec(dllexport)
	void delete_dxgi_manager(void* dxgi_manager) {
		DXGIManager* m = (DXGIManager*)(dxgi_manager);
		delete m;
	}

	__declspec(dllexport)
	void set_timeout(void* dxgi_manager, uint32_t timeout) {
		((DXGIManager*)dxgi_manager)->set_timeout(timeout);
	}

	__declspec(dllexport)
	void get_output_dimensions(void*const dxgi_manager, size_t* width, size_t* height) {
		RECT dimensions = ((DXGIManager*)dxgi_manager)->get_output_rect();
		*width = dimensions.right - dimensions.left;
		*height = dimensions.bottom - dimensions.top;
	}

	// Return the CaptureResult of acquiring frame and its data
	__declspec(dllexport)
	uint8_t get_frame_bytes(void* dxgi_manager, size_t* o_size, uint8_t** o_bytes) {
		return ((DXGIManager*)dxgi_manager)->get_output_data(o_bytes, o_size);
	}
}

// Debugging
int main(int argc, _TCHAR* argv[]) {
	init();

	auto dxgi_manager = create_dxgi_manager();
	if (dxgi_manager == NULL) {
		printf("dxgi_manager is null\n");
		return 1;
	}
	size_t width, height;
	get_output_dimensions(dxgi_manager, &width, &height);
	printf("%d x %d\n", width, height);

	size_t buf_size;
	uint8_t* buf = NULL;
	for (size_t i = 0; i < 60000; i++) {
		switch (get_frame_bytes(dxgi_manager, &buf_size, &buf)) {
			case CR_OK:
				break;
			case CR_ACCESS_DENIED:
				printf("Access denied\n");
				break;
			case CR_ACCESS_LOST:
				printf("Access lost\n");
				break;
			case CR_TIMEOUT:
				printf("Timeout\n");
				break;
			case CR_FAIL:
				printf("General failure\n");
				break;
		}
	}

	get_frame_bytes(dxgi_manager, &buf_size, &buf);
	
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

	delete_dxgi_manager(dxgi_manager);

	return 0;
}
