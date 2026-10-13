#include "pch.h"
#include "wicutil.h"

bool WICIsOrientationSupported(IWICBitmapDecoder* pDecoder)
{
	const GUID GUID_ContainerFormatJpegXL = { 0xfec14e3f, 0x427a, 0x4736, { 0xaa, 0xe6, 0x27, 0xed, 0x84, 0xf6, 0x93, 0x22 } };

	GUID guidContainerFormat = {};
	if (FAILED(pDecoder->GetContainerFormat(&guidContainerFormat)))
		return false;

	return guidContainerFormat == GUID_ContainerFormatJpeg
		|| guidContainerFormat == GUID_ContainerFormatAdng
		|| guidContainerFormat == GUID_ContainerFormatHeif
		|| guidContainerFormat == GUID_ContainerFormatRaw
		|| guidContainerFormat == GUID_ContainerFormatJpegXL;
}

HRESULT WICGetTransformOptionFromMetadata(IWICMetadataQueryReader* pQueryReader, WICBitmapTransformOptions* pOptions)
{
	*pOptions = WICBitmapTransformRotate0;

	PROPVARIANT varOrientation = {};
	HRESULT hr = pQueryReader->GetMetadataByName(L"System.Photo.Orientation", &varOrientation);
	if (hr == WINCODEC_ERR_PROPERTYNOTFOUND || varOrientation.vt != VT_UI2)
	{
		*pOptions = WICBitmapTransformRotate0;
		hr = S_OK;
	}
	else if (SUCCEEDED(hr))
	{
		switch (varOrientation.uiVal)
		{
		case 1:
			*pOptions = WICBitmapTransformRotate0;
			break;
		case 2:
			*pOptions = WICBitmapTransformFlipHorizontal;
			break;
		case 3:
			*pOptions = WICBitmapTransformRotate180;
			break;
		case 4:
			*pOptions = WICBitmapTransformFlipVertical;
			break;
		case 5:
			*pOptions = (WICBitmapTransformOptions)(WICBitmapTransformFlipVertical | WICBitmapTransformRotate90);
			break;
		case 6:
			*pOptions = WICBitmapTransformRotate90;
			break;
		case 7:
			*pOptions = (WICBitmapTransformOptions)(WICBitmapTransformFlipVertical | WICBitmapTransformRotate270);
			break;
		case 8:
			*pOptions = WICBitmapTransformRotate270;
			break;
		default:
			*pOptions = WICBitmapTransformRotate0;
			break;
		}
	}
	PropVariantClear(&varOrientation);
	return hr;
}

HRESULT WICCreateCachedOrientedBitmapSource(IWICImagingFactory* pFactory, IWICBitmapSource* pSourceBitmap,
	IWICMetadataQueryReader* pQueryReader, IWICBitmapSource** ppOutputBitmap)
{
	*ppOutputBitmap = nullptr;

	WICBitmapTransformOptions transformOptions = WICBitmapTransformRotate0;
	HRESULT hr = WICGetTransformOptionFromMetadata(pQueryReader, &transformOptions);
	if (SUCCEEDED(hr))
	{
		if (transformOptions == WICBitmapTransformRotate0)
		{
			*ppOutputBitmap = pSourceBitmap;
			pSourceBitmap->AddRef();
		}
		else
		{
			IWICBitmapFlipRotator* pFlipRotator = nullptr;
			hr = pFactory->CreateBitmapFlipRotator(&pFlipRotator);
			if (SUCCEEDED(hr))
			{
				if ((transformOptions & (WICBitmapTransformRotate90 | WICBitmapTransformFlipVertical)) != 0 || transformOptions == WICBitmapTransformRotate180)
				{
					IWICBitmap* pCachedBitmap = nullptr;
					hr = pFactory->CreateBitmapFromSource(pSourceBitmap, WICBitmapCacheOnLoad, &pCachedBitmap);
					if (SUCCEEDED(hr))
					{
						hr = pFlipRotator->Initialize(pCachedBitmap, transformOptions);
						pCachedBitmap->Release();
					}
				}
				else
				{
					hr = pFlipRotator->Initialize(pSourceBitmap, transformOptions);
				}

				if (SUCCEEDED(hr))
				{
					*ppOutputBitmap = pFlipRotator;
					pFlipRotator->AddRef();
				}

				pFlipRotator->Release();
			}
		}
	}

	return hr;
}

