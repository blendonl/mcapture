#include "mcapture.h"

#include <objbase.h>
#include <wincodec.h>

static const GUID *container_for(McFormat format) {
    switch (format) {
    case MC_FORMAT_JPEG: return &GUID_ContainerFormatJpeg;
    case MC_FORMAT_BMP:  return &GUID_ContainerFormatBmp;
    case MC_FORMAT_PNG:  break;
    }
    return &GUID_ContainerFormatPng;
}

bool mc_image_save(const McImage *img, const wchar_t *path, McFormat format) {
    HRESULT hr_init  = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    bool    did_init = SUCCEEDED(hr_init);

    IWICImagingFactory    *factory   = NULL;
    IWICBitmap            *bitmap    = NULL;
    IWICFormatConverter   *converter = NULL;
    IWICStream            *stream    = NULL;
    IWICBitmapEncoder     *encoder   = NULL;
    IWICBitmapFrameEncode *frame     = NULL;
    bool                   opened    = false;

    UINT w      = (UINT)img->w;
    UINT h      = (UINT)img->h;
    UINT stride = w * 4;

    const wchar_t *step = L"start the image encoder";
    HRESULT hr = CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
                                  &IID_IWICImagingFactory, (void **)&factory);
    if (FAILED(hr)) goto done;

    step = L"read the captured pixels";
    hr = IWICImagingFactory_CreateBitmapFromMemory(factory, w, h,
                                                   &GUID_WICPixelFormat32bppBGRA,
                                                   stride, stride * h, img->bits, &bitmap);
    if (FAILED(hr)) goto done;

    step = L"open the file";
    hr = IWICImagingFactory_CreateStream(factory, &stream);
    if (FAILED(hr)) goto done;
    hr = IWICStream_InitializeFromFilename(stream, path, GENERIC_WRITE);
    if (FAILED(hr)) goto done;
    opened = true;

    step = L"encode the image";
    hr = IWICImagingFactory_CreateEncoder(factory, container_for(format), NULL, &encoder);
    if (FAILED(hr)) goto done;
    hr = IWICBitmapEncoder_Initialize(encoder, (IStream *)stream, WICBitmapEncoderNoCache);
    if (FAILED(hr)) goto done;
    hr = IWICBitmapEncoder_CreateNewFrame(encoder, &frame, NULL);
    if (FAILED(hr)) goto done;
    hr = IWICBitmapFrameEncode_Initialize(frame, NULL);
    if (FAILED(hr)) goto done;
    hr = IWICBitmapFrameEncode_SetSize(frame, w, h);
    if (FAILED(hr)) goto done;

    WICPixelFormatGUID pixel = GUID_WICPixelFormat24bppBGR;
    hr = IWICBitmapFrameEncode_SetPixelFormat(frame, &pixel);
    if (FAILED(hr)) goto done;

    step = L"convert the pixels";
    hr = IWICImagingFactory_CreateFormatConverter(factory, &converter);
    if (FAILED(hr)) goto done;
    hr = IWICFormatConverter_Initialize(converter, (IWICBitmapSource *)bitmap, &pixel,
                                        WICBitmapDitherTypeNone, NULL, 0.0,
                                        WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) goto done;

    step = L"write the image";
    hr = IWICBitmapFrameEncode_WriteSource(frame, (IWICBitmapSource *)converter, NULL);
    if (FAILED(hr)) goto done;
    hr = IWICBitmapFrameEncode_Commit(frame);
    if (FAILED(hr)) goto done;
    hr = IWICBitmapEncoder_Commit(encoder);

done:
    if (frame)     IWICBitmapFrameEncode_Release(frame);
    if (encoder)   IWICBitmapEncoder_Release(encoder);
    if (stream)    IWICStream_Release(stream);
    if (converter) IWICFormatConverter_Release(converter);
    if (bitmap)    IWICBitmap_Release(bitmap);
    if (factory)   IWICImagingFactory_Release(factory);

    if (FAILED(hr)) {
        if (opened) DeleteFileW(path);
        mc_error(L"could not %ls for %ls (0x%08lX)", step, path, (unsigned long)hr);
    }

    if (did_init) CoUninitialize();
    return SUCCEEDED(hr);
}
