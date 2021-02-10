#ifndef IMAGE_CAPTURE_H
#define IMAGE_CAPTURE_H

//#define STRICT /* Require use of exact types. */
//#define WIN32_LEAN_AND_MEAN 1 /* Speed up compilation. */
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <stdbool.h>    // c语言中没有bool，需要加入此包
//#pragma comment(lib, "D3D11.lib")

#ifdef __cplusplus
extern "C" {
#endif
typedef void(*ImageCaptureCallback)(unsigned char* data, int size, int width, int height, void* user_data);

typedef struct {
    int m_nWidth;//捕获区域-宽度
    int m_nHeight;//捕获区域-高度
    int m_nFps;// 捕获帧率
    int m_nMemSize;//RGB数据缓冲区大小

    bool m_bActive;//是否激活，即是否获取到视频帧
    HANDLE m_hCaptureThread;//捕获线程句柄
    HANDLE m_hStopSignal;//线程停止信号

    ID3D11Device* m_hDevice;//设备对象
    ID3D11DeviceContext* m_hContext;//设备上下文
    IDXGIOutputDuplication* m_hDeskDup;//桌面对象
    DXGI_OUTPUT_DESC m_hOutDesc;//桌面对象描述-保存了桌面分辨率等信息
    DXGI_OUTDUPL_DESC m_hDuplDesc;// 输出的尺寸以及包含桌面图像的表面，桌面映像的格式始终为DXGI_FORMAT_B8G8R8A8_UNORM。

    ImageCaptureCallback m_imageCaptureCallback;//实时捕获数据回调函数
    unsigned char* data;
    void* m_userData;
} ImageCapture;

typedef struct Capture{
    int m_nWidth;//捕获区域-宽度
    int m_nHeight;//捕获区域-高度
    unsigned char* data;
    int data_size;
} Capture;

extern ImageCapture* imageCapture;
/*
 * 初始化ImageCapture 只应该调用一次
 */
extern bool DXGI_InitCapture();

/*
 * 释放ImageCapture 资源 应该与Init成对出现
 */
extern bool DXGI_ReleaseCapture();

/*
 * 开始捕获函数，captureCallback将在捕获线程中执行
 */
extern bool DXGI_StartCapture(
    int fps,
    ImageCaptureCallback captureCallback,
    void* userData);

/*
 * 开始捕获函数(单次捕获), 并返回单张有效图片
 */
extern Capture* DXGI_GetCapture();

/*
 * 返回单张图片
 */
//extern ImageCapture* DXGI_GetImage();

/*
 * 停止捕获函数
 */
extern bool DXGI_StopCapture();

#ifdef __cplusplus
}
#endif

#endif //IMAGE_CAPTURE_H
