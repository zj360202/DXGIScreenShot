#include "image_capture.h"
#include <process.h>
#include <cstdio>

#define RESET_OBJECT(A) {if (A) A->Release(); A = nullptr;}

ImageCapture *imageCapture = new ImageCapture();
/*
 * 将桌面挂到这个进程中
 */
static bool AttachToThread() {
    HDESK oldDesktop = GetThreadDesktop(GetCurrentThreadId());
    HDESK currentDesktop = OpenInputDesktop(0, FALSE, GENERIC_ALL);
    if (!currentDesktop) {
        return false;
    }

    bool attached = SetThreadDesktop(currentDesktop);
    CloseDesktop(oldDesktop);
    CloseDesktop(currentDesktop);

    return attached;
}

static void mapDesktopSurface() {
    IDXGIResource* desktopResource = nullptr;
    DXGI_OUTDUPL_FRAME_INFO frameInfo;
    DXGI_MAPPED_RECT mappedRect;

    ///截取屏幕数据
    HRESULT hr = imageCapture->m_hDeskDup->AcquireNextFrame(0, &frameInfo, &desktopResource);
    if (FAILED(hr)) {
        // 在一些win10的系统上,如果桌面没有变化的情况下，
        // 这里会发生超时现象，但是这并不是发生了错误，而是系统优化了刷新动作导致的。
        printf("AcquireNextFrame failure1! \n");
        return;
    }

    hr = imageCapture->m_hDeskDup->MapDesktopSurface(&mappedRect);
    if (FAILED(hr)) {
        printf("MapDesktopSurface failure! \n");
        imageCapture->m_hDeskDup->ReleaseFrame();
        return;
    }

    if (imageCapture->m_bActive) {
        if (imageCapture->m_imageCaptureCallback) {
            imageCapture->m_imageCaptureCallback(mappedRect.pBits,
                imageCapture->m_nMemSize,
                imageCapture->m_nWidth,
                imageCapture->m_nHeight,
                imageCapture->m_userData);
        }
    } else {
        imageCapture->m_bActive = true;
    }

    imageCapture->m_hDeskDup->UnMapDesktopSurface();
}
static void mapSurface() {
    IDXGIResource* desktopResource = nullptr;
    DXGI_OUTDUPL_FRAME_INFO frameInfo;
    ID3D11Texture2D* acquiredDesktopImage = nullptr;
    D3D11_TEXTURE2D_DESC frameDescriptor;
    ID3D11Texture2D* newDesktopImage = nullptr;
    IDXGISurface* dxgiSurface = nullptr;
    DXGI_MAPPED_RECT mappedRect;

    HRESULT hr = imageCapture->m_hDeskDup->AcquireNextFrame(0, &frameInfo, &desktopResource);
    if (FAILED(hr)) {
        // 在一些win10的系统上,如果桌面没有变化的情况下，
        // 这里会发生超时现象，但是这并不是发生了错误，而是系统优化了刷新动作导致的。
        printf("AcquireNextFrame failure2! \n");
        return;
    }
    hr = desktopResource->QueryInterface(__uuidof(ID3D11Texture2D),
        reinterpret_cast<void**>(&acquiredDesktopImage));
    RESET_OBJECT(desktopResource);
    if (FAILED(hr)) {
        printf("QueryInterface failure! \n");
        return;
    }
    acquiredDesktopImage->GetDesc(&frameDescriptor);

    //创建一个新的2D纹理对象,用于把 hAcquiredDesktopImage的数据copy进去
    frameDescriptor.Usage = D3D11_USAGE_STAGING;
    frameDescriptor.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    frameDescriptor.BindFlags = 0;
    frameDescriptor.MiscFlags = 0;
    frameDescriptor.MipLevels = 1;
    frameDescriptor.ArraySize = 1;
    frameDescriptor.SampleDesc.Count = 1;


    hr = imageCapture->m_hDevice->CreateTexture2D(&frameDescriptor,
        nullptr,
        &newDesktopImage);
    if (FAILED(hr)) {
        printf("CreateTexture2D failure! \n");
        RESET_OBJECT(acquiredDesktopImage);
        imageCapture->m_hDeskDup->ReleaseFrame();
        return;
    }

    ///获取整个帧的数据
    imageCapture->m_hContext->CopyResource((ID3D11Resource*)newDesktopImage,
        (ID3D11Resource*)acquiredDesktopImage);
    RESET_OBJECT(acquiredDesktopImage);
    imageCapture->m_hDeskDup->ReleaseFrame();

    // 获取这个2D纹理对象的表面
    hr = newDesktopImage->QueryInterface(__uuidof(IDXGISurface),
        reinterpret_cast<void**>(&dxgiSurface));
    RESET_OBJECT(newDesktopImage);
    if (FAILED(hr)) {
        printf("QueryInterface failure! \n");
        return;
    }

    //映射锁定表面,从而获取表面的数据地址
    //这个时候 mappedRect.pBits 指向的内存就是原始的图像数据, DXGI固定为 32位深度色
    hr = dxgiSurface->Map(&mappedRect, DXGI_MAP_READ);
    if (FAILED(hr)) {
        printf("Map  failure! \n");
        RESET_OBJECT(dxgiSurface);
        return;
    }

    if (imageCapture->m_bActive) {
        if (imageCapture->m_imageCaptureCallback) {
            imageCapture->m_imageCaptureCallback(mappedRect.pBits,
                imageCapture->m_nMemSize,
                imageCapture->m_nWidth,
                imageCapture->m_nHeight,
                imageCapture->m_userData);
        }
    } else {
        imageCapture->m_bActive = true;
    }
    dxgiSurface->Unmap();

    RESET_OBJECT(dxgiSurface);
}

