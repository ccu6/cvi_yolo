#define LOG_TAG "SampleFD"
#define LOG_LEVEL LOG_LEVEL_INFO

#include "middleware_utils.h"
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
#include <string.h>
#include <sys/time.h>
#include <unistd.h>






// uint8_t class_name[80][16] = {
//     '__background__', 'person', 'bicycle', 'car', 'motorcycle', 'airplane',
//     'bus', 'train', 'truck', 'boat', 'traffic light', 'fire hydrant',
//     'stop sign', 'parking meter', 'bench', 'bird', 'cat', 'dog', 'horse',
//     'sheep', 'cow', 'elephant', 'bear', 'zebra', 'giraffe', 'backpack',
//     'umbrella', 'handbag', 'tie', 'suitcase', 'frisbee', 'skis',
//     'snowboard', 'sports ball', 'kite', 'baseball bat', 'baseball glove',
//     'skateboard', 'surfboard', 'tennis racket', 'bottle', 'wine glass',
//     'cup', 'fork', 'knife', 'spoon', 'bowl', 'banana', 'apple', 'sandwich',
//     'orange', 'broccoli', 'carrot', 'hot dog', 'pizza', 'donut', 'cake',
//     'chair', 'couch', 'potted plant', 'bed', 'dining table', 'toilet', 'tv',
//     'laptop', 'mouse', 'remote', 'keyboard', 'cell phone', 'microwave',
//     'oven', 'toaster', 'sink', 'refrigerator', 'book', 'clock', 'vase',
//     'scissors', 'teddy bear', 'hair drier', 'toothbrush'
// };


static volatile bool bExit = false;

static cvtdl_object_t obj_data = {0};
static cvtdl_object_t obj_data2 = {0};

MUTEXAUTOLOCK_INIT(ResultMutex);

typedef struct {
  SAMPLE_TDL_MW_CONTEXT *pstMWContext;
  cvitdl_service_handle_t stServiceHandle;
} SAMPLE_TDL_VENC_THREAD_ARG_S;


void *run_venc(void *args) {
  printf("Enter encoder thread\n");
  SAMPLE_TDL_VENC_THREAD_ARG_S *pstArgs = (SAMPLE_TDL_VENC_THREAD_ARG_S *)args;
  VIDEO_FRAME_INFO_S stFrame;
  CVI_S32 s32Ret;
  // cvtdl_face_t stFaceMeta = {0};

  while (bExit == false) {
    s32Ret = CVI_VPSS_GetChnFrame(0, 0, &stFrame, 2000);
    if (s32Ret != CVI_SUCCESS) {
      printf("CVI_VPSS_GetChnFrame chn0 failed with %#x\n", s32Ret);
      break;
    }

    {
      MutexAutoLock(ResultMutex, lock);
    }

    s32Ret = CVI_TDL_Service_ObjectDrawRect(pstArgs->stServiceHandle, &obj_data2, &stFrame, true,
                                          CVI_TDL_Service_GetDefaultBrush());
    if (s32Ret != CVI_TDL_SUCCESS) {
      CVI_VPSS_ReleaseChnFrame(0, 0, &stFrame);
      printf("Draw fame fail!, ret=%x\n", s32Ret);
      goto error;
    }
    s32Ret = SAMPLE_TDL_Send_Frame_RTSP(&stFrame, pstArgs->pstMWContext);
    if (s32Ret != CVI_SUCCESS) {
      CVI_VPSS_ReleaseChnFrame(0, 0, &stFrame);
      printf("Send Output Frame NG, ret=%x\n", s32Ret);
      goto error;
    }

  error:
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

  CVI_TDL_SetModelThreshold(tdl_handle, CVI_TDL_SUPPORTED_MODEL_YOLOV5, 0.5);
  CVI_TDL_SetModelNmsThreshold(tdl_handle, CVI_TDL_SUPPORTED_MODEL_YOLOV5, 0.5);


  while (bExit == false) {
    if(CVI_VPSS_GetChnFrame(0, VPSS_CHN1, &fdFrame, 2000) != CVI_SUCCESS)
    {
        CVI_TDL_Free(&obj_data);
        pthread_exit(NULL);
    }
    struct timeval t0, t1;

    // Predict face quality score.
    gettimeofday(&t0, NULL);
    if(CVI_TDL_Yolov5(tdl_handle, &fdFrame, &obj_data) != CVI_SUCCESS)
    {
        CVI_VPSS_ReleaseChnFrame(0, 1, &fdFrame);
        CVI_TDL_Free(&obj_data);
        bExit = true;
        pthread_exit(NULL);
    }
    gettimeofday(&t1, NULL);
    unsigned long execution_time = ((t1.tv_sec - t0.tv_sec) * 1000000 + t1.tv_usec - t0.tv_usec);

    printf("obj count: %d, exec time=%lu us\n", obj_data.size, execution_time);
    for (uint32_t i = 0; i < obj_data.size; i++) {
    printf("detect res: %f %f %f %f %f %d\n", obj_data.info[i].bbox.x1, obj_data.info[i].bbox.y1,
           obj_data.info[i].bbox.x2, obj_data.info[i].bbox.y2, obj_data.info[i].bbox.score,
           obj_data.info[i].classes);
  }
  CVI_TDL_Free(&obj_data2);

    {
      MutexAutoLock(ResultMutex, lock);
      CVI_TDL_CopyObjectMeta(&obj_data,&obj_data2);
    }

    CVI_VPSS_ReleaseChnFrame(0, 1, &fdFrame);
    CVI_TDL_Free(&obj_data);
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

  pthread_join(stVencThread, NULL);
  pthread_join(stTDLThread, NULL);

  CVI_TDL_Service_DestroyHandle(stServiceHandle);
  CVI_TDL_DestroyHandle(stTDLHandle);
  SAMPLE_TDL_Destroy_MW(&stMWContext);

  return 0;
}
