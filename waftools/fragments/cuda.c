#define CUDA_VERSION 7050

typedef void * CUcontext;

#include <libavcodec/avcodec.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_cuda.h>

int main(int argc, char *argv[]) {
    enum AVHWDeviceType type = AV_HWDEVICE_TYPE_CUDA;
    AVCUDADeviceContextInternal *foo;
    AVCodecContext *avctx = avcodec_alloc_context3(NULL);
    avctx->hw_device_ctx = NULL;
    return 0;
}