static void mapDesktopSurfaceSingle() {
    IDXGIResource* desktopResource = nullptr;
    DXGI_OUTDUPL_FRAME_INFO frameInfo;
    DXGI_MAPPED_RECT mappedRect;
    //printf("mapDesktopSurfaceSingle \n");

    ///截取屏幕数据
    HRESULT hr = imageCapture->m_hDeskDup->AcquireNextFrame(0, &frameInfo, &desktopResource);
    if (FAILED(hr)) {
        // 在一些win10的系统上,如果桌面没有变化的情况下，
        // 这里会发生超时现象，但是这并不是发生了错误，而是系统优化了刷新动作导致的。
        printf("AcquireNextFrame failure3! \n");
        return;
    }

    hr = imageCapture->m_hDeskDup->MapDesktopSurface(&mappedRect);
    if (FAILED(hr)) {
        printf("MapDesktopSurface failure! \n");
        imageCapture->m_hDeskDup->ReleaseFrame();
        return;
    }

    if (imageCapture->m_bActive) {
        imageCapture->data = mappedRect.pBits;
    } else {
        imageCapture->m_bActive = true;
    }

    imageCapture->m_hDeskDup->UnMapDesktopSurface();
}
static void mapSurfaceSingle() {
    //printf("mapSurfaceSingle \n");
    IDXGIResource* desktopResource = nullptr;
    DXGI_OUTDUPL_FRAME_INFO frameInfo;
    ID3D11Texture2D* acquiredDesktopImage = nullptr;
    D3D11_TEXTURE2D_DESC frameDescriptor;
    ID3D11Texture2D* newDesktopImage = nullptr;
    IDXGISurface* dxgiSurface = nullptr;
    DXGI_MAPPED_RECT mappedRect;

    printf("mapSurfaceSingle1 \n");
    HRESULT hr = imageCapture->m_hDeskDup->AcquireNextFrame(0, &frameInfo, &desktopResource);
    if (FAILED(hr)) {
        // 在一些win10的系统上,如果桌面没有变化的情况下，
        // 这里会发生超时现象，但是这并不是发生了错误，而是系统优化了刷新动作导致的。
        printf("AcquireNextFrame failure4! \n");
        return;
    }
    printf("mapSurfaceSingle2 \n");
    hr = desktopResource->QueryInterface(__uuidof(ID3D11Texture2D),
        reinterpret_cast<void**>(&acquiredDesktopImage));
    RESET_OBJECT(desktopResource);
    if (FAILED(hr)) {
        printf("QueryInterface failure! \n");
        return;
    }
    //printf("mapSurfaceSingle3 \n");
    acquiredDesktopImage->GetDesc(&frameDescriptor);

    //printf("mapSurfaceSingle4 \n");
    //创建一个新的2D纹理对象,用于把 hAcquiredDesktopImage的数据copy进去
    frameDescriptor.Usage = D3D11_USAGE_STAGING;
    frameDescriptor.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    frameDescriptor.BindFlags = 0;
    frameDescriptor.MiscFlags = 0;
    frameDescriptor.MipLevels = 1;
    frameDescriptor.ArraySize = 1;
    frameDescriptor.SampleDesc.Count = 1;

    //printf("mapSurfaceSingle5 \n");
    hr = imageCapture->m_hDevice->CreateTexture2D(&frameDescriptor,
        nullptr,
        &newDesktopImage);
    if (FAILED(hr)) {
        printf("CreateTexture2D failure! \n");
        RESET_OBJECT(acquiredDesktopImage);
        imageCapture->m_hDeskDup->ReleaseFrame();
        return;
    }

    //printf("mapSurfaceSingle6 \n");
    ///获取整个帧的数据
    imageCapture->m_hContext->CopyResource((ID3D11Resource*)newDesktopImage,
        (ID3D11Resource*)acquiredDesktopImage);
    RESET_OBJECT(acquiredDesktopImage);
    //printf("mapSurfaceSingle7 \n");
    imageCapture->m_hDeskDup->ReleaseFrame();

    //printf("mapSurfaceSingle8 \n");
    // 获取这个2D纹理对象的表面
    hr = newDesktopImage->QueryInterface(__uuidof(IDXGISurface),
        reinterpret_cast<void**>(&dxgiSurface));
    RESET_OBJECT(newDesktopImage);
    if (FAILED(hr)) {
        printf("QueryInterface failure! \n");
        return;
    }

    //printf("mapSurfaceSingle9 \n");
    //映射锁定表面,从而获取表面的数据地址
    //这个时候 mappedRect.pBits 指向的内存就是原始的图像数据, DXGI固定为 32位深度色
    hr = dxgiSurface->Map(&mappedRect, DXGI_MAP_READ);
    if (FAILED(hr)) {
        printf("Map  failure! \n");
        RESET_OBJECT(dxgiSurface);
        return;
    }

    //printf("mapSurfaceSingle10 \n");
    if (imageCapture->m_bActive) {
        imageCapture->data = mappedRect.pBits;
    } else {
        imageCapture->m_bActive = true;
    }
    dxgiSurface->Unmap();

    //printf("mapSurfaceSingle11 \n");
    RESET_OBJECT(dxgiSurface);
}
static int  max(int a, int b){
    if (a > b){return a;}
    else{return b;}
}