HRESULT WICCreateOrientedBitmapSource(IWICImagingFactory* pFactory, IWICBitmapSource* pSourceBitmap,
	IWICMetadataQueryReader* pQueryReader, IWICBitmapSource** ppOrientedBitmap)
{
	*ppOrientedBitmap = nullptr;

	WICBitmapTransformOptions transformOptions = WICBitmapTransformRotate0;
	HRESULT hr = WICGetTransformOptionFromMetadata(pQueryReader, &transformOptions);
	if (SUCCEEDED(hr))
	{
		IWICBitmapFlipRotator* pFlipRotator = nullptr;
		hr = pFactory->CreateBitmapFlipRotator(&pFlipRotator);
		if (SUCCEEDED(hr))
		{
			hr = pFlipRotator->Initialize(pSourceBitmap, transformOptions);
			if (SUCCEEDED(hr))
			{
				*ppOrientedBitmap = pFlipRotator;
				pFlipRotator->AddRef();
			}

			pFlipRotator->Release();
		}
	}

	return hr;
}

HRESULT WICOrientateFrame(IWICImagingFactory* pFactory, IWICBitmapDecoder* pDecoder,
	IWICBitmapFrameDecode* pFrameDecode, bool fCacheBitmap, IWICBitmapSource** ppOutputBitmap)
{
	*ppOutputBitmap = nullptr;
	HRESULT hr = S_OK;

	if (WICIsOrientationSupported(pDecoder))
	{
		IWICMetadataQueryReader* pQueryReader;
		pQueryReader = nullptr;
		if (SUCCEEDED(pFrameDecode->GetMetadataQueryReader(&pQueryReader)))
		{
			if (fCacheBitmap)
			{
				hr = WICCreateCachedOrientedBitmapSource(pFactory, pFrameDecode, pQueryReader, ppOutputBitmap);
			}
			else
			{
				hr = WICCreateOrientedBitmapSource(pFactory, pFrameDecode, pQueryReader, ppOutputBitmap);
			}
			pQueryReader->Release();
		}
		else
		{
			*ppOutputBitmap = pFrameDecode;
			pFrameDecode->AddRef();
		}
	}
	else
	{
		*ppOutputBitmap = pFrameDecode;
		pFrameDecode->AddRef();
	}

	return hr;
}

HRESULT LoadImageWithWIC(IWICImagingFactory* pWICImagingFactory, IStream* pStream, LOAD_IMAGE_WITH_WIC_OPTION option,
	IWICBitmapSource** ppWICBitmapSource, IWICBitmapFrameDecode** ppWICBitmapFrameDecode, GUID* pguidContainerFormat)
{
	*ppWICBitmapSource = nullptr;
	if (ppWICBitmapFrameDecode)
		*ppWICBitmapFrameDecode = nullptr;
	if (pguidContainerFormat)
		*pguidContainerFormat = GUID_NULL;

	HRESULT hr = S_OK;

	Microsoft::WRL::ComPtr<IWICImagingFactory> spWICImagingFactory(pWICImagingFactory);
	if (!spWICImagingFactory.Get())
	{
		hr = CoCreateInstance(
			CLSID_WICImagingFactory2, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&spWICImagingFactory));
	}
	if (SUCCEEDED(hr))
	{
		Microsoft::WRL::ComPtr<IWICBitmapDecoder> spDecoder;
		hr = spWICImagingFactory->CreateDecoderFromStream(
			pStream, &GUID_VendorMicrosoftBuiltIn, WICDecodeMetadataCacheOnDemand, &spDecoder);
		if (SUCCEEDED(hr) && pguidContainerFormat)
		{
			hr = spDecoder->GetContainerFormat(pguidContainerFormat);
		}
		if (SUCCEEDED(hr))
		{
			Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> spBitmapFrameDecode;
			hr = spDecoder->GetFrame(0, &spBitmapFrameDecode);
			if (SUCCEEDED(hr))
			{
				if (option != LIWW_NONE)
				{
					hr = WICOrientateFrame(
						spWICImagingFactory.Get(), spDecoder.Get(), spBitmapFrameDecode.Get(),
						option == LIWW_ORIENTATE_AND_CACHE, ppWICBitmapSource);
				}
				else
				{
					hr = spBitmapFrameDecode.CopyTo(ppWICBitmapSource);
				}
			}
			if (SUCCEEDED(hr) && ppWICBitmapFrameDecode)
			{
				*ppWICBitmapFrameDecode = spBitmapFrameDecode.Detach();
			}
		}
	}

	return hr;
}

