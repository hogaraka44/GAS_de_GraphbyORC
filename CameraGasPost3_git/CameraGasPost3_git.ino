#include "esp_camera.h" //これは動く
#include "FS.h"
#include "SD.h"
#include "SPI.h"

#define CAMERA_MODEL_XIAO_ESP32S3 // Has PSRAM
#include "camera_pins.h"//上のdefineに対応したpinを与える。

#include <WiFi.h>  //postを送るだけのやつ
#include "HTTPSRedirect.h"
#include <HTTPClient.h>//多分いらない
int waitingTime =6000;

const char *ssid1 = "<your SSid>";  //redirect用
const char *password1 = "<your Pass>";
//ここからGAS定義
const String url = "<your url>";//試し中

const char *host = "script.google.com";
const int httpsPort = 443;//ここまで
String payload = "";
HTTPSRedirect *client = nullptr;

unsigned long lastCaptureTime = 0; // Last shooting time
int imageCount = 1;                // File Counter
bool camera_sign = false;          // Check camera status
bool sd_sign = false;              // Check sd status

camera_fb_t * fb;

String imageFile = "";
//GASに送るためにbase64にエンコード
/*
ase64エンコーディング関数: a3_to_a4、a4_to_a3、b64_lookup、base64_encode、base64_enc_len関数は、
Base64エンコーディングとデコーディングを行うためのものです。これらはバイナリデータ（ここでは画像データ）をASCII文字列に変換するために使用されます。

createBase64Encode関数: この関数は、入力として与えられたバイナリデータ（ここでは画像データ）をBase64エンコードした文字列を生成します。
この文字列は後でHTTPリクエストで送信するために使用されます。
*/
void photo_save(const char * fileName) {
  // Take a photo
  camera_fb_t *fb = esp_camera_fb_get();//ここで撮影し、それを*fb(フレームバッファ)に保存している
  if (!fb) {
    Serial.println("Failed to get camera frame buffer");
    return;
  } 
  //writeFile(SD, fileName, fb->buf, fb->len);
  imageFile = createBase64Encode((char *)fb->buf, fb->len);//ここ追加　
  // Release image buffer
  esp_camera_fb_return(fb);

  Serial.println("Photo saved to file");//うまくいっている
}


void Httpsr_connect() {
  client = new HTTPSRedirect(httpsPort);
  client->setInsecure();
  client->setPrintResponseBody(true);
  //client->setContentTypeHeader("application/json");
  client->setContentTypeHeader("text/plain");

  Serial.print("Connecting to ");
  Serial.println(host);

  // Try to connect for a maximum of 5 times
  for (int i = 0; i < 5; i++) {
    int retval = client->connect(host, httpsPort);
      
    if (retval == 1) {
      Serial.println("接続成功");//多分成功している
      break;
    } else {
      Serial.println("Connection failed. Retrying...");
      delay(5);
    }
  }
}



void setup() {
  Serial.begin(115200);
  while(!Serial); // When the serial monitor is turned on, the program starts to execute

  WiFi.begin(ssid1, password1);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
//ここからcamraのPINなどの設定
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
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_UXGA;//FRAMESIZE_VGA;と何が違うのか
  config.pixel_format = PIXFORMAT_JPEG; // for streaming
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 1;
  
  

  if(config.pixel_format == PIXFORMAT_JPEG){
    if(psramFound()){
      config.jpeg_quality = 10;
      config.fb_count = 2;
      config.grab_mode = CAMERA_GRAB_LATEST;
    } else {
      // Limit the frame size when PSRAM is not available
      config.frame_size = FRAMESIZE_SVGA;
      config.fb_location = CAMERA_FB_IN_DRAM;
    }
  } else {
    // Best option for face detection/recognition
    config.frame_size = FRAMESIZE_240X240;
#if CONFIG_IDF_TARGET_ESP32S3
    config.fb_count = 2;
#endif
  }
  // camera init　カメラの初期化
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }//ここまで
  
  camera_sign = true; // Camera initialization check passes

  // Initialize SD card　SDカードの初期化
  if(!SD.begin(21)){
    Serial.println("Card Mount Failed");
    return;
  }
  uint8_t cardType = SD.cardType();

  // Determine if the type of SD card is available
  if(cardType == CARD_NONE){
    Serial.println("No SD card attached");
    return;
  }

  sd_sign = true; // sd initialization check passes　
  //ここまで

  Serial.println("Photos will begin in one minute, please be ready.");
}
unsigned long now = 15000;

String urlencode(String str)//urlエンコードしている。
{
    String encodedString="";
    char c;
    char code0;
    char code1;
    char code2;
    for (int i =0; i < str.length(); i++){
      c=str.charAt(i);
      if (c == ' '){
        encodedString+= '+';
      } else if (isalnum(c)){
        encodedString+=c;
      } else{
        code1=(c & 0xf)+'0';
        if ((c & 0xf) >9){
            code1=(c & 0xf) - 10 + 'A';
        }
        c=(c>>4)&0xf;
        code0=c+'0';
        if (c > 9){
            code0=c - 10 + 'A';
        }
        code2='\0';
        encodedString+='%';
        encodedString+=code0;
        encodedString+=code1;
        //encodedString+=code2;
      }
      yield();
    }
    return encodedString;
}
//ここまでエンコーダ
const char PROGMEM b64_alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

