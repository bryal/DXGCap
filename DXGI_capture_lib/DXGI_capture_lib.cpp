// DXGICaptureSample.cpp : Defines the entry point for the console application.
//

#include "DXGIManager.hpp"


int main(int argc, _TCHAR* argv[]) {
	printf("DXGICaptureSample. Fast windows screen capture\n"
		"Capturing desktop to: capture.bmp\n");

	CoInitialize(NULL);

	DXGIManager dxgi_manager;
	dxgi_manager.set_capture_source(1);

	RECT dimensions = dxgi_manager.get_output_rect();
	UINT32 width = dimensions.right - dimensions.left;
	UINT32 height = dimensions.bottom - dimensions.top;
	printf("width=%d height=%d\n", width, height);

	CComPtr<IWICImagingFactory> spWICFactory;
	TRY_RETURN(spWICFactory.CoCreateInstance(CLSID_WICImagingFactory));

	vector<BYTE> buf;
	HRESULT hr = S_OK;
	UINT8 i = 0;
	do {
		try {
			buf = dxgi_manager.get_output_data();
		} catch (HRESULT e) {
			hr = e;
		}
		i++;
	} while (hr == DXGI_ERROR_WAIT_TIMEOUT || i < 2);
	if (FAILED(hr)) {
		printf("get_output_data failed with hr=0x%08x\n", hr);
		return hr;
	}

	printf("Saving capture to file\n");

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

	return 0;
}