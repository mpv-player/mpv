#define CUDA_VERSION 7050

typedef void * CUcontext;

#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_cuda.h>

int main(int argc, char *argv[]) {
    enum AVHWDeviceType type = AV_HWDEVICE_TYPE_CUDA;
    AVCUDADeviceContextInternal *foo;
    return 0;
}