/*
 * 视频数据循环捕获线程
 */
static unsigned WINAPI OnImageCaptureThread(void* param) {
    ImageCapture* imgCapture;
    imgCapture = static_cast<ImageCapture*>(param);

    int fps = max(imgCapture->m_nFps, 10);
    DWORD dwTimeout = 1000 / fps;

    if (!AttachToThread()) {
        printf("attach To thread failure! \n");
        goto __exit;
    }
    printf("%d \n", int(dwTimeout));
    // 等待超时进入下一次图像数据获取
    while (WaitForSingleObject(imgCapture->m_hStopSignal, dwTimeout) == WAIT_TIMEOUT) {
        if (imgCapture->m_hDuplDesc.DesktopImageInSystemMemory) {
            mapDesktopSurface();
        } else {
            mapSurface();
        }
    }

__exit:
    _endthreadex(0);
    return 0;
}

/*
 * 视频数据单次捕获线程
 */
static unsigned WINAPI OnImageCaptureSingle() {
    //printf("OnImageCaptureSingle \n");
    // 等待超时进入下一次图像数据获取
    if (imageCapture->m_hDuplDesc.DesktopImageInSystemMemory) {
        mapDesktopSurfaceSingle();
    } else {
        mapSurfaceSingle();
    }
    return 0;
}

