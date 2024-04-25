#define LOG_TAG "RTSP_TEST"
#define LOG_LEVEL LOG_LEVEL_INFO

#include <rtsp_test.h>
#include <unistd.h>
#include <string>
#include <semaphore.h>
#include <ctime>
#include <iostream>
#include <cJSON/cJSON.h>
#include <sstream>
// #include <mongoose/mongoose.h>

// #include <opencv2/opencv.hpp>
static char class_name[][16] = {
    "person\0", "bicycle\0", "car\0", "motorcycle\0", "airplane\0",
    "bus\0", "train\0", "truck\0", "boat\0", "traffic light\0", "fire hydrant\0",
    "stop sign\0", "parking meter\0", "bench\0", "bird\0", "cat\0", "dog\0", "horse\0",
    "sheep\0", "cow\0", "elephant\0", "bear\0", "zebra\0", "giraffe\0", "backpack\0",
    "umbrella\0", "handbag\0", "tie\0", "suitcase\0", "frisbee\0", "skis\0",
    "snowboard\0", "sports ball\0", "kite\0", "baseball bat\0", "baseball glove\0",
    "skateboard\0", "surfboard\0", "tennis racket\0", "bottle\0", "wine glass\0",
    "cup\0", "fork\0", "knife\0", "spoon\0", "bowl\0", "banana\0", "apple\0", "sandwich\0",
    "orange\0", "broccoli\0", "carrot\0", "hot dog\0", "pizza\0", "donut\0", "cake\0",
    "chair\0", "couch\0", "potted plant\0", "bed\0", "dining table\0", "toilet\0", "tv\0",
    "laptop\0", "mouse\0", "remote\0", "keyboard\0", "cell phone\0", "microwave\0",
    "oven\0", "toaster\0", "sink\0", "refrigerator\0", "book\0", "clock\0", "vase\0",
    "scissors\0", "teddy bear\0", "hair drier\0", "toothbrush\0"
};
pthread_t NetThread;
char ip_addr[16] = {0};
sem_t NetSemphore;
int flag = 0;
pthread_mutex_t ResultMutex;


void MY_RTSP_ON_CONNECT(const char *ip, void *arg) {
  strlcpy(ip_addr,ip,16);
  printf("MY RTSP client connected from: %s\n", ip_addr);
  pthread_create(&NetThread, NULL, network_thread, (void *)ip_addr);
}

void MY_RTSP_ON_DISCONNECT(const char *ip, void *arg) {
  printf("MY RTSP client disconnected from: %s\n", ip);
  pthread_cancel(NetThread);
}



static volatile bool bExit = false;

static cvtdl_object_t g_obj_data = {0};

typedef struct {
  SAMPLE_TDL_MW_CONTEXT *pstMWContext;
  cvitdl_service_handle_t stServiceHandle;
} SAMPLE_TDL_VENC_THREAD_ARG_S;


void *network_thread(void *ip){
  int sockfd;
  struct sockaddr_in servaddr;
  cvtdl_object_t stObjMeta3 = {0};
  char UDPPacket[1024] = {0};
  char *ptr;
  uint32_t i;
  
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);

  memset(&servaddr, 0, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_port = htons(11451); 
  servaddr.sin_addr.s_addr = inet_addr((char *) ip); 

  while (true)
  {
    sem_wait(&NetSemphore);
    pthread_mutex_lock(&ResultMutex);
    CVI_TDL_CopyObjectMeta(&g_obj_data, &stObjMeta3);
    pthread_mutex_unlock(&ResultMutex);
    cJSON* DetJson = cJSON_CreateObject();
    for(i = 0 ; i < stObjMeta3.size; i++)
    {
      cJSON *Object = cJSON_CreateArray();
      cJSON_AddItemToArray(Object,cJSON_CreateNumber(stObjMeta3.info[i].classes/1.0));
      cJSON_AddItemToArray(Object,cJSON_CreateString(class_name[stObjMeta3.info[i].classes]));
      cJSON_AddItemToArray(Object,cJSON_CreateNumber(stObjMeta3.info[i].bbox.score));
      cJSON_AddItemToArray(Object,cJSON_CreateNumber(stObjMeta3.info[i].bbox.x1));
      cJSON_AddItemToArray(Object,cJSON_CreateNumber(stObjMeta3.info[i].bbox.y1));
      cJSON_AddItemToArray(Object,cJSON_CreateNumber(stObjMeta3.info[i].bbox.x2));
      cJSON_AddItemToArray(Object,cJSON_CreateNumber(stObjMeta3.info[i].bbox.y2));
      sprintf(UDPPacket,"Obj.%02d_%02d_%s",
                i,stObjMeta3.info[i].classes,class_name[stObjMeta3.info[i].classes]);
      cJSON_AddItemToObject(DetJson,UDPPacket,Object);
    }
    ptr = cJSON_Print(DetJson);
    // printf("UDP_Send :\n %s",ptr);
    sendto(sockfd, ptr, strlen(ptr), 0, (struct sockaddr*)&servaddr, sizeof(servaddr));
    cJSON_Delete(DetJson);
  }
  close(sockfd);

  return 0;
}

