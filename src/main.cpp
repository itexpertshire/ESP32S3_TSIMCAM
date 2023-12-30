/*
* Capture ESP32 Cam JPEG images into a AVI file and store on SD
* AVI files stored on the SD card can also be selected and streamed to a browser as MJPEG.
*
* s60sc 2020 - 2023
https://github.com/s60sc/ESP32-CAM_MJPEG2SD

https://github.com/itexpertshire/ESP32S3_TSIMCAM
*/

#include "appGlobals.h"

char camModel[10];

static void prepNightCam() {
  // initialise camera depending on model and board for Night Photography
  // configure camera
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = xclkMhz * 1000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  // init with high specs to pre-allocate larger buffers
  config.fb_location = CAMERA_FB_IN_PSRAM;
#if CONFIG_IDF_TARGET_ESP32S3
  config.frame_size = FRAMESIZE_QSXGA; // 8M
#else
  config.frame_size = FRAMESIZE_UXGA;  // 4M
#endif  
  config.jpeg_quality = 10;
  config.fb_count = FB_BUFFERS + 1; // +1 needed

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

  // camera init
  if (psramFound()) {
    esp_err_t err = ESP_FAIL;
    uint8_t retries = 2;
    while (retries && err != ESP_OK) {
      err = esp_camera_init(&config);
      if (err != ESP_OK) {
        // power cycle the camera, provided pin is connected
        digitalWrite(PWDN_GPIO_NUM, 1);
        delay(100);
        digitalWrite(PWDN_GPIO_NUM, 0); 
        delay(100);
        retries--;
      }
    } 
    if (err != ESP_OK) snprintf(startupFailure, SF_LEN, "Startup Failure: Camera init error 0x%x", err);
    else {
      sensor_t * s = esp_camera_sensor_get();
      switch (s->id.PID) {
        case (OV2640_PID):
          strcpy(camModel, "OV2640");
        break;
        case (OV3660_PID):
          strcpy(camModel, "OV3660");
        break;
        case (OV5640_PID):
          strcpy(camModel, "OV5640");
        break;
        default:
          strcpy(camModel, "Other");
        break;
      }
      LOG_INF("Camera init OK for model %s on board %s", camModel, CAM_BOARD);

      // model specific corrections
      if (s->id.PID == OV3660_PID) {
        // initial sensors are flipped vertically and colors are a bit saturated
        s->set_vflip(s, 1);//flip it back
        s->set_brightness(s, 1);//up the brightness just a bit
        s->set_saturation(s, -2);//lower the saturation
      }
      // set frame size to configured value
      char fsizePtr[4];
      if (retrieveConfigVal("framesize", fsizePtr)) s->set_framesize(s, (framesize_t)(atoi(fsizePtr)));
      else s->set_framesize(s, FRAMESIZE_SVGA);
  
#if defined(CAMERA_MODEL_M5STACK_WIDE)
      s->set_vflip(s, 1);
      s->set_hmirror(s, 1);
#endif
  
#if defined(CAMERA_MODEL_M5STACK_WIDE) || defined(CAMERA_MODEL_M5STACK_ESP32CAM)
    s->set_vflip(s, 1);
    s->set_hmirror(s, 1);
#endif
  
#if defined(CAMERA_MODEL_ESP32S3_EYE)
    s->set_vflip(s, 1);
#endif

   if (s->id.PID == OV2640_PID){
        int day_switch_value=nightSwitch;
        //Night Phototgraphy Camera Exposure Control
        s->set_whitebal(s, 1);       // 0 = disable , 1 = enable
        s->set_awb_gain(s, 1);       // 0 = disable , 1 = enable
        s->set_wb_mode(s, 2);        // 0 to 4 - if awb_gain enabled (0 - Auto, 1 - Sunny, 2 - Cloudy, 3 - Office, 4 - Home)
        //s->set_exposure_ctrl(s, 1);  // 0 = disable , 1 = enable
      // s->set_aec2(s, 0);           // 0 = disable , 1 = enable
        //s->set_ae_level(s, 2);       // -2 to 2
        //s->set_aec_value(s, 1200);    // 0 to 1200
        s->set_gain_ctrl(s, 0);      // 0 = disable , 1 = enable
        s->set_agc_gain(s, 0);       // 0 to 30
        s->set_gainceiling(s, (gainceiling_t)6);  // 0 to 6
        s->set_bpc(s, 1);            // 0 = disable , 1 = enable
        s->set_wpc(s, 1);            // 0 = disable , 1 = enable
        s->set_raw_gma(s, 1);        // 0 = disable , 1 = enable
        s->set_lenc(s, 0);           // 0 = disable , 1 = enable
        s->set_hmirror(s, 0);        // 0 = disable , 1 = enable
        s->set_vflip(s, 0);          // 0 = disable , 1 = enable
        s->set_dcw(s, 0);            // 0 = disable , 1 = enable
        s->set_colorbar(s, 0);       // 0 = disable , 1 = enable  
        s->set_reg(s,0xff,0xff,0x01);//banksel   
        int light=s->get_reg(s,0x2f,0xff);
        LOG_INF("First light is %d",light);

        if(light<day_switch_value)
    {
      //here we are in night mode
      if(light<day_switch_value)s->set_reg(s,0x11,0xff,1);//frame rate (1 means longer exposure)
      s->set_reg(s,0x13,0xff,0);//manual everything
      s->set_reg(s,0x0c,0x6,0x8);//manual banding
           
      s->set_reg(s,0x45,0x3f,0x3f);//really long exposure (but it doesn't really work)

         
        if(light==0)
          {
            s->set_reg(s,0x47,0xff,0x40);//Frame Length Adjustment MSBs
            s->set_reg(s,0x2a,0xf0,0xf0);//line adjust MSB
            s->set_reg(s,0x2b,0xff,0xff);//line adjust
          }
          else if(light==1)
          {
            s->set_reg(s,0x47,0xff,0x40);//Frame Length Adjustment MSBs
            s->set_reg(s,0x2a,0xf0,0xd0);//line adjust MSB
            s->set_reg(s,0x2b,0xff,0xff);//line adjust
          }
          else if(light==2)
          {
            s->set_reg(s,0x47,0xff,0x40);//Frame Length Adjustment MSBs
            s->set_reg(s,0x2a,0xf0,0xb0);//line adjust MSB
            s->set_reg(s,0x2b,0xff,0xff);//line adjust  
          }
          else if(light==3)
          {
            s->set_reg(s,0x47,0xff,0x40);//Frame Length Adjustment MSBs
            s->set_reg(s,0x2a,0xf0,0x70);//line adjust MSB
            s->set_reg(s,0x2b,0xff,0xff);//line adjust      
          }    
          else if(light==4)
          {
            s->set_reg(s,0x47,0xff,0x40);//Frame Length Adjustment MSBs
            s->set_reg(s,0x2a,0xf0,0x40);//line adjust MSB
            s->set_reg(s,0x2b,0xff,0xff);//line adjust
          }
          else if(light==5)
          {
            s->set_reg(s,0x47,0xff,0x20);//Frame Length Adjustment MSBs
            s->set_reg(s,0x2a,0xf0,0x80);//line adjust MSB
            s->set_reg(s,0x2b,0xff,0xff);//line adjust        
          }
          else if(light==6)
          {
            s->set_reg(s,0x47,0xff,0x20);//Frame Length Adjustment MSBs
            s->set_reg(s,0x2a,0xf0,0x40);//line adjust MSB
            s->set_reg(s,0x2b,0xff,0xff);//line adjust        
          }         
          else if(light==7)
          {
            s->set_reg(s,0x47,0xff,0x20);//Frame Length Adjustment MSBs
            s->set_reg(s,0x2a,0xf0,0x30);//line adjust MSB
            s->set_reg(s,0x2b,0xff,0xff);//line adjust        
          }
          else if(light==8)
          {
            s->set_reg(s,0x47,0xff,0x20);//Frame Length Adjustment MSBs
            s->set_reg(s,0x2a,0xf0,0x20);//line adjust MSB
            s->set_reg(s,0x2b,0xff,0xff);//line adjust        
          }     
          else if(light==9)
          {
            s->set_reg(s,0x47,0xff,0x20);//Frame Length Adjustment MSBs
            s->set_reg(s,0x2a,0xf0,0x10);//line adjust MSB
            s->set_reg(s,0x2b,0xff,0xff);//line adjust        
          }    
          else if(light==10)
          {
            s->set_reg(s,0x47,0xff,0x10);//Frame Length Adjustment MSBs
            s->set_reg(s,0x2a,0xf0,0x70);//line adjust MSB
            s->set_reg(s,0x2b,0xff,0xff);//line adjust        
          }
          else if(light<=12)
          {
            s->set_reg(s,0x47,0xff,0x10);//Frame Length Adjustment MSBs
            s->set_reg(s,0x2a,0xf0,0x60);//line adjust MSB
            s->set_reg(s,0x2b,0xff,0xff);//line adjust        
          }
          else if(light<=14)
          {
            s->set_reg(s,0x47,0xff,0x10);//Frame Length Adjustment MSBs
            s->set_reg(s,0x2a,0xf0,0x40);//line adjust MSB
            s->set_reg(s,0x2b,0xff,0xff);//line adjust        
          }          
          else if(light<=18)
          {
            s->set_reg(s,0x47,0xff,0x08);//Frame Length Adjustment MSBs
            s->set_reg(s,0x2a,0xf0,0xb0);//line adjust MSB
            s->set_reg(s,0x2b,0xff,0xff);//line adjust        
          }
          else if(light<=20)
          {
            s->set_reg(s,0x47,0xff,0x08);//Frame Length Adjustment MSBs
            s->set_reg(s,0x2a,0xf0,0x80);//line adjust MSB
            s->set_reg(s,0x2b,0xff,0xff);//line adjust        
          }      
          else if(light<=23)
          {
            s->set_reg(s,0x47,0xff,0x08);//Frame Length Adjustment MSBs
            s->set_reg(s,0x2a,0xf0,0x60);//line adjust MSB
            s->set_reg(s,0x2b,0xff,0xff);//line adjust        
          }           
          else if(light<=27)
          {
            s->set_reg(s,0x47,0xff,0x04);//Frame Length Adjustment MSBs
            s->set_reg(s,0x2a,0xf0,0xd0);//line adjust MSB
            s->set_reg(s,0x2b,0xff,0xff);//line adjust        
          }
          else if(light<=31)
          {
            s->set_reg(s,0x47,0xff,0x04);//Frame Length Adjustment MSBs
            s->set_reg(s,0x2a,0xf0,0x80);//line adjust MSB
            s->set_reg(s,0x2b,0xff,0xff);//line adjust        
          }     
          else if(light<=35)
          {
            s->set_reg(s,0x47,0xff,0x04);//Frame Length Adjustment MSBs
            s->set_reg(s,0x2a,0xf0,0x60);//line adjust MSB
            s->set_reg(s,0x2b,0xff,0xff);//line adjust        
          }    
          else if(light<=40)
          {
            s->set_reg(s,0x47,0xff,0x02);//Frame Length Adjustment MSBs
            s->set_reg(s,0x2a,0xf0,0x70);//line adjust MSB
            s->set_reg(s,0x2b,0xff,0xff);//line adjust        
          }
          else if(light<45)
          {
            s->set_reg(s,0x47,0xff,0x02);//Frame Length Adjustment MSBs
            s->set_reg(s,0x2a,0xf0,0x40);//line adjust MSB
            s->set_reg(s,0x2b,0xff,0xff);//line adjust        
          }
          //after this the frame rate is higher, so we need to compensate
          else if(light<50)
          {
            s->set_reg(s,0x47,0xff,0x04);//Frame Length Adjustment MSBs
            s->set_reg(s,0x2a,0xf0,0xa0);//line adjust MSB
            s->set_reg(s,0x2b,0xff,0xff);//line adjust        
          }  
          if(light<day_switch_value)s->set_reg(s,0x43,0xff,0x40);//magic value to give us the frame faster (bit 6 must be 1)
         
            
         }
   }
   else LOG_ERR("This Camera model %s not supported for night photography",camModel);


    }

  }
  debugMemory("prepNightCam");

  
}