//开始捕获视频数据
static bool StartImageCaptureThread() {
    // 创建停止信号
    imageCapture->m_hStopSignal = CreateEvent(nullptr, TRUE, FALSE, nullptr);

    // 创建捕获线程
    unsigned int dwThreadId;
    imageCapture->m_hCaptureThread = (HANDLE)_beginthreadex(nullptr,
        0,
        &OnImageCaptureThread,
        imageCapture,
        THREAD_PRIORITY_NORMAL,
        &dwThreadId);

    return imageCapture->m_hCaptureThread != nullptr;
}

static bool StopImageCaptureThread() {
    if (!imageCapture->m_hStopSignal || !imageCapture->m_hCaptureThread) {
        return true;
    }

    //发送线程停止工作信号
    SetEvent(imageCapture->m_hStopSignal);
    //等待线程安全退出
    WaitForSingleObject(imageCapture->m_hCaptureThread, INFINITE);
    //关闭线程句柄
    CloseHandle(imageCapture->m_hCaptureThread);
    imageCapture->m_hCaptureThread = nullptr;
    // 关闭停止信号句柄
    CloseHandle(imageCapture->m_hStopSignal);
    imageCapture->m_hStopSignal = nullptr;

    return true;
}

// DXGI方式，初始化Capture
bool DXGI_InitCapture() {
    INT nOutput = 0;
    IDXGIDevice* hDxgiDevice = nullptr;
    IDXGIAdapter* hDxgiAdapter = nullptr;
    IDXGIOutput* hDxgiOutput = nullptr;
    IDXGIOutput1* hDxgiOutput1 = nullptr;

    //参数初始化
    imageCapture->m_bActive = false;
    imageCapture->m_hCaptureThread = nullptr;
    imageCapture->m_imageCaptureCallback = nullptr;
    imageCapture->m_userData = nullptr;

    HRESULT hr = S_OK;
    //Direct3D驱动类型
    D3D_DRIVER_TYPE DriverTypes[] =
    {
            D3D_DRIVER_TYPE_HARDWARE, //硬件驱动
            D3D_DRIVER_TYPE_WARP,//软件驱动-性能高
            D3D_DRIVER_TYPE_REFERENCE, //软件驱动-精度高，速度慢
            D3D_DRIVER_TYPE_SOFTWARE,//软件驱动-性能低
    };

    UINT NumDriverTypes = ARRAYSIZE(DriverTypes);
    D3D_FEATURE_LEVEL FeatureLevels[] =
    {
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1,
            D3D_FEATURE_LEVEL_10_0,
            D3D_FEATURE_LEVEL_9_1
    };

    //初始化D3D设备-m_hDevice
    UINT NumFeatureLevels = ARRAYSIZE(FeatureLevels);
    D3D_FEATURE_LEVEL FeatureLevel;
    for (UINT DriverTypeIndex = 0; DriverTypeIndex < NumDriverTypes; ++DriverTypeIndex) {
        hr = D3D11CreateDevice(nullptr,
            DriverTypes[DriverTypeIndex],
            nullptr,
            0,
            FeatureLevels,
            NumFeatureLevels,
            D3D11_SDK_VERSION,
            &imageCapture->m_hDevice,
            &FeatureLevel,
            &imageCapture->m_hContext);

        if (SUCCEEDED(hr)) {
            break;
        }
    }

    if (FAILED(hr)) {
        DXGI_ReleaseCapture();
        return false;
    }

    hr = imageCapture->m_hDevice->QueryInterface(__uuidof(IDXGIDevice),
        reinterpret_cast<void**>(&hDxgiDevice));
    if (FAILED(hr)) {
        DXGI_ReleaseCapture();
        return false;
    }

    //获取桌面对象描述符-m_hOutDesc，主要是获取桌面分辨率大小
    hr = hDxgiDevice->GetParent(__uuidof(IDXGIAdapter),
        reinterpret_cast<void**>(&hDxgiAdapter));
    RESET_OBJECT(hDxgiDevice);

    if (FAILED(hr)) {
        DXGI_ReleaseCapture();
        return false;
    }

    hr = hDxgiAdapter->EnumOutputs(nOutput, &hDxgiOutput);
    RESET_OBJECT(hDxgiAdapter);
    if (FAILED(hr)) {
        DXGI_ReleaseCapture();
        return false;
    }

    hDxgiOutput->GetDesc(&imageCapture->m_hOutDesc);
    imageCapture->m_nWidth = imageCapture->m_hOutDesc.DesktopCoordinates.right
        - imageCapture->m_hOutDesc.DesktopCoordinates.left;
    imageCapture->m_nHeight = imageCapture->m_hOutDesc.DesktopCoordinates.bottom
        - imageCapture->m_hOutDesc.DesktopCoordinates.top;

    //计算所需存放图像的缓存大小
    imageCapture->m_nMemSize = imageCapture->m_nWidth * imageCapture->m_nHeight * 4;//获取的图像位图深度32位，所以是*4

    hr = hDxgiOutput->QueryInterface(__uuidof(IDXGIOutput1),
        reinterpret_cast<void**>(&hDxgiOutput1));
    RESET_OBJECT(hDxgiOutput);
    if (FAILED(hr)) {
        DXGI_ReleaseCapture();
        return false;
    }

    hr = hDxgiOutput1->DuplicateOutput(imageCapture->m_hDevice, &imageCapture->m_hDeskDup);
    RESET_OBJECT(hDxgiOutput1);

    if (FAILED(hr)) {
        DXGI_ReleaseCapture();
        return false;
    }

    imageCapture->m_hDeskDup->GetDesc(&imageCapture->m_hDuplDesc);

    return true;
}

