typedef struct vd_info_s
{
        /* driver name ("Matrox Millennium G200/G400" */
        const char *name;
        /* short name (for config strings) ("mga") */
        const char *short_name;
	/* -vfm id */
	const int id;
        /* interface author/maintainer */
        const char *maintainer;
        /* codec author ("Aaron Holtzman <aholtzma@ess.engr.uvic.ca>") */
        const char *author;
        /* any additional comments */
        const char *comment;
} vd_info_t;

/* interface towards mplayer and */
typedef struct vd_functions_s
{
	vd_info_t *info;
        int (*init)(sh_video_t *sh);
        void (*uninit)(sh_video_t *sh);
        int (*control)(sh_video_t *sh,int cmd,void* arg, ...);
        mp_image_t* (*decode)(sh_video_t *sh,void* data,int len,int flags);
} vd_functions_t;

// NULL terminated array of all drivers
extern vd_functions_t* mpcodecs_vd_drivers[];

#define CONTROL_OK 1
#define CONTROL_TRUE 1
#define CONTROL_FALSE 0
#define CONTROL_UNKNOWN -1
#define CONTROL_ERROR -2
#define CONTROL_NA -3

#define VDCTRL_QUERY_FORMAT 3 /* test for availabilty of a format */

