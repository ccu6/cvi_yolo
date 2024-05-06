        if (stTrackerMeta.info[i].state == CVI_TRACKER_STABLE){
          if(s_PersonState[(uint8_t)stTrackerMeta.info->id] == 0x00){
            s_PersonState[(uint8_t)stTrackerMeta.info->id] |= 0b00000001;
            s_PersonTimeOut[(uint8_t)stTrackerMeta.info->id] = 100;
            s_Personcount++;
          }
          s_PersonState[(uint8_t)stTrackerMeta.info->id] |= 0b00000100;
          for(uint32_t ii = 0; ii < stObjMeta.size; ii++){
            if(stObjMeta.info[ii].classes == 0){
              if(utilis_is_in(stObjMeta.info[ii],stTrackerMeta.info[i]) == true){
                s_PersonState[(uint8_t)stTrackerMeta.info->id] &= ~0b00000100;
              }
            }
            else if(stObjMeta.info[ii].classes == 2){
              if(utilis_is_in(stObjMeta.info[ii],stTrackerMeta.info[i]) == true){
                break;
              }
            }
          }
          s_PersonState[(uint8_t)stTrackerMeta.info->id] |= 0b00001000;
          for(uint32_t ii = 0; ii < stObjMeta.size; ii++){
            if(stObjMeta.info[ii].classes == 7){
              if(utilis_is_in(stObjMeta.info[ii],stTrackerMeta.info[i]) == true){
                s_PersonState[(uint8_t)stTrackerMeta.info->id] &= ~0b00001000;
              }
            }
            else if(stObjMeta.info[ii].classes == 4){
              if(utilis_is_in(stObjMeta.info[ii],stTrackerMeta.info[i]) == true){
                break;
              }
            }
          }
        } 