bool DXGI_ReleaseCapture() {
    DXGI_StopCapture();

    RESET_OBJECT(imageCapture->m_hDeskDup);
    RESET_OBJECT(imageCapture->m_hContext);
    RESET_OBJECT(imageCapture->m_hDevice);

    return true;
}

// DXGI方式，设置视频回调函数，并开始捕获视频数据
bool DXGI_StartCapture(int fps,ImageCaptureCallback captureCallback,void* userData) {
    imageCapture->m_nFps = fps;
    imageCapture->m_imageCaptureCallback = captureCallback;
    imageCapture->m_userData = userData;

    // 先停止前一个线程
    StopImageCaptureThread();
    return StartImageCaptureThread();
}

// 开始捕获函数，每次请求返回数据 return: 0表示成功，1表示失败
Capture* DXGI_GetCapture() {
    imageCapture->data = nullptr;  // 清空数据，后续可以判定是否拿到新的数据
    OnImageCaptureSingle();
    if (imageCapture->data != nullptr){
        Capture *capture = (Capture *)malloc(sizeof(Capture));
        capture->m_nWidth = imageCapture->m_nWidth;
        capture->m_nHeight = imageCapture->m_nHeight;
        capture->data = imageCapture->data;
        capture->data_size = imageCapture->m_nMemSize;
        return capture;
    }
    return nullptr;
}

//// 开始捕获函数，每次请求返回数据
//ImageCapture* DXGI_GetImage() {
//    return imageCapture;
//}

// 停止视频数据捕获
bool DXGI_StopCapture() {
    // 停止线程
    StopImageCaptureThread();

    imageCapture->m_bActive = false;
    imageCapture->m_imageCaptureCallback = nullptr;
    imageCapture->m_userData = nullptr;

    return true;
}