HRESULT LoadImageWithWIC(IWICImagingFactory* pWICImagingFactory, LPCWSTR pszPath, LOAD_IMAGE_WITH_WIC_OPTION option,
	IWICBitmapSource** ppWICBitmapSource, IWICBitmapFrameDecode** ppWICBitmapFrameDecode, GUID* pguidContainerFormat)
{
	*ppWICBitmapSource = nullptr;
	if (ppWICBitmapFrameDecode)
		*ppWICBitmapFrameDecode = nullptr;
	if (pguidContainerFormat)
		*pguidContainerFormat = GUID_NULL;

	HRESULT hr = S_OK;

	Microsoft::WRL::ComPtr<IWICImagingFactory> spWICImagingFactory(pWICImagingFactory);
	if (!spWICImagingFactory.Get())
	{
		hr = CoCreateInstance(CLSID_WICImagingFactory2, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&spWICImagingFactory));
	}
	if (SUCCEEDED(hr))
	{
		Microsoft::WRL::ComPtr<IWICBitmapDecoder> spDecoder;
		hr = spWICImagingFactory->CreateDecoderFromFilename(
			pszPath, &GUID_VendorMicrosoftBuiltIn, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &spDecoder);
		if (SUCCEEDED(hr) && pguidContainerFormat)
		{
			hr = spDecoder->GetContainerFormat(pguidContainerFormat);
		}
		if (SUCCEEDED(hr))
		{
			Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> spBitmapFrameDecode;
			hr = spDecoder->GetFrame(0, &spBitmapFrameDecode);
			if (SUCCEEDED(hr))
			{
				if (option != LIWW_NONE)
				{
					hr = WICOrientateFrame(
						spWICImagingFactory.Get(), spDecoder.Get(), spBitmapFrameDecode.Get(),
						option == LIWW_ORIENTATE_AND_CACHE, ppWICBitmapSource);
				}
				else
				{
					hr = spBitmapFrameDecode.CopyTo(ppWICBitmapSource);
				}
			}
			if (SUCCEEDED(hr) && ppWICBitmapFrameDecode)
			{
				*ppWICBitmapFrameDecode = spBitmapFrameDecode.Detach();
			}
		}
	}

	return hr;
}

HRESULT ConvertWICBitmapPixelFormat(IWICImagingFactory* pWICImagingFactory, IWICBitmapSource* pWICBitmapSource,
	GUID guidPixelFormatSource, WICBitmapDitherType bitmapDitherType, IWICBitmapSource** ppWICBitmapSource)
{
	*ppWICBitmapSource = nullptr;

	Microsoft::WRL::ComPtr<IWICFormatConverter> spWICFormatConverter;
	HRESULT hr = pWICImagingFactory->CreateFormatConverter(&spWICFormatConverter);
	if (SUCCEEDED(hr))
	{
		hr = spWICFormatConverter->Initialize(
			pWICBitmapSource, guidPixelFormatSource, bitmapDitherType, nullptr, 0.0, WICBitmapPaletteTypeCustom);
		if (SUCCEEDED(hr))
		{
			hr = spWICFormatConverter->QueryInterface(IID_PPV_ARGS(ppWICBitmapSource));
		}
	}

	return hr;
}

