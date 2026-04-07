#include "rv1126_isp_function.h"

int init_all_isp_function()
{
    rk_aiq_working_mode_t hdr_mode = RK_AIQ_WORKING_MODE_NORMAL;
    if (SAMPLE_COMM_ISP_Init(hdr_mode, RK_FALSE) != 0)
        return -1;

    if (SAMPLE_COMM_ISP_Run() != 0)
        return -1;

    SAMPLE_COMM_ISP_SetFrameRate(30);

    return 0;
}
