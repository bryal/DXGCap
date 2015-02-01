// DXGICaptureSample.cpp : Defines the entry point for the console application.
//

#include "DXGIManager.hpp"


int main(int argc, _TCHAR* argv[]) {
	CoInitialize(NULL);

	DXGIManager dxgi_manager;
	dxgi_manager.init();

	RECT dimensions = dxgi_manager.get_output_rect();
	UINT32 width = dimensions.right - dimensions.left;
	UINT32 height = dimensions.bottom - dimensions.top;
	printf("width=%d height=%d\n", width, height);

	vector<BYTE> buf;
	// Benchmark
	for (UINT32 j = 0; j < 1200; j++) {
		
		HRESULT hr = S_OK;
		do {
			hr = S_OK;
			try {
				buf = dxgi_manager.get_output_data();
			} catch (HRESULT e) {
				hr = e;
			}
		} while (hr == DXGI_ERROR_WAIT_TIMEOUT);
		if (FAILED(hr)) {
			printf("get_output_data failed with hr=0x%08x\n", hr);
			return hr;
		}
	}
	
	/*
	printf("Saving capture to capture.bmp\n");

	CComPtr<IWICImagingFactory> spWICFactory;
	TRY_RETURN(spWICFactory.CoCreateInstance(CLSID_WICImagingFactory));

	CComPtr<IWICBitmap> spBitmap;
	TRY_RETURN(spWICFactory->CreateBitmapFromMemory(width, height, GUID_WICPixelFormat32bppBGRA, width * 4, buf.size(), (BYTE*)buf.data(), &spBitmap));

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
	TRY_RETURN(hr = spFrame->SetPixelFormat(&format));
	TRY_RETURN(hr = spFrame->WriteSource(spBitmap, NULL));
	TRY_RETURN(hr = spFrame->Commit());
	TRY_RETURN(hr = spEncoder->Commit());
	*/

	CoUninitialize();

	return 0;
}