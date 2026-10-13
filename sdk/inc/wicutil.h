#include "pch.h"
#include "wincodec.h"
#include <shcore.h>

enum LOAD_IMAGE_WITH_WIC_OPTION
{
	LIWW_NONE,
	LIWW_ORIENTATE,
	LIWW_ORIENTATE_AND_CACHE,
};

bool WICIsOrientationSupported(IWICBitmapDecoder* pDecoder);

HRESULT WICGetTransformOptionFromMetadata(IWICMetadataQueryReader* pQueryReader, WICBitmapTransformOptions* pOptions);

HRESULT WICCreateCachedOrientedBitmapSource(
	IWICImagingFactory* pFactory,
	IWICBitmapSource* pSourceBitmap,
	IWICMetadataQueryReader* pQueryReader,
	IWICBitmapSource** ppOutputBitmap);

HRESULT WICCreateOrientedBitmapSource(
	IWICImagingFactory* pFactory, IWICBitmapSource* pSourceBitmap, IWICMetadataQueryReader* pQueryReader,
	IWICBitmapSource** ppOrientedBitmap);

HRESULT WICOrientateFrame(
	IWICImagingFactory* pFactory, IWICBitmapDecoder* pDecoder, IWICBitmapFrameDecode* pFrameDecode, bool fCacheBitmap,
	IWICBitmapSource** ppOutputBitmap);

HRESULT LoadImageWithWIC(
	IWICImagingFactory* pWICImagingFactory, IStream* pStream, LOAD_IMAGE_WITH_WIC_OPTION option,
	IWICBitmapSource** ppWICBitmapSource, IWICBitmapFrameDecode** ppWICBitmapFrameDecode, GUID* pguidContainerFormat);

HRESULT LoadImageWithWIC(
	IWICImagingFactory* pWICImagingFactory, LPCWSTR pszPath, LOAD_IMAGE_WITH_WIC_OPTION option,
	IWICBitmapSource** ppWICBitmapSource, IWICBitmapFrameDecode** ppWICBitmapFrameDecode, GUID* pguidContainerFormat);

HRESULT ConvertWICBitmapPixelFormat(
	IWICImagingFactory* pWICImagingFactory, IWICBitmapSource* pWICBitmapSource, GUID guidPixelFormatSource,
	WICBitmapDitherType bitmapDitherType, IWICBitmapSource** ppWICBitmapSource);

HRESULT Convert32bppWICBitmapSourceToHBITMAP(IWICBitmapSource* pWICBitmapSource, HBITMAP* phbmBitmap);

HRESULT ConvertWICBitmapToHBITMAP(
	IWICImagingFactory* pWICImagingFactory, IWICBitmapSource* pWICBitmapSource, HBITMAP* phbmImage);

HRESULT GetBitmapFromRandomStream(
	Microsoft::WRL::ComPtr<ABI::Windows::Storage::Streams::IRandomAccessStream> stream, HBITMAP* outBitmap);
