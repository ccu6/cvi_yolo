#include "MilkV_WebCam_Utils.hpp"

CVI_S32 init_param(const cvitdl_handle_t tdl_handle) {
  // setup preprocess
  YoloPreParam preprocess_cfg =
      CVI_TDL_Get_YOLO_Preparam(tdl_handle, CVI_TDL_SUPPORTED_MODEL_YOLOV8_DETECTION);

  for (int i = 0; i < 3; i++) {
    printf("asign val %d \n", i);
    preprocess_cfg.factor[i] = 0.003922;
    preprocess_cfg.mean[i] = 0.0;
  }
  preprocess_cfg.format = PIXEL_FORMAT_RGB_888_PLANAR;

  printf("setup yolov8 param \n");
  CVI_S32 ret = CVI_TDL_Set_YOLO_Preparam(tdl_handle, CVI_TDL_SUPPORTED_MODEL_YOLOV8_DETECTION,
                                          preprocess_cfg);
  if (ret != CVI_SUCCESS) {
    printf("Can not set yolov8 preprocess parameters %#x\n", ret);
    return ret;
  }

  // setup yolo algorithm preprocess
  YoloAlgParam yolov8_param = CVI_TDL_Get_YOLO_Algparam(tdl_handle, CVI_TDL_SUPPORTED_MODEL_YOLOV8_DETECTION);
  yolov8_param.cls = 10;

  printf("setup yolov8 algorithm param \n");
  ret = CVI_TDL_Set_YOLO_Algparam(tdl_handle, CVI_TDL_SUPPORTED_MODEL_YOLOV8_DETECTION, yolov8_param);
  if (ret != CVI_SUCCESS) {
    printf("Can not set yolov8 algorithm parameters %#x\n", ret);
    return ret;
  }

  // set theshold
  CVI_TDL_SetModelThreshold(tdl_handle, CVI_TDL_SUPPORTED_MODEL_YOLOV8_DETECTION, 0.5);
  CVI_TDL_SetModelNmsThreshold(tdl_handle, CVI_TDL_SUPPORTED_MODEL_YOLOV8_DETECTION, 0.5);

  printf("yolov8 algorithm parameters setup success!\n");
  return ret;
}

float* utilis_get_mid(cvtdl_object_info_t x)
{
  static float out[2] = {0.0,0.0};
  out[0] = (x.bbox.x1 + x.bbox.x2) / 2;
  out[1] = (x.bbox.y1 + x.bbox.y2) / 2;
  return out;
}

bool utilis_wear_safe_hat(cvtdl_object_info_t tar,cvtdl_tracker_info_t obj)
{
  float mid_x = (tar.bbox.x1 + tar.bbox.x2)/2;
  float mid_y = (tar.bbox.y1 + tar.bbox.y2)/2;
  float mid_obj_x = (obj.bbox.x1 + obj.bbox.x2)/2;
  float mid_obj_y = (obj.bbox.y1 + obj.bbox.y2)/2;
  float len_y = obj.bbox.y2 - obj.bbox.y1;
  // if((mid_x >= obj.bbox.x1) && (mid_x <= obj.bbox.x2) && (mid_y >= obj.bbox.y1) && (mid_y <= obj.bbox.y2)){
    if((mid_y <= (obj.bbox.y1 + (len_y * 0.4)))&&(mid_y >= (obj.bbox.y1 - (len_y * 0.4))) && (mid_x >= obj.bbox.x1) && (mid_x <= obj.bbox.x2))
      return true;
    else
      return false;
  // }
  // else
  //   return false;
}
bool utilis_wear_safe_vest(cvtdl_object_info_t tar,cvtdl_tracker_info_t obj)
{
  float mid_x = (tar.bbox.x1 + tar.bbox.x2)/2;
  float mid_y = (tar.bbox.y1 + tar.bbox.y2)/2;
  if((mid_x >= obj.bbox.x1) && (mid_x <= obj.bbox.x2) && (mid_y >= obj.bbox.y1) && (mid_y <= obj.bbox.y2)){
    // if((tar.bbox.y2 >= obj.bbox.y2*0.8)&&(tar.bbox.y2 <= obj.bbox.y2*1.2))
      return true;
    // else
    //   return false;
  }
  else
    return false;
}

bool utilis_is_in(cvtdl_object_info_t tar,cvtdl_tracker_info_t obj)
{
  float mid_x = (tar.bbox.x1 + tar.bbox.x2)/2;
  float mid_y = (tar.bbox.y1 + tar.bbox.y2)/2;
  if((mid_x >= obj.bbox.x1) && (mid_x <= obj.bbox.x2) && (mid_y >= obj.bbox.y1) && (mid_y <= obj.bbox.y2)){
      return true;
  }
  else
    return false;
}

// float* utilis_is_same_object(cvtdl_object_info_t n,cvtdl_object_info_t p1,cvtdl_object_info_t p2,uint16_t error)
// {
//   float out[2] = {0.0,0.0};
//   out[0] = (x.bbox.x1 + x.bbox.x2) / 2;
//   out[1] = (x.bbox.y1 + x.bbox.y2) / 2;
//   return out;
// }