// 'Private' declarations 
inline void a3_to_a4(unsigned char * a4, unsigned char * a3);
inline void a4_to_a3(unsigned char * a3, unsigned char * a4);
inline unsigned char b64_lookup(char c);


inline void a3_to_a4(unsigned char * a4, unsigned char * a3) {
  a4[0] = (a3[0] & 0xfc) >> 2;
  a4[1] = ((a3[0] & 0x03) << 4) + ((a3[1] & 0xf0) >> 4);
  a4[2] = ((a3[1] & 0x0f) << 2) + ((a3[2] & 0xc0) >> 6);
  a4[3] = (a3[2] & 0x3f);
}

inline void a4_to_a3(unsigned char * a3, unsigned char * a4) {
  a3[0] = (a4[0] << 2) + ((a4[1] & 0x30) >> 4);
  a3[1] = ((a4[1] & 0xf) << 4) + ((a4[2] & 0x3c) >> 2);
  a3[2] = ((a4[2] & 0x3) << 6) + a4[3];
}

inline unsigned char b64_lookup(char c) {
  if(c >='A' && c <='Z') return c - 'A';
  if(c >='a' && c <='z') return c - 71;
  if(c >='0' && c <='9') return c + 4;
  if(c == '+') return 62;
  if(c == '/') return 63;
  return -1;
}
//ここまで追加
//以下はbase64のエンコーダ
int base64_encode(char *output, char *input, int inputLen) { //この中身がまずい
  int i = 0, j = 0;
  int encLen = 0;
  unsigned char a3[3];
  unsigned char a4[4];

  while(inputLen--) {
    a3[i++] = *(input++);
    if(i == 3) {
      a3_to_a4(a4, a3); //これが定義されていない

      for(i = 0; i < 4; i++) {
        output[encLen++] = pgm_read_byte(&b64_alphabet[a4[i]]);
      }

      i = 0;
    }
  }

  if(i) {
    for(j = i; j < 3; j++) {
      a3[j] = '\0';
    }

    a3_to_a4(a4, a3);

    for(j = 0; j < i + 1; j++) {
      output[encLen++] = pgm_read_byte(&b64_alphabet[a4[j]]);
    }

    while((i++ < 3)) {
      output[encLen++] = '=';
    }
  }
  output[encLen] = '\0';
  return encLen;
}

int base64_enc_len(int plainLen) {
  int n = plainLen;
  return (n + 2 - ((n + 2) % 3)) / 3 * 4;
}

// https://github.com/adamvr/arduino-base64 
String createBase64Encode(char *input, int input_len) {//base64変換
  char output[base64_enc_len(3)];
  String imageFile2 = imageFile;
  for (int i=0;i<input_len;i++) {
    base64_encode(output, (input++), 3);//動くようになった。
    if (i%3==0) imageFile2 += urlencode(String(output));
  }
  Serial.println("createBase64Encode は走っている3");
  return imageFile2;
  
}
//ここまでエンコードでコード関係

void loop() {
  if(camera_sign && sd_sign){
    now = millis();
  
    //If it has been more than 1 minute since the last shot, take a picture and save it to the SD card
    if ((now - lastCaptureTime) >= 5000 ) {
      lastCaptureTime = now;
      char filename[32];
      sprintf(filename, "/image%d.jpg", imageCount);
      //{
        photo_save(filename);//写真を撮って保存する関数　SDに保存する必要本来ない
        Serial.printf("Saved picture:%s\n", filename);
      //}
      
      
      imageCount++;
      
      //SendPic();

       String bodyTop = "filename=ESP32-CAM.jpg&"
                        "mimetype=image/jpeg&"
                        "data=";
    

  //ここから
    Serial.println("Send JPEG DATA by GAS");
    
    Httpsr_connect();//ここまではおｋ 

    Serial.println(imageFile);
    Serial.println("Waiting for response.");
    //送信開始
    if (client->POST(url, host, "start")) {
        Serial.println("hajimatta");
      }
  //中間データ
    //  JPEGデータは1000bytesに区切ってPOST
    for (int i = 0; i < imageFile.length(); i = i+1000) {
      //payload = readFile(SD, str.c_str());  //ここでGASに送り付ける strはString型
      payload = imageFile.substring(i, i+1000);  //ここでGASに送り付ける strはString型
      if (client->POST(url, host, payload)) {
      //if (client->POST(url, host, "payload")) {
        Serial.println("Success! send data");
      } 
    
    }
    //最終データ
    Serial.println("Waiting for response.");
    if (client->POST(url, host, "endend")) {
        Serial.println("owatta");
        //state = true;
      } 
    
  delete client;
  lastCaptureTime = now;
    }
  }
  
}