void *run_venc(void *args) {
  printf("Enter encoder thread\n");
  SAMPLE_TDL_VENC_THREAD_ARG_S *pstArgs = (SAMPLE_TDL_VENC_THREAD_ARG_S *)args;
  VIDEO_FRAME_INFO_S stFrame;
  CVI_S32 s32Ret;
  cvtdl_object_t stObjMeta2 = {0};

  while (bExit == false) {
    s32Ret = CVI_VPSS_GetChnFrame(0, 0, &stFrame, 2000);
    if (s32Ret != CVI_SUCCESS) {
      printf("CVI_VPSS_GetChnFrame chn0 failed with %#x\n", s32Ret);
      break;
    }

    if(pthread_mutex_trylock(&ResultMutex) != EBUSY)
    {
      CVI_TDL_CopyObjectMeta(&g_obj_data, &stObjMeta2);
      pthread_mutex_unlock(&ResultMutex);
    }

    cvtdl_service_brush_t brushi2;
    brushi2.color.r = 255;
    brushi2.color.g = 0;
    brushi2.color.b = 0;
    brushi2.size = 4;
    s32Ret = CVI_TDL_Service_ObjectDrawRect(pstArgs->stServiceHandle, &stObjMeta2, &stFrame, true,
                                          brushi2);
    if (s32Ret != CVI_TDL_SUCCESS) {
      CVI_VPSS_ReleaseChnFrame(0, 0, &stFrame);
      printf("Draw fame fail!, ret=%x\n", s32Ret);
      pthread_exit(NULL);
    }

    
    s32Ret = SAMPLE_TDL_Send_Frame_RTSP(&stFrame, pstArgs->pstMWContext);
    if (s32Ret != CVI_SUCCESS) {
      CVI_VPSS_ReleaseChnFrame(0, 0, &stFrame);
      printf("Send Output Frame NG, ret=%x\n", s32Ret);
      pthread_exit(NULL);
    }
    // CVI_TDL_Free(&obj_data2);
    CVI_VPSS_ReleaseChnFrame(0, 0, &stFrame);
    if (s32Ret != CVI_SUCCESS) {
      bExit = true;
    }
  }
  printf("Exit encoder thread\n");
  pthread_exit(NULL);
}