static void prepCam() {
  // initialise camera depending on model and board
  // configure camera
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = xclkMhz * 1000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_LATEST;
  // init with high specs to pre-allocate larger buffers
  config.fb_location = CAMERA_FB_IN_PSRAM;
#if CONFIG_IDF_TARGET_ESP32S3
  config.frame_size = FRAMESIZE_QSXGA; // 8M
#else
  config.frame_size = FRAMESIZE_UXGA;  // 4M
#endif  
  config.jpeg_quality = 10;
  config.fb_count = FB_BUFFERS + 1; // +1 needed

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

  // camera init
  if (psramFound()) {
    esp_err_t err = ESP_FAIL;
    uint8_t retries = 2;
    while (retries && err != ESP_OK) {
      err = esp_camera_init(&config);
      if (err != ESP_OK) {
        // power cycle the camera, provided pin is connected
        digitalWrite(PWDN_GPIO_NUM, 1);
        delay(100);
        digitalWrite(PWDN_GPIO_NUM, 0); 
        delay(100);
        retries--;
      }
    } 
    if (err != ESP_OK) snprintf(startupFailure, SF_LEN, "Startup Failure: Camera init error 0x%x", err);
    else {
      sensor_t * s = esp_camera_sensor_get();
      switch (s->id.PID) {
        case (OV2640_PID):
          strcpy(camModel, "OV2640");
        break;
        case (OV3660_PID):
          strcpy(camModel, "OV3660");
        break;
        case (OV5640_PID):
          strcpy(camModel, "OV5640");
        break;
        default:
          strcpy(camModel, "Other");
        break;
      }
      LOG_INF("Camera init OK for model %s on board %s", camModel, CAM_BOARD);

      // model specific corrections
      if (s->id.PID == OV3660_PID) {
        // initial sensors are flipped vertically and colors are a bit saturated
        s->set_vflip(s, 1);//flip it back
        s->set_brightness(s, 1);//up the brightness just a bit
        s->set_saturation(s, -2);//lower the saturation
      }
      // set frame size to configured value
      char fsizePtr[4];
      if (retrieveConfigVal("framesize", fsizePtr)) s->set_framesize(s, (framesize_t)(atoi(fsizePtr)));
      else s->set_framesize(s, FRAMESIZE_SVGA);
  
#if defined(CAMERA_MODEL_M5STACK_WIDE)
      s->set_vflip(s, 1);
      s->set_hmirror(s, 1);
#endif
  
#if defined(CAMERA_MODEL_M5STACK_WIDE) || defined(CAMERA_MODEL_M5STACK_ESP32CAM)
    s->set_vflip(s, 1);
    s->set_hmirror(s, 1);
#endif
  
#if defined(CAMERA_MODEL_ESP32S3_EYE)
    s->set_vflip(s, 1);
#endif
    }
  }
  debugMemory("prepCam");
}

void setup() {   
  logSetup();
  if (!psramFound()) snprintf(startupFailure, SF_LEN, "Startup Failure: Need PSRAM to be enabled");
  else {
    // prep SD card storage
    startStorage(); 
    
    // Load saved user configuration
    loadConfig();
  
    // initialise camera
    if (nightphotoOn) {
                       prepNightCam();
                         // connect wifi or start config AP if router details not available
                       prepRecording();
                       startWifi();
                       startWebServer();
                       return;
    }
    else prepCam();
  }
  
#ifdef DEV_ONLY
  devSetup();
#endif

  // connect wifi or start config AP if router details not available
  startWifi();

  startWebServer();
  if (strlen(startupFailure)) LOG_ERR("%s", startupFailure);
  else {
    // start rest of services
    startStreamServer();
    prepSMTP(); 
    prepPeripherals();
    prepMic(); 
    prepRecording();
    prepTelemetry();
    LOG_INF("Camera model %s on board %s ready @ %uMHz", camModel, CAM_BOARD, xclkMhz); 
    checkMemory();
    LOG_INF("************************************\n");
    //syncDataFiles();
  } 
}

void loop() {
  vTaskDelete(NULL); // free 8k ram
}
