#ifndef MILKV_WEBCAM_UTILS
#define MILKV_WEBCAM_UTILS
#include "midware_utils.h"
#include "sample_utils.h"
#include "vi_vo_utils.h"

#include <core/utils/vpss_helper.h>
#include <cvi_comm.h>
#include <rtsp.h>
#include <sample_comm.h>
#include "cvi_tdl.h"
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "cviruntime.h"
#include <sys/types.h>
#include <ifaddrs.h>


#include <iostream>
#include <cstring>
#include <sys/ioctl.h>
#include <net/if.h>
#include <arpa/inet.h>

#include <termios.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <semaphore.h>
#include <ctime>
#include <cJSON/cJSON.h>
#include <sstream>

#include "stb_image.h"
#include "stb_image_write.h"
void *network_thread(void *ip);
CVI_S32 init_param(const cvitdl_handle_t tdl_handle);
void set_sample_mot_config(cvtdl_deepsort_config_t *ds_conf);
float* utilis_get_mid(cvtdl_object_info_t x);
cvtdl_service_brush_t get_random_brush(uint64_t seed, int min) ;
void My_CopyObjectInfo(cvtdl_object_info_t *src,cvtdl_object_info_t *dst);
bool utilis_is_in(cvtdl_object_info_t tar,cvtdl_tracker_info_t obj);
bool utilis_wear_safe_hat(cvtdl_object_info_t tar,cvtdl_tracker_info_t obj);
bool utilis_wear_safe_vest(cvtdl_object_info_t tar,cvtdl_tracker_info_t obj);
void uid_reallc(uint64_t *id, uint8_t *stat_map, uint64_t *stat_idmap);
int open_serial_port(const char *dev);
int8_t Network_SendResult(char * ip, uint16_t port,cvtdl_object_t obj_data, uint32_t pc);
ssize_t write_to_serial(int fd, const std::string &data);
#endif