void *run_tdl_thread(void *pHandle) {
  printf("Enter TDL thread\n");
  cvitdl_handle_t tdl_handle = (cvitdl_handle_t)pHandle;
  VIDEO_FRAME_INFO_S fdFrame;
  cvtdl_object_t stObjMeta = {0};
  CVI_TDL_SetModelThreshold(tdl_handle, CVI_TDL_SUPPORTED_MODEL_YOLOV5, 0.5);
  CVI_TDL_SetModelNmsThreshold(tdl_handle, CVI_TDL_SUPPORTED_MODEL_YOLOV5, 0.5);
  int sem;

  while (bExit == false) {
    if(CVI_VPSS_GetChnFrame(0, VPSS_CHN1, &fdFrame, 2000) != CVI_SUCCESS)
    {
        CVI_TDL_Free(&stObjMeta);
        pthread_exit(NULL);
    }
    struct timeval t0, t1;

    // Predict face quality score.
    gettimeofday(&t0, NULL);
    if(CVI_TDL_Yolov5(tdl_handle, &fdFrame, &stObjMeta) != CVI_SUCCESS)
    {
        CVI_VPSS_ReleaseChnFrame(0, 1, &fdFrame);
        CVI_TDL_Free(&stObjMeta);
        bExit = true;
        pthread_exit(NULL);
    }
    gettimeofday(&t1, NULL);
    unsigned long execution_time = ((t1.tv_sec - t0.tv_sec) * 1000000 + t1.tv_usec - t0.tv_usec);
    if(stObjMeta.size != 0)
    {
       printf("obj count: %d, exec time=%lu us\n",stObjMeta.size, execution_time);
      for (uint32_t i = 0; i < stObjMeta.size; i++)
       {
        strlcpy(stObjMeta.info[i].name,class_name[stObjMeta.info[i].classes],16);
        printf("detect res: %f %f %f %f %f %s\n", stObjMeta.info[i].bbox.x1, stObjMeta.info[i].bbox.y1,
            stObjMeta.info[i].bbox.x2, stObjMeta.info[i].bbox.y2, stObjMeta.info[i].bbox.score,
            class_name[stObjMeta.info[i].classes]);
      }
      sem_getvalue(&NetSemphore,&sem);
      if(sem == 0){
        sem_post(&NetSemphore);
      }   
    }

    pthread_mutex_lock(&ResultMutex);
    CVI_TDL_CopyObjectMeta(&stObjMeta, &g_obj_data);
    pthread_mutex_unlock(&ResultMutex);

    CVI_VPSS_ReleaseChnFrame(0, 1, &fdFrame);
    CVI_TDL_Free(&stObjMeta);
  }

  printf("Exit TDL thread\n");
  pthread_exit(NULL);
}

