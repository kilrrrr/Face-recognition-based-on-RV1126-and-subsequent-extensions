#include "rkmedia_ffmpeg_config.h"
#include "rkmedia_container.h"
#include "ffmpeg_audio_queue.h"
#include "ffmpeg_video_queue.h"
#include "rkmedia_module_function.h"
#include "rkmedia_assignment_manage.h"
#include "rv1126_isp_control_layer.h"

VIDEO_QUEUE *high_video_queue = NULL;
VIDEO_QUEUE *low_video_queue = NULL;

int main(int argc, char *argv[])
{
    if (argc < 5)
    {
        printf("Please Input ./rv1126_ffmpeg_main high_stream_type high_url_address low_stream_type low_url_address. Notice URL_TYPE: 0-->FLV  1-->TS\n");
        return -1;
    }

    int high_protocol_type = atoi(argv[1]);
    char *high_network_address = argv[2];

    int low_protocol_type = atoi(argv[3]);
    char *low_network_address = argv[4];

    if (init_rv1126_isp_control_layer(NULL) != 0)
    {
        printf("init_rv1126_isp_control_layer failed\n");
        return -1;
    }

    high_video_queue = new VIDEO_QUEUE();
    low_video_queue = new VIDEO_QUEUE();

    init_rkmedia_module_function();
    init_rv1126_first_assignment(high_protocol_type, high_network_address, low_protocol_type, low_network_address);

    while (1)
        sleep(20);

    return 0;
}
