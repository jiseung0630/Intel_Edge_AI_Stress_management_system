#include <WiFiEsp.h>
#include <SoftwareSerial.h>
#include <MsTimer2.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>
#include <DHT.h>
#include <stdlib.h>

#define DEBUG
#define PROCESSING_VISUALIZER 1
#define SERIAL_PLOTTER  2
#define DHTPIN 4
#define ARR_CNT 5
#define WIFITX 7  //7:TX -->ESP8266 RX
#define WIFIRX 8  //8:RX-->ESP8266 TX

#define AP_SSID "iot0"
#define AP_PASS "iot00000"
#define SERVER_NAME "10.10.141.82"
#define SERVER_PORT 5000
#define LOGID "JYJ_ARD"
#define PASSWD "PASSWD"
#define DHTTYPE DHT11
#define CMD_SIZE 50

//  Heart Beat Variables
int pulsePin = A0;                 // Pulse Sensor purple wire connected to analog pin 0
int blinkPin = 13;                // pin to blink led at each beat
int fadePin = 5;                  // pin to do fancy classy fading blink at each beat
int fadeRate = 0;                 // used to fade LED on with PWM on fadePin

volatile int rate[10]; // 마지막 10개의 IBI 값을 저장하는 배열
volatile unsigned long sampleCounter = 0; // 맥박 타이밍을 결정하는 데 사용
volatile unsigned long lastBeatTime = 0;  // IBI를 찾는 데 사용
volatile int P = 512; // 맥파의 피크를 찾는 데 사용, 초기값 설정됨
volatile int T = 512; // 맥파의 최저점을 찾는 데 사용, 초기값 설정됨
volatile int thresh = 530; // 심박 순간을 찾는 데 사용, 초기값 설정됨
volatile int amp = 0; // 맥파의 진폭을 저장하는 데 사용, 초기값 설정됨
volatile boolean firstBeat = true; // 시작할 때 적절한 BPM을 얻기 위해 rate 배열을 초기화하는 데 사용
volatile boolean secondBeat = false; // 시작할 때 적절한 BPM을 얻기 위해 rate 배열을 초기화하는 데 사용
unsigned long lastReadTime = 0;
const unsigned long readInterval = 5; // 3밀리초마다 읽기
volatile int IBI = 600; // 비트 간의 간격(밀리초 단위), 초기값 설정
volatile boolean Pulse = false; // 맥박 상태를 저장하는 변수
volatile boolean QS = false; // 맥박을 감지했는지 여부를 저장하는 변수

// Sensor Variables
static int hbeat = 0;
float temp = 0.0;
float humi = 0.0;
int sensorTime;
unsigned int secCount;

// Flags
bool timerIsrFlag = false;
bool updatTimeFlag = false;

// Communication Variables
char sendId[10] = "JYJ_ARD";
char sendBuf[CMD_SIZE];
char getSensorId[10];

typedef struct {
  int year;
  int month;
  int day;
  int hour;
  int min;
  int sec;
} DATETIME;

DATETIME dateTime = {0, 0, 0, 12, 0, 0};
DHT dht(DHTPIN, DHTTYPE);
SoftwareSerial wifiSerial(WIFIRX, WIFITX);
WiFiEspClient client;

void setup() {
  #ifdef DEBUG
  Serial.begin(115200); //DEBUG
  #endif
  pinMode(pulsePin, INPUT);
  pinMode(blinkPin, OUTPUT);
  wifi_Setup();
  MsTimer2::set(1000, timerIsr); // 1000ms period
  MsTimer2::start();
  dht.begin();  
}