static void SampleHandleSig(CVI_S32 signo) {
  signal(SIGINT, SIG_IGN);
  signal(SIGTERM, SIG_IGN);
  printf("handle signal, signo: %d\n", signo);
  if (SIGINT == signo || SIGTERM == signo) {
    bExit = true;
  }
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    printf(
        "\nUsage: %s RETINA_MODEL_PATH QUALITY_MODEL_PATH INPUT_FORMAT\n\n"
        "\tRETINA_MODEL_PATH, path to retinaface model.\n"
        "\tQUALITY_MODEL_PATH, path to face quality model.\n"
        "\tINPUT_FORMAT, input format of face quality model. 0: RGB888, 1: NV21, 2: YUV420.\n",
        argv[0]);
    return CVI_TDL_FAILURE;
  }
  signal(SIGINT, SampleHandleSig);
  signal(SIGTERM, SampleHandleSig);
  pthread_mutex_init(&ResultMutex, NULL);
  sem_init(&NetSemphore, 0, 0);
  SAMPLE_TDL_MW_CONFIG_S stMWConfig;
  memset(&stMWConfig,0,sizeof(stMWConfig));

  CVI_S32 s32Ret = SAMPLE_TDL_Get_VI_Config(&stMWConfig.stViConfig);
  if (s32Ret != CVI_SUCCESS || stMWConfig.stViConfig.s32WorkingViNum <= 0) {
    printf("Failed to get senor infomation from ini file (/mnt/data/sensor_cfg.ini).\n");
    return -1;
  }

  // Get VI size
  PIC_SIZE_E enPicSize;
  s32Ret = SAMPLE_COMM_VI_GetSizeBySensor(stMWConfig.stViConfig.astViInfo[0].stSnsInfo.enSnsType,
                                          &enPicSize);
  if (s32Ret != CVI_SUCCESS) {
    printf("Cannot get senor size\n");
    return -1;
  }

  SIZE_S stSensorSize;
  s32Ret = SAMPLE_COMM_SYS_GetPicSize(enPicSize, &stSensorSize);
  if (s32Ret != CVI_SUCCESS) {
    printf("Cannot get senor size\n");
    return -1;
  }

  // Setup frame size of video encoder to 1080p
  SIZE_S stVencSize = {
      .u32Width = 1920,
      .u32Height = 1080,
  };

  PIXEL_FORMAT_E enInputFormat;
  enInputFormat = PIXEL_FORMAT_RGB_888;

  stMWConfig.stVBPoolConfig.u32VBPoolCount = 3;

  // VBPool 0 for VPSS Grp0 Chn0
  stMWConfig.stVBPoolConfig.astVBPoolSetup[0].enFormat = VI_PIXEL_FORMAT;
  stMWConfig.stVBPoolConfig.astVBPoolSetup[0].u32BlkCount = 3;
  stMWConfig.stVBPoolConfig.astVBPoolSetup[0].u32Height = stSensorSize.u32Height;
  stMWConfig.stVBPoolConfig.astVBPoolSetup[0].u32Width = stSensorSize.u32Width;
  stMWConfig.stVBPoolConfig.astVBPoolSetup[0].bBind = true;
  stMWConfig.stVBPoolConfig.astVBPoolSetup[0].u32VpssChnBinding = VPSS_CHN0;
  stMWConfig.stVBPoolConfig.astVBPoolSetup[0].u32VpssGrpBinding = (VPSS_GRP)0;

  // VBPool 1 for VPSS Grp0 Chn1
  stMWConfig.stVBPoolConfig.astVBPoolSetup[1].enFormat = enInputFormat;
  stMWConfig.stVBPoolConfig.astVBPoolSetup[1].u32BlkCount = 3;
  stMWConfig.stVBPoolConfig.astVBPoolSetup[1].u32Height = stVencSize.u32Height;
  stMWConfig.stVBPoolConfig.astVBPoolSetup[1].u32Width = stVencSize.u32Width;
  stMWConfig.stVBPoolConfig.astVBPoolSetup[1].bBind = true;
  stMWConfig.stVBPoolConfig.astVBPoolSetup[1].u32VpssChnBinding = VPSS_CHN1;
  stMWConfig.stVBPoolConfig.astVBPoolSetup[1].u32VpssGrpBinding = (VPSS_GRP)0;

  // VBPool 2 for TDL preprocessing
  stMWConfig.stVBPoolConfig.astVBPoolSetup[2].enFormat = PIXEL_FORMAT_RGB_888_PLANAR;
  stMWConfig.stVBPoolConfig.astVBPoolSetup[2].u32BlkCount = 1;
  stMWConfig.stVBPoolConfig.astVBPoolSetup[2].u32Height = 720;
  stMWConfig.stVBPoolConfig.astVBPoolSetup[2].u32Width = 1280;
  stMWConfig.stVBPoolConfig.astVBPoolSetup[2].bBind = false;

  // Setup VPSS Grp0
  stMWConfig.stVPSSPoolConfig.u32VpssGrpCount = 1;
#ifndef CV186X
  stMWConfig.stVPSSPoolConfig.stVpssMode.aenInput[0] = VPSS_INPUT_MEM;
  stMWConfig.stVPSSPoolConfig.stVpssMode.enMode = VPSS_MODE_DUAL;
  stMWConfig.stVPSSPoolConfig.stVpssMode.ViPipe[0] = 0;
  stMWConfig.stVPSSPoolConfig.stVpssMode.aenInput[1] = VPSS_INPUT_ISP;
  stMWConfig.stVPSSPoolConfig.stVpssMode.ViPipe[1] = 0;
