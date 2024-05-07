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
static char class_name[][16] = {"Hardhat","Mask","NO-Hardhat","NO-Mask","NO-Safety Vest","Person","Safety Cone","Safety Vest","machinery","vehicle"};
/*
  0: Hardhat
  1: Mask
  2: NO-Hardhat
  3: NO-Mask
  4: NO-Safety Vest 
  5: Person
  6: Safety Cone
  7: Safety Vest
  8: machinery 
  9: vehicle
*/
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
static cvtdl_object_t g_obj_data2 = {0};
static cvtdl_tracker_t g_stTrackerMeta = {0};
static uint32_t g_Personcount = 0;
static uint8_t g_PersonTimeOut[256] = {0};
static uint8_t g_PersonState[256] = {0};

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
  cvtdl_object_t stObjMeta = {0};
  cvtdl_object_t stObjMeta2 = {0};
  cvtdl_tracker_t stTrackerMeta2 = {0};

  // uint32_t s_Personcount = 0;
  uint8_t s_PersonTimeOut[256] = {0};
  uint8_t s_PersonState[256] = {0};
  cvtdl_service_brush_t brush_green = {
    .color={
        .r = 0,
        .g = 255,
        .b = 0,
    },
    .size = 4
  };
  cvtdl_service_brush_t brush_blue = {
    .color={
        .r = 0,
        .g = 0,
        .b = 255,
    },
    .size = 4
  };
  cvtdl_service_brush_t brush_red = {
    .color={
        .r = 255,
        .g = 0,
        .b = 0,
    },
    .size = 4
  };
  cvtdl_service_brush_t brush_yellow = {
    .color={
        .r = 255,
        .g = 255,
        .b = 0,
    },
    .size = 4
  };
  cvtdl_service_brush_t brush_pink = {
    .color={
        .r = 255,
        .g = 0,
        .b = 255,
    },
    .size = 4
  };

  while (bExit == false) {
    s32Ret = CVI_VPSS_GetChnFrame(0, VPSS_CHN0, &stFrame, 2000);
    if (s32Ret != CVI_SUCCESS) {
      printf("CVI_VPSS_GetChnFrame chn0 failed with %#x\n", s32Ret);
      break;
    }

    if(pthread_mutex_trylock(&ResultMutex) != EBUSY)
    {
      CVI_TDL_CopyObjectMeta(&g_obj_data, &stObjMeta);
      CVI_TDL_CopyObjectMeta(&g_obj_data2, &stObjMeta2);
      CVI_TDL_CopyTrackerMeta(&g_stTrackerMeta, &stTrackerMeta2);
      pthread_mutex_unlock(&ResultMutex);
      memcpy(s_PersonState,g_PersonState,256);
      // s_Personcount = g_Personcount;
    }
    
    // s32Ret = CVI_TDL_Service_ObjectDrawRect(pstArgs->stServiceHandle, &stObjMeta2, &stFrame, true,
    //                                       brushi2);

    cvtdl_service_brush_t *brushes = (cvtdl_service_brush_t *)malloc(stObjMeta2.size * sizeof(cvtdl_service_brush_t));
    for (uint32_t oid = 0; oid < stObjMeta2.size; oid++) {
      if((s_PersonState[stTrackerMeta2.info[oid].id] & 0b00001101) == 0b00001101) { 
        brushes[oid] = brush_red;
        snprintf(stObjMeta2.info[oid].name, sizeof(stObjMeta2.info[oid].name), "No Vest and Safe Hat");
      }      
      else if((s_PersonState[stTrackerMeta2.info[oid].id] & 0b00001001) == 0b00001001) {
        brushes[oid] = brush_yellow;
        snprintf(stObjMeta2.info[oid].name, sizeof(stObjMeta2.info[oid].name), "No Vest");
      }    
      else if((s_PersonState[stTrackerMeta2.info[oid].id] & 0b00000101) == 0b00000101) {
        brushes[oid] = brush_pink;
        snprintf(stObjMeta2.info[oid].name, sizeof(stObjMeta2.info[oid].name), "No Safe Hat");
      }
      else if((s_PersonState[stTrackerMeta2.info[oid].id] & 0b00000001) == 0b00000001) {
        brushes[oid] = brush_green;
        snprintf(stObjMeta2.info[oid].name, sizeof(stObjMeta2.info[oid].name), "Safe");
      }
      else{
        brushes[oid] = brush_blue;
        snprintf(stObjMeta2.info[oid].name, sizeof(stObjMeta2.info[oid].name), "New");
      }
    }

    s32Ret = CVI_TDL_Service_ObjectDrawRect2(pstArgs->stServiceHandle, &stObjMeta2, &stFrame, true, brushes);
    if (s32Ret != CVI_TDL_SUCCESS) {
      CVI_VPSS_ReleaseChnFrame(0, 0, &stFrame);
      printf("Draw frame fail!, ret=%x\n", s32Ret);
      bExit = true;
      free(brushes);
    }

    s32Ret = SAMPLE_TDL_Send_Frame_RTSP(&stFrame, pstArgs->pstMWContext);
    if (s32Ret != CVI_SUCCESS) {
      CVI_VPSS_ReleaseChnFrame(0, 0, &stFrame);
      printf("Send Output Frame NG, ret=%x\n", s32Ret);
      bExit = true;
      free(brushes);
    }
    // CVI_TDL_Free(&obj_data2);
    CVI_VPSS_ReleaseChnFrame(0, 0, &stFrame);
    if (s32Ret != CVI_SUCCESS) {
      bExit = true;
      free(brushes);
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
  cvtdl_tracker_t stTrackerMeta = {0};
  cvtdl_object_t stTrackObjMeta = {0};
  cvtdl_object_t stTrackObjMeta2 = {0};
  uint32_t s_Personcount = 0;
  uint8_t s_PersonTimeOut[256] = {0};
  uint8_t s_PersonState[256] = {0};
  int sem;

  while (bExit == false) {
    if(CVI_VPSS_GetChnFrame(0, VPSS_CHN1, &fdFrame, 2000) != CVI_SUCCESS)
    {
        CVI_TDL_Free(&stObjMeta);
        pthread_exit(NULL);
    }
    struct timeval yolot0, yolot1;
    struct timeval deepsortt0, deepsortt1;

/*****************************YOLO stage*****************************/    
    gettimeofday(&yolot0, NULL);
    if(CVI_TDL_YOLOV8_Detection(tdl_handle, &fdFrame, &stObjMeta) != CVI_SUCCESS){
      printf("YOLOV8 failed!\n");
      CVI_VPSS_ReleaseChnFrame(0, 1, &fdFrame);
      CVI_TDL_Free(&stObjMeta);
      bExit = true;
    }
    gettimeofday(&yolot1, NULL);
/*****************************DeepSORT stage*****************************/
    gettimeofday(&deepsortt0, NULL);
    CVI_TDL_CopyObjectMeta(&stObjMeta, &stTrackObjMeta);
    
    int j = 0;
    int k = 0;
    for (uint32_t i = 0; i < stObjMeta.size; i++){
      if(stObjMeta.info[i].classes == 5) j++;
    }
    if(j != 0){  
      CVI_TDL_CopyObjectMeta(&stObjMeta,&stTrackObjMeta);
      for (uint32_t i = 0; i < stObjMeta.size; i++){
        if(stTrackObjMeta.info[i].classes == 5){ 
          My_CopyObjectInfo(&stObjMeta.info[i],&stTrackObjMeta.info[k]);
          k++;
        } 
        stTrackObjMeta.size = k;  
      }
      CVI_TDL_CopyObjectMeta(&stTrackObjMeta, &stTrackObjMeta2);
    }
    else CVI_TDL_Free(&stTrackObjMeta2);
    if (CVI_TDL_OSNet(tdl_handle, &fdFrame, &stTrackObjMeta2) != CVI_TDL_SUCCESS) {
      printf("DeepSORT failed!\n");
      CVI_VPSS_ReleaseChnFrame(0, 1, &fdFrame);
      CVI_TDL_Free(&stTrackObjMeta2);
      bExit = true;
    }
    if (CVI_TDL_DeepSORT_Obj(tdl_handle, &stTrackObjMeta2, &stTrackerMeta, true) != CVI_TDL_SUCCESS) {
      printf("DeepSORT failed!\n");
      CVI_VPSS_ReleaseChnFrame(0, 1, &fdFrame);
      CVI_TDL_Free(&stTrackObjMeta);
      CVI_TDL_Free(&stTrackObjMeta2);
      CVI_TDL_Free(&stObjMeta);
      CVI_TDL_Free(&stTrackerMeta);
      bExit = true;
    }      
    gettimeofday(&deepsortt1, NULL);
/***************************** END *****************************/
    unsigned long yolo_execution_time = ((yolot1.tv_sec - yolot0.tv_sec) * 1000000 + yolot1.tv_usec - yolot0.tv_usec)/1000;
    unsigned long deepsort_execution_time = ((deepsortt1.tv_sec - deepsortt0.tv_sec) * 1000000 + deepsortt1.tv_usec - deepsortt0.tv_usec)/1000;
    printf("obj count: %d, exec time=%lu ms\n",stObjMeta.size, yolo_execution_time);
    printf("obj count: %d, exec time=%lu ms\n",stTrackerMeta.size, deepsort_execution_time);
    if(stObjMeta.size != 0)
    {
      printf("------------------yolo info-------------------\n");
      for (uint32_t i = 0; i < stObjMeta.size; i++){
        strlcpy(stObjMeta.info[i].name,class_name[stObjMeta.info[i].classes],16);
        printf("detect res: %4.1f %4.1f %4.1f %4.1f %1.3f %s\n", stObjMeta.info[i].bbox.x1, stObjMeta.info[i].bbox.y1,
            stObjMeta.info[i].bbox.x2, stObjMeta.info[i].bbox.y2, stObjMeta.info[i].bbox.score,
            class_name[stObjMeta.info[i].classes]);
      }
      sem_getvalue(&NetSemphore,&sem);
      if(sem == 0){
        sem_post(&NetSemphore);
      }   
    }
    if(stTrackObjMeta2.size != 0)
    {
      printf("------------------Track obj info-------------------\n");
      for (uint32_t i = 0; i < stTrackObjMeta2.size; i++){
        strlcpy(stTrackObjMeta2.info[i].name,class_name[stTrackObjMeta2.info[i].classes],16);
        printf("detect res: %4.1f %4.1f %4.1f %4.1f %1.3f %s\n", stTrackObjMeta2.info[i].bbox.x1, stTrackObjMeta2.info[i].bbox.y1,
            stTrackObjMeta2.info[i].bbox.x2, stTrackObjMeta2.info[i].bbox.y2, stTrackObjMeta2.info[i].bbox.score,
            class_name[stTrackObjMeta2.info[i].classes]);
      }
    }
    if(stTrackerMeta.size != 0)
    {
      printf("------------------DeepSORT info-------------------\n");
      for (uint32_t i = 0; i < stTrackerMeta.size; i++)
      {
        printf("track res: ");
        printf("%4.1f %4.1f %4.1f %1.3f",stTrackerMeta.info[i].bbox.x1 ,stTrackerMeta.info[i].bbox.y1 ,
                                          stTrackerMeta.info[i].bbox.x2 ,stTrackerMeta.info[i].bbox.y2);
        if(stTrackerMeta.info[i].state == CVI_TRACKER_NEW) printf(" NEW ");
        else if(stTrackerMeta.info[i].state == CVI_TRACKER_UNSTABLE) printf(" UNSTABLE ");
        else if(stTrackerMeta.info[i].state == CVI_TRACKER_STABLE) printf(" STABLE ");
        printf(" %ld %d\n",stTrackerMeta.info[i].id,stTrackerMeta.info[i].out_num);
      }
    }
    if(stTrackerMeta.size != 0)
    {
      printf("------------------Person Count %4d-------------------\n",s_Personcount);
      for (uint32_t i = 0; i < stTrackerMeta.size; i++){
        stTrackObjMeta2.info[i].unique_id = stTrackerMeta.info[i].id;
        if (stTrackerMeta.info[i].state == CVI_TRACKER_STABLE){
          if(s_PersonState[stTrackerMeta.info[i].id] == 0b00010000 && s_PersonTimeOut[stTrackerMeta.info[i].id] == 0x00){
            s_PersonState[stTrackerMeta.info[i].id] = 0b00000001;
            s_PersonTimeOut[stTrackerMeta.info[i].id] = 100;
            s_Personcount++;
          }
          else if(s_PersonState[stTrackerMeta.info[i].id] == 0b00000000){
            s_PersonState[stTrackerMeta.info[i].id] = 0b00010000;
            s_PersonTimeOut[stTrackerMeta.info[i].id] = 10;
          }        
          if((s_PersonState[stTrackerMeta.info[i].id] & 0b00000001) == 0b00000001){
            s_PersonTimeOut[stTrackerMeta.info[i].id] = 100;
            s_PersonState[stTrackerMeta.info[i].id] |= 0b00000100;
            for(uint32_t ii = 0; ii < stObjMeta.size; ii++){
              if(stObjMeta.info[ii].classes == 0){
                if(utilis_wear_safe_hat(stObjMeta.info[ii],stTrackerMeta.info[i]) == true){
                  s_PersonState[stTrackerMeta.info->id] &= ~0b00000100;
                }
              }
              else if(stObjMeta.info[ii].classes == 2){
                if(utilis_wear_safe_hat(stObjMeta.info[ii],stTrackerMeta.info[i]) == true){
                  break;
                }
              }
            }
            s_PersonState[stTrackerMeta.info[i].id] |= 0b00001000;
            for(uint32_t ii = 0; ii < stObjMeta.size; ii++){
              if(stObjMeta.info[ii].classes == 7){
                if(utilis_wear_safe_vest(stObjMeta.info[ii],stTrackerMeta.info[i]) == true){
                  s_PersonState[stTrackerMeta.info[i].id] &= ~0b00001000;
                }
              }
              else if(stObjMeta.info[ii].classes == 4){
                if(utilis_wear_safe_vest(stObjMeta.info[ii],stTrackerMeta.info[i]) == true){
                  break;
                }
              }
            }
          }
        } 
      }
      for (uint32_t i = 0; i < 256; i++){
        if(s_PersonState[i] != 0x00){
          printf("ID:%3d Remain %3dTicks ",i,s_PersonTimeOut[i]);
          if(s_PersonState[i] & 0b00000100) printf("No Safe Hat  ");
          if(s_PersonState[i] & 0b00001000) printf("No Vest  ");
          if(s_PersonState[i] & 0b00010000) printf("New");
          else if(!(s_PersonState[i] & 0b00001100)) printf("SAFE");
          
          printf("\n");
        }
        if(s_PersonTimeOut[i] == 0x00){
          s_PersonState[i] = 0x00;
        }
        else{
          s_PersonTimeOut[i]--;
        }
      }
    }
    /*
    bit 0 ID是否有效
    bit 1 ID是否拍照
    bit 2 ID是否佩戴头盔
    bit 3 ID是否穿戴反光衣
    bit 4 
    */

   /*
  0: Hardhat
  1: Mask
  2: NO-Hardhat
  3: NO-Mask
  4: NO-Safety Vest 
  5: Person
  6: Safety Cone
  7: Safety Vest
  8: machinery 
  9: vehicle
*/


    pthread_mutex_lock(&ResultMutex);
    CVI_TDL_CopyObjectMeta(&stObjMeta, &g_obj_data);
    CVI_TDL_CopyObjectMeta(&stTrackObjMeta2, &g_obj_data2);
    CVI_TDL_CopyTrackerMeta(&stTrackerMeta, &g_stTrackerMeta);
    pthread_mutex_unlock(&ResultMutex);
    memcpy(g_PersonState,s_PersonState,256);
    g_Personcount = s_Personcount;
    CVI_VPSS_ReleaseChnFrame(0, 1, &fdFrame);
    CVI_TDL_Free(&stObjMeta);
    CVI_TDL_Free(&stTrackObjMeta);
    CVI_TDL_Free(&stTrackObjMeta2);
    CVI_TDL_Free(&stTrackerMeta);
    printf("------------------ END -------------------\n\n\n");
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
  if (argc != 3) {
    printf(
        "\nUsage: %s YOLOV8_PATH OSNET_PATH\n\n"
        "\tYOLOV8_PATH\n"
        "\tOSNET_PATH\n",
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

  printf("---------------------open yolo model-----------------------");
  init_param(stTDLHandle);
  int ret2 = (int)CVI_TDL_OpenModel(stTDLHandle, CVI_TDL_SUPPORTED_MODEL_YOLOV8_DETECTION, argv[1]);
  if(ret2 !=CVI_SUCCESS)
  {
    printf("openmodel failed ret=%X\n",ret2);
    CVI_TDL_Service_DestroyHandle(stServiceHandle);
    CVI_TDL_DestroyHandle(stTDLHandle);
    SAMPLE_TDL_Destroy_MW(&stMWContext);    
  }
  printf("---------------------setup deepsort-----------------------\n");
  ret2 = (int)CVI_TDL_OpenModel(stTDLHandle, CVI_TDL_SUPPORTED_MODEL_OSNET, argv[2]);
  if(ret2 !=CVI_SUCCESS)
  {
    printf("openmodel failed ret=%X\n",ret2);
    CVI_TDL_Service_DestroyHandle(stServiceHandle);
    CVI_TDL_DestroyHandle(stTDLHandle);
    SAMPLE_TDL_Destroy_MW(&stMWContext);    
  }
  CVI_TDL_DeepSORT_Init(stTDLHandle, true);
  cvtdl_deepsort_config_t ds_conf;
  CVI_TDL_DeepSORT_GetDefaultConfig(&ds_conf);
  set_sample_mot_config(&ds_conf);
  ret2 = (int)CVI_TDL_DeepSORT_SetConfig(stTDLHandle, &ds_conf, -1, false);
  if(ret2 !=CVI_SUCCESS){
    printf("DeepSORT config failed ret=%X\n",ret2);
  }
  else printf("DeepSORT config Success\n");
  printf("---------------------all finish-----------------------\n"); 

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
  float len_x = obj.bbox.x2 - obj.bbox.x1;
  if((mid_x >= obj.bbox.x1) && (mid_x <= obj.bbox.x2) && (mid_y >= obj.bbox.y1) && (mid_y <= obj.bbox.y2)){
    if((mid_x >= (mid_obj_x - (len_x * 0.2)))&&(mid_x <= (mid_obj_x + (len_x * 0.2))))
      return true;
    else
      return false;
  }
  else
    return false;
}
bool utilis_wear_safe_vest(cvtdl_object_info_t tar,cvtdl_tracker_info_t obj)
{
  float mid_x = (tar.bbox.x1 + tar.bbox.x2)/2;
  float mid_y = (tar.bbox.y1 + tar.bbox.y2)/2;
  if((mid_x >= obj.bbox.x1) && (mid_x <= obj.bbox.x2) && (mid_y >= obj.bbox.y1) && (mid_y <= obj.bbox.y2)){
    if((tar.bbox.y2 >= obj.bbox.y2*0.8)&&(tar.bbox.y2 <= obj.bbox.y2*1.2))
      return true;
    else
      return false;
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