void loop() {
  if (client.available()) {
    socketEvent();
  }
  unsigned long currentTime = millis();
  if (currentTime - lastReadTime >= readInterval) {
    lastReadTime = currentTime;
    readPulseSensor();
  }
  if (timerIsrFlag) //1초에 한번씩 실행
  {
    timerIsrFlag = false;
    if (!(secCount % 1)) //5초에 한번씩 실행, 심박,습도,온도
    {
      humi = dht.readHumidity();
      temp = dht.readTemperature();

      #ifdef DEBUG
      Serial.print("HeartBeat: ");
      Serial.print(hbeat);
      Serial.print(" Humidity: ");
      Serial.print(humi);
      Serial.print(" Temperature: ");
      Serial.println(temp);
      #endif
    }
    if (!client.connected()) {
      server_Connect();
    }
    if (sensorTime != 0 && !(secCount % sensorTime)) {
      sprintf(sendBuf, "[%s]SENSOR@%d@%d@%d\r\n", getSensorId,(int)hbeat,(int)temp, (int)humi);
      //sprintf(sendBuf, "[%s]SENSOR@%d@%d\r\n", getSensorId,(int)temp, (int)humi);
      client.write(sendBuf, strlen(sendBuf));
      client.flush();
    }
    if (updatTimeFlag) {
      client.print("[GETTIME]\n");
      updatTimeFlag = false;
    }
                     //  take a break
  }
}
void socketEvent() {
  int i = 0;
  char *pToken;
  char *pArray[ARR_CNT] = {0};
  char recvBuf[CMD_SIZE] = {0};
  int len;

  sendBuf[0] = '\0';

  len = client.readBytesUntil('\n', recvBuf, CMD_SIZE);
  client.flush();

  #ifdef DEBUG
  Serial.print("recv : ");
  Serial.print(recvBuf);
  #endif

  pToken = strtok(recvBuf, "[@]");
  while (pToken != NULL) {
    pArray[i] = pToken;
    if (++i >= ARR_CNT)
      break;
    pToken = strtok(NULL, "[@]");
  }

  if (!strncmp(pArray[1], " New", 4)) // New Connected
  {
    #ifdef DEBUG
    Serial.write('\n');
    #endif
    updatTimeFlag = true;
    return;
  } else if (!strncmp(pArray[1], " Alr", 4)) //Already logged
  {
    #ifdef DEBUG
    Serial.write('\n');
    #endif
    client.stop();
    server_Connect();
    return;
  } else if (!strncmp(pArray[1], "GETSENSOR", 9)) {
    if (pArray[2] != NULL) {
      sensorTime = atoi(pArray[2]);
      strcpy(getSensorId, pArray[0]);
      return;
    } else {
      sensorTime = 0;
      sprintf(sendBuf, "[%s]%s@%d@%d@%d\n", pArray[0], pArray[1],(int)hbeat,(int)humi,(int)temp);
      //sprintf(sendBuf, "[%s]%s@%d@%d\n", pArray[0], pArray[1], (int)humi, (int)temp);
    }
  } else if (!strcmp(pArray[0], "GETTIME")) {  //GETTIME
    dateTime.year = (pArray[1][0] - 0x30) * 10 + pArray[1][1] - 0x30;
    dateTime.month = (pArray[1][3] - 0x30) * 10 + pArray[1][4] - 0x30;
    dateTime.day = (pArray[1][6] - 0x30) * 10 + pArray[1][7] - 0x30;
    dateTime.hour = (pArray[1][9] - 0x30) * 10 + pArray[1][10] - 0x30;
    dateTime.min = (pArray[1][12] - 0x30) * 10 + pArray[1][13] - 0x30;
    dateTime.sec = (pArray[1][15] - 0x30) * 10 + pArray[1][16] - 0x30;
    #ifdef DEBUG
    sprintf(sendBuf, "\nTime %02d.%02d.%02d %02d:%02d:%02d\n\r", dateTime.year, dateTime.month, dateTime.day, dateTime.hour, dateTime.min, dateTime.sec);
    Serial.println(sendBuf);
    #endif
    return;
  } else {
    return;
  }
  client.write(sendBuf, strlen(sendBuf));
  client.flush();

  #ifdef DEBUG
  Serial.print(", send : ");
  Serial.print(sendBuf);
  #endif
}

void timerIsr() {
  timerIsrFlag = true;
  secCount++;
  clock_calc(&dateTime);
}

void clock_calc(DATETIME *dateTime) {
  dateTime->sec++; // increment second

  if (dateTime->sec >= 60) { // if second = 60, second = 0
    dateTime->sec = 0;
    dateTime->min++;

    if (dateTime->min >= 60) { // if minute = 60, minute = 0
      dateTime->min = 0;
      dateTime->hour++; // increment hour
      if (dateTime->hour == 24) {
        dateTime->hour = 0;
        updatTimeFlag = true;
      }
    }
  }
}

void wifi_Setup() {
  wifiSerial.begin(38400);
  wifi_Init();
  server_Connect();
}

void wifi_Init() {
  do {
    WiFi.init(&wifiSerial);
    if (WiFi.status() == WL_NO_SHIELD) {
      #ifdef DEBUG_WIFI
      Serial.println("WiFi shield not present");
      #endif
    } else {
      break;
    }
  } while (1);

  #ifdef DEBUG_WIFI
  Serial.print("Attempting to connect to WPA SSID: ");
  Serial.println(AP_SSID);
  #endif
  while (WiFi.begin(AP_SSID, AP_PASS) != WL_CONNECTED) {
    #ifdef DEBUG_WIFI
    Serial.print("Attempting to connect to WPA SSID: ");
    Serial.println(AP_SSID);
    #endif
  }
  #ifdef DEBUG_WIFI
  Serial.println("You're connected to the network");
  printWifiStatus();
  #endif
}