#endif

  SAMPLE_TDL_VPSS_CONFIG_S *pstVpssConfig = &stMWConfig.stVPSSPoolConfig.astVpssConfig[0];
  pstVpssConfig->bBindVI = true;

  // Assign device 1 to VPSS Grp0, because device1 has 3 outputs in dual mode.
  VPSS_GRP_DEFAULT_HELPER2(&pstVpssConfig->stVpssGrpAttr, stSensorSize.u32Width,
                           stSensorSize.u32Height, VI_PIXEL_FORMAT, 1);
  pstVpssConfig->u32ChnCount = 2;
  pstVpssConfig->u32ChnBindVI = 0;
  VPSS_CHN_DEFAULT_HELPER(&pstVpssConfig->astVpssChnAttr[0], stVencSize.u32Width,
                          stVencSize.u32Height, VI_PIXEL_FORMAT, true);

  // Prepare Vpss Chn1 for TDL inference
  VPSS_CHN_DEFAULT_HELPER(&pstVpssConfig->astVpssChnAttr[1], stVencSize.u32Width,
                          stVencSize.u32Height, enInputFormat, true);

  // Get default VENC configurations
  SAMPLE_TDL_Get_Input_Config(&stMWConfig.stVencConfig.stChnInputCfg);
  stMWConfig.stVencConfig.u32FrameWidth = stVencSize.u32Width;
  stMWConfig.stVencConfig.u32FrameHeight = stVencSize.u32Height;

  // Get default RTSP configurations
  SAMPLE_TDL_Get_RTSP_Config(&stMWConfig.stRTSPConfig.stRTSPConfig);

  SAMPLE_TDL_MW_CONTEXT stMWContext = {0};
  s32Ret = SAMPLE_TDL_Init_WM(&stMWConfig, &stMWContext);
  if (s32Ret != CVI_SUCCESS) {
    printf("init middleware failed! ret=%x\n", s32Ret);
    return -1;
  }

  cvitdl_handle_t stTDLHandle = NULL;

  if(CVI_TDL_CreateHandle2(&stTDLHandle, 1, 0) != CVI_SUCCESS)
  {
    SAMPLE_TDL_Destroy_MW(&stMWContext);
    return -1;
  }

  if(CVI_TDL_SetVBPool(stTDLHandle, 0, 2) != CVI_SUCCESS)
  {
    CVI_TDL_DestroyHandle(stTDLHandle);
    SAMPLE_TDL_Destroy_MW(&stMWContext);
    return -1;
  }

  CVI_TDL_SetVpssTimeout(stTDLHandle, 1000);

  cvitdl_service_handle_t stServiceHandle = NULL;
  if(CVI_TDL_Service_CreateHandle(&stServiceHandle, stTDLHandle) != CVI_SUCCESS)
  {
    CVI_TDL_DestroyHandle(stTDLHandle);
    SAMPLE_TDL_Destroy_MW(&stMWContext);
    return -1;
  }

  if(CVI_TDL_OpenModel(stTDLHandle, CVI_TDL_SUPPORTED_MODEL_YOLOV5, argv[1]) !=CVI_SUCCESS)
  {
    CVI_TDL_Service_DestroyHandle(stServiceHandle);
    CVI_TDL_DestroyHandle(stTDLHandle);
    SAMPLE_TDL_Destroy_MW(&stMWContext);    
  }

  CVI_TDL_SetModelThreshold(stTDLHandle, CVI_TDL_SUPPORTED_MODEL_YOLOV5, 0.5);
  CVI_TDL_SetModelNmsThreshold(stTDLHandle, CVI_TDL_SUPPORTED_MODEL_YOLOV5, 0.5);
  printf("init model seccess! ");

  // GOTO_IF_FAILED(CVI_TDL_OpenModel(stTDLHandle, CVI_TDL_SUPPORTED_MODEL_FACEQUALITY, argv[2]),
  //                s32Ret, setup_tdl_fail);

  pthread_t stVencThread,stTDLThread;

  SAMPLE_TDL_VENC_THREAD_ARG_S args = {
      .pstMWContext = &stMWContext,
      .stServiceHandle = stServiceHandle,
  };

  pthread_create(&stVencThread, NULL, run_venc, &args);
  pthread_create(&stTDLThread, NULL, run_tdl_thread, stTDLHandle);
  sleep(1);

  pthread_join(stVencThread, NULL);
  pthread_join(stTDLThread, NULL);


  // pthread_join(stHttpThread, NULL);

  CVI_TDL_Service_DestroyHandle(stServiceHandle);
  CVI_TDL_DestroyHandle(stTDLHandle);
  SAMPLE_TDL_Destroy_MW(&stMWContext);

  return 0;
}
