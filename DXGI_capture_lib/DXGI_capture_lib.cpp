// DXGICaptureSample.cpp : Defines the entry point for the console application.
//

#include "DXGIManager.hpp"



int _tmain(int argc, _TCHAR* argv[]) {
	printf("DXGICaptureSample. Fast windows screen capture\n");
	printf("Capturing desktop to: capture.bmp\n");
	printf("Log: logfile.log\n");

	CoInitialize(NULL);

	DXGIManager dxgi_manager;

	dxgi_manager.set_capture_source(0);

	RECT rcDim;
	dxgi_manager.get_output_rect(rcDim);
	DWORD dwWidth = rcDim.right - rcDim.left;
	DWORD dwHeight = rcDim.bottom - rcDim.top;
	printf("dwWidth=%d dwHeight=%d\n", dwWidth, dwHeight);

	DWORD dwBufSize = dwWidth*dwHeight * DEF_PIXEL_SIZE;

	CComPtr<IWICImagingFactory> spWICFactory = NULL;
	HRESULT hr = spWICFactory.CoCreateInstance(CLSID_WICImagingFactory);
	if (FAILED(hr)) {
		return hr;
	}

	vector<BYTE> buf;
	int i = 0;
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

	BYTE* pBuf = buf.data();

	printf("Saving capture to file\n");

	CComPtr<IWICBitmap> spBitmap = NULL;
	hr = spWICFactory->CreateBitmapFromMemory(dwWidth, dwHeight, GUID_WICPixelFormat32bppBGRA, dwWidth * 4, dwBufSize, (BYTE*)pBuf, &spBitmap);
	if (FAILED(hr)) {
		return hr;
	}

	CComPtr<IWICStream> spStream = NULL;

	hr = spWICFactory->CreateStream(&spStream);
	if (SUCCEEDED(hr)) {
		hr = spStream->InitializeFromFilename(L"capture.bmp", GENERIC_WRITE);
	}

	CComPtr<IWICBitmapEncoder> spEncoder = NULL;
	if (SUCCEEDED(hr)) {
		hr = spWICFactory->CreateEncoder(GUID_ContainerFormatBmp, NULL, &spEncoder);
	}

	if (SUCCEEDED(hr)) {
		hr = spEncoder->Initialize(spStream, WICBitmapEncoderNoCache);
	}

	CComPtr<IWICBitmapFrameEncode> spFrame = NULL;
	if (SUCCEEDED(hr)) {
		hr = spEncoder->CreateNewFrame(&spFrame, NULL);
	}

	if (SUCCEEDED(hr)) {
		hr = spFrame->Initialize(NULL);
	}

	if (SUCCEEDED(hr)) {
		hr = spFrame->SetSize(dwWidth, dwHeight);
	}

	WICPixelFormatGUID format;
	spBitmap->GetPixelFormat(&format);

	if (SUCCEEDED(hr)) {
		hr = spFrame->SetPixelFormat(&format);
	}

	if (SUCCEEDED(hr)) {
		hr = spFrame->WriteSource(spBitmap, NULL);
	}

	if (SUCCEEDED(hr)) {
		hr = spFrame->Commit();
	}

	if (SUCCEEDED(hr)) {
		hr = spEncoder->Commit();
	}

	return 0;
}