int server_Connect() {
  #ifdef DEBUG_WIFI
  Serial.println("Starting connection to server...");
  #endif

  if (client.connect(SERVER_NAME, SERVER_PORT)) {
    #ifdef DEBUG_WIFI
    Serial.println("Connect to server");
    #endif
    client.print("[" LOGID ":" PASSWD "]");
  } else {
    #ifdef DEBUG_WIFI
    Serial.println("server connection failure");
    #endif
  }
}

void printWifiStatus() {
  // print the SSID of the network you're attached to
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your WiFi shield's IP address
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength
  long rssi = WiFi.RSSI();
  Serial.print("Signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}

void readPulseSensor() {
  int Signal = analogRead(pulsePin); // Pulse 센서 읽기
  sampleCounter += readInterval; // 이 변수로 시간을 밀리초 단위로 추적
  int N = sampleCounter - lastBeatTime; // 마지막 비트 이후의 시간을 모니터링하여 노이즈 방지

  /*//Serial.print("Signal = ");
  Serial.println(Signal);

  Serial.print("N = ");
  Serial.println(N);

  Serial.print("IBI = ");
  Serial.println(IBI);//*/

  // 맥파의 피크와 최저점을 찾습니다.
  if (Signal < thresh && N > (IBI / 5) * 3) { // 마지막 IBI의 3/5를 기다려 이중 노이즈 방지
    if (Signal < T) { // T는 최저점
      T = Signal; // 맥파의 최저점을 추적
    }
  }

  if (Signal > thresh && Signal > P) { // thresh 조건은 노이즈 방지에 도움
    P = Signal; // P는 피크
  } // 맥파의 최고점을 추적

  // 이제 심박수를 찾을 때입니다.
  // 신호가 매번 맥박이 있을 때마다 급증합니다.
  if (N > 250) { // 고주파 노이즈 방지
    if ((Signal > thresh) && (Pulse == false) && (N > (IBI / 5) * 3)) {
      Pulse = true; // 맥박이 있다고 판단되면 Pulse 플래그 설정
      digitalWrite(blinkPin, HIGH); // 핀 13 LED 켜기
      IBI = sampleCounter - lastBeatTime; // 비트 간의 시간을 밀리초 단위로 측정
      lastBeatTime = sampleCounter; // 다음 맥박을 위해 시간 추적

      if (secondBeat) { // 만약 이것이 두 번째 비트라면
        secondBeat = false; // secondBeat 플래그 해제
        for (int i = 0; i <= 9; i++) { // 시작할 때 현실적인 BPM을 얻기 위해 running total 초기화
          rate[i] = IBI;
        }
      }

      if (firstBeat) { // 첫 번째 맥박을 찾은 경우
        firstBeat = false; // firstBeat 플래그 해제
        secondBeat = true; // secondBeat 플래그 설정
        return; // IBI 값이 신뢰할 수 없으므로 폐기
      }

      // 마지막 10개의 IBI 값을 유지합니다.
      word runningTotal = 0; // runningTotal 변수를 초기화

      for (int i = 0; i <= 8; i++) { // rate 배열의 데이터를 이동
        rate[i] = rate[i + 1]; // 가장 오래된 IBI 값을 제거
        runningTotal += rate[i]; // 가장 오래된 9개의 IBI 값을 더함
      }

      rate[9] = IBI; // 최신 IBI를 rate 배열에 추가
      runningTotal += rate[9]; // 최신 IBI를 runningTotal에 추가
      runningTotal /= 10; // 마지막 10개의 IBI 값을 평균
      hbeat = 60000 / runningTotal; // 1분에 몇 번의 비트가 있는지 계산, 이것이 BPM!
      QS = true; // Quantified Self 플래그 설정
      // QS 플래그는 이 ISR 내에서 해제되지 않음
    }
  }

  if (Signal < thresh && Pulse == true) { // 값이 하락할 때 맥박이 끝남
    digitalWrite(blinkPin, LOW); // 핀 13 LED 끄기
    Pulse = false; // Pulse 플래그 초기화
    amp = P - T; // 맥파의 진폭 측정
    thresh = amp / 2 + T; // 진폭의 50%에서 thresh 설정
    P = thresh; // 다음을 위해 값 초기화
    T = thresh;
  }

  if (N > 2500) { // 2.5초 동안 비트가 없을 경우
    thresh = 530; // thresh 기본값 설정
    P = 512; // P 기본값 설정
    T = 512; // T 기본값 설정
    lastBeatTime = sampleCounter; // lastBeatTime을 최신으로 업데이트
    firstBeat = true; // 노이즈 방지를 위해 값 설정
    secondBeat = false; // 심박을 다시 찾을 때 설정
  }
}
