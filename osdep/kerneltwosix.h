#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,70)
#define __KERNEL__
#include <linux/thread_info.h>
#include <linux/list.h>
#undef __KERNEL__
#endif