HRESULT Convert32bppWICBitmapSourceToHBITMAP(IWICBitmapSource* pWICBitmapSource, HBITMAP* phbmBitmap)
{
	*phbmBitmap = nullptr;

	UINT uWidth, uHeight;
	HRESULT hr = pWICBitmapSource->GetSize(&uWidth, &uHeight);
	if (SUCCEEDED(hr))
	{
		BITMAPINFO bmi = {};
		bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bmi.bmiHeader.biWidth = (LONG)uWidth;
		bmi.bmiHeader.biHeight = -(LONG)uHeight;
		bmi.bmiHeader.biPlanes = 1;
		bmi.bmiHeader.biBitCount = 32;

		void* pBits;

		HBITMAP hBitmap = CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, &pBits, nullptr, 0);
		if (hBitmap)
		{
			WICRect rect = { 0, 0, (INT)uWidth, (INT)uHeight };
			hr = pWICBitmapSource->CopyPixels(&rect, 4 * uWidth, 4 * uHeight * uWidth, (BYTE*)pBits);
			if (SUCCEEDED(hr))
			{
				*phbmBitmap = hBitmap;
			}
			else
			{
				DeleteObject(hBitmap);
			}
		}
		else
		{
			hr = E_OUTOFMEMORY;
		}
	}

	return hr;
}

HRESULT ConvertWICBitmapToHBITMAP(IWICImagingFactory* pWICImagingFactory, IWICBitmapSource* pWICBitmapSource,
	HBITMAP* phbmImage)
{
	*phbmImage = nullptr;
	HRESULT hr = S_OK;

	Microsoft::WRL::ComPtr<IWICImagingFactory> spWICImagingFactory = pWICImagingFactory;
	if (!spWICImagingFactory.Get())
	{
		hr = CoCreateInstance(CLSID_WICImagingFactory2, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&spWICImagingFactory));
	}
	if (SUCCEEDED(hr))
	{
		Microsoft::WRL::ComPtr<IWICBitmapSource> spBitmapSourceConverted;
		hr = ConvertWICBitmapPixelFormat(
			spWICImagingFactory.Get(), pWICBitmapSource, GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, &spBitmapSourceConverted);
		if (SUCCEEDED(hr))
		{
			hr = Convert32bppWICBitmapSourceToHBITMAP(spBitmapSourceConverted.Get(), phbmImage);
		}
	}

	return hr;
}

HRESULT GetBitmapFromRandomStream(Microsoft::WRL::ComPtr<ABI::Windows::Storage::Streams::IRandomAccessStream> stream, HBITMAP* outBitmap)
{
	Microsoft::WRL::ComPtr<IStream> spStream;
	RETURN_IF_FAILED(CreateStreamOverRandomAccessStream(stream.Get(), IID_PPV_ARGS(&spStream)));

	Microsoft::WRL::ComPtr<IWICImagingFactory> spWICFactory;
	RETURN_IF_FAILED(CoCreateInstance(CLSID_WICImagingFactory2, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&spWICFactory)));

	Microsoft::WRL::ComPtr<IWICBitmapSource> spWICBitmapSource;
	RETURN_IF_FAILED(LoadImageWithWIC(spWICFactory.Get(), spStream.Get(), LIWW_NONE, &spWICBitmapSource, nullptr, nullptr));

	//HBITMAP hbmpImage;
	RETURN_IF_FAILED(ConvertWICBitmapToHBITMAP(spWICFactory.Get(), spWICBitmapSource.Get(), outBitmap));
	return S_OK;
}