void set_sample_mot_config(cvtdl_deepsort_config_t *ds_conf) {
  ds_conf->ktracker_conf.P_beta[2] = 0.01;
  ds_conf->ktracker_conf.P_beta[6] = 1e-5;

  // ds_conf.kfilter_conf.Q_beta[2] = 0.1;
  ds_conf->kfilter_conf.Q_beta[2] = 0.01;
  ds_conf->kfilter_conf.Q_beta[6] = 1e-5;
  ds_conf->kfilter_conf.R_beta[2] = 0.1;
}

cvtdl_service_brush_t get_random_brush(uint64_t seed, int min) {
  float scale = (256. - (float)min) / 256.;
  srand((uint32_t)seed);
  cvtdl_service_brush_t brush = {0};
  brush.color.r = (int)((floor(((float)rand() / (RAND_MAX)) * 256.)) * scale) + min;
  brush.color.g = (int)((floor(((float)rand() / (RAND_MAX)) * 256.)) * scale) + min;
  brush.color.b = (int)((floor(((float)rand() / (RAND_MAX)) * 256.)) * scale) + min;
  brush.size = 4;
  return brush;
}

void My_CopyObjectInfo(cvtdl_object_info_t *src,cvtdl_object_info_t *dst)
{
  dst->bbox.score = src->bbox.score;
  dst->bbox.x1 = src->bbox.x1;
  dst->bbox.x2 = src->bbox.x2;
  dst->bbox.y1 = src->bbox.y1;
  dst->bbox.y2 = src->bbox.y2;
  dst->classes = src->classes;
  dst->feature = src->feature;
  dst->is_cross = src->is_cross;
  dst->pedestrian_properity = src->pedestrian_properity;
  dst->unique_id = src->unique_id;
  dst->vehicle_properity = src->vehicle_properity;
  dst->track_state = src->track_state;
}

void uid_reallc(uint64_t *id, uint8_t *stat_map, uint64_t *stat_idmap){
  if(*id >= 255){
    for (uint32_t i = 0;i < 256;i ++){
      if(stat_idmap[i] == *id){
        *id = i;
        return;
      }
    }
    for (uint32_t i = 0;i < 256;i ++){
      if(stat_map[i] == 0x00){
        stat_idmap[i] = *id;
        *id = i;
        return ;
      } 
    }
  }
}
int8_t Network_SendResult(char * ip, uint16_t port,cvtdl_object_t obj_data, uint32_t pc){
    if(ip[0] == 0) {   
        printf("ip not set\n");
        return -1;
    }
    int sockfd;
    struct sockaddr_in servaddr;
    char UDPPacket[1024] = {0};
    char *ptr;
    uint32_t i;
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port); 
    servaddr.sin_addr.s_addr = inet_addr(ip); 
    cJSON* DetJson = cJSON_CreateObject();
    for(i = 0 ; i < obj_data.size; i++)
    {
        if(obj_data.info[i].classes > 11){
        cJSON_AddItemToObject(DetJson,"Person_count",cJSON_CreateNumber(pc));
        cJSON_AddItemToObject(DetJson,"Calsses",cJSON_CreateNumber(obj_data.info[i].classes - 10));
        snprintf(UDPPacket,1023,"%s.png",obj_data.info[i].name);
        cJSON_AddItemToObject(DetJson,"Image",cJSON_CreateString(UDPPacket));
        cJSON_AddItemToObject(DetJson,"x1",cJSON_CreateNumber(obj_data.info[i].bbox.x1));
        cJSON_AddItemToObject(DetJson,"y1",cJSON_CreateNumber(obj_data.info[i].bbox.y1));
        cJSON_AddItemToObject(DetJson,"x2",cJSON_CreateNumber(obj_data.info[i].bbox.x2));
        cJSON_AddItemToObject(DetJson,"y2",cJSON_CreateNumber(obj_data.info[i].bbox.y2));
        ptr = cJSON_Print(DetJson);
        int ret = sendto(sockfd, ptr, strlen(ptr), 0, (struct sockaddr*)&servaddr, sizeof(servaddr));
        free(ptr);
        }
        // printf("%s.png",obj_data.info[i].name);
    }
    cJSON_Delete(DetJson);
  close(sockfd);
  return 0;
}


int open_serial_port(const char *dev) {
    int fd = open(dev, O_RDWR | O_NOCTTY | O_NDELAY);
    if (fd == -1) {
        std::cerr << "Unable to open serial port " << dev << std::endl;
        return -1;
    }
    fcntl(fd, F_SETFL, 0);
    struct termios options;
    tcgetattr(fd, &options);
    cfsetispeed(&options, B115200);
    cfsetospeed(&options, B115200);
    options.c_cflag |= (CLOCAL | CREAD);
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8;
    options.c_cflag &= ~PARENB;
    options.c_cflag &= ~CSTOPB;
    options.c_cc[VMIN] = 1;
    options.c_cc[VTIME] = 0;
    tcsetattr(fd, TCSANOW, &options);
    return fd;
}
 
ssize_t write_to_serial(int fd, const std::string &data) {
    return write(fd, data.c_str(), data.length());
}
 
ssize_t read_from_serial(int fd, std::string &buffer, size_t length) {
    buffer.resize(length);
    ssize_t bytes_read = read(fd, &buffer[0], length);
    if (bytes_read > 0) {
        buffer.resize(bytes_read);
    }
    return bytes_read;
}