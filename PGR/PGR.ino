//--------------------------------Мобильная наземная приёмная станция МНПС-1-------------------------------

#include <SPI.h>                                          // библиотека SPI
#include <nRF24L01.h>                                     // Подключаем файл настроек из библиотеки RF24
#include <RF24.h>
#include <UTFT.h>                                         // библиотека дисплея
#include <SD.h>                                           // библиотека карты памяти
#include "buttonMinim.h"                                  // библиотека кнопки


#include "bytes.h"                                        // байты телеметрии

RF24           radio(7, 8);                               // Создаём объект radio   для работы с библиотекой RF24, указывая номера выводов nRF24L01+ (CE, CSN)
byte           data[32];          // массив с данными
float temperature[2] = {0, 0}; //массив, в который записывается температура
boolean sd_work, sd_rec, start_rec = false, stop_rec = false, fullprint = false;
String filename;
byte file_num = 0;

extern uint8_t SmallFont[];

UTFT myGLCD(ITDB32S, 38, 39, 40, 41); //дисплей

File myFile;

#define SERIAL_ON 1 // 1 - вывод данных в Serial, 0 - нет вывода
#define REC_BUT 6   // пин кнопки записи
#define SCR_BUT 5   // пин кнопки управления экраном
#define BUZ A0

buttonMinim rec_btn(REC_BUT);
buttonMinim scr_btn(SCR_BUT);

void setup() {
#if SERIAL_ON
  Serial.begin(9600);
#endif

   tone(BUZ,500,500);
   delay(1000);
  // Запуск и проверка наличия SD карты
  if (SD.begin(53)) {
    sd_work = true;
    tone(BUZ,1000,100);
  }
  else {
    sd_work = false;
    tone(BUZ,500,300);
  }
  delay(300);
  // Инициализация экрана
  myGLCD.InitLCD();
  myGLCD.setFont(SmallFont);

  // Инициируем работу nRF24L01+ и выводим ошибку на экран, если модуль не работает
  if (!radio.begin()) {
    myGLCD.print("NRF24L01 doesn't work!", CENTER, 0);
    tone(BUZ,750);
    while (true) {}
  }
  tone(BUZ,1000,100);
  radio.setChannel(0x70);                               // Канал приёма данных 
  radio.setDataRate(RF24_250KBPS);                      // Скорость передачи данных (RF24_250KBPS, RF24_1MBPS, RF24_2MBPS), RF24_1MBPS - 1Мбит/сек
  radio.setPALevel(RF24_PA_LOW);                        // Мощность передатчика (RF24_PA_MIN=-18dBm, RF24_PA_LOW=-12dBm, RF24_PA_HIGH=-6dBm, RF24_PA_MAX=0dBm)
  radio.setRetries(0, 0);                               // Задержка и количество переотправок
  radio.setPayloadSize(32);                             // Размер пакета
  radio.setAutoAck(false);                              // Отключаем автопотверждение
  radio.disableCRC();                                   // Отключаем проверку контрольной суммы
  radio.openReadingPipe (1, 0x1234567890LL);            // Открываем 1 трубу с идентификатором 0x1234567890 для приема данных (на одном канале может быть открыто до 6 разных труб, которые должны отличаться только последним байтом идентификатора)
  radio.startListening  ();                             // Включаем приемник, начинаем прослушивать открытую трубу



  // Очистка экрана и вывод заголовка
  myGLCD.setColor(255, 255, 255);
  //myGLCD.clrScr();
  myGLCD.fillRect(0, 0, 319, 240);
  myGLCD.setBackColor(255, 0, 0);
  myGLCD.setColor(255, 255, 0);
  myGLCD.print("        Portable Ground Receiver        ", CENTER, 0);
  myGLCD.setBackColor(255, 255, 255);
  myGLCD.setColor(0, 0, 255);

  myGLCD.print(sd_work ? "SD Card Work" : "SD Card Fail", LEFT, 227);

  // Создание нового файла
  if (sd_work) {
    filename = "PGR_" + String(file_num) + ".txt";
    while (SD.exists(filename)) {
      file_num++;
      filename = "PGR_" + String(file_num) + ".txt";
    }

    myFile = SD.open(filename, FILE_WRITE);
    myFile.println("Portable Ground Receiver ready!");
    myFile.close();
  }


#if SERIAL_ON==1
  Serial.println("Portable Ground Receiver ready!");
#endif
}

double last_p = millis();

unsigned long but_timer = 0;

void loop() {
  if (rec_btn.holded()) sd_rec = !sd_rec; // Начало или остановка записи при нажатии на кнопку
  if (scr_btn.holded()) fullprint = !fullprint;                 // Включение и выключение полного вывода данных при нажатии на кнопку

  myGLCD.print((sd_rec && sd_work) ? "Do Recording" : "No Recording", RIGHT, 227);
  myGLCD.print(fullprint ? "Full Mode" : "Fast Mode", RIGHT, 12);

  if (!start_rec && !stop_rec)  myGLCD.print("Signal Waiting... ", LEFT, 12);
  else if (start_rec)           myGLCD.print("Receiving data... ", LEFT, 12);
  else if (stop_rec)            myGLCD.print("Receiving stopped!", LEFT, 12);

  if (radio.available()) {                              // Если в буфере имеются принятые данные
    radio.read(&data, sizeof(data));                  // Читаем данные в массив data и указываем сколько байт читать
    //tone(BUZ,750,75);
    String dat = "   "+String(data[0])+"   ";
    myGLCD.print(dat, CENTER, 12);
    if (data[0] == 127) {
      sd_save("Receiving data... ");
      start_rec = true;
      stop_rec = false; 
      sd_rec = true;}
      
    else if (data[0] == 255) {
      sd_save("Receiving stopped!");
      start_rec = false;
      stop_rec = true;
      sd_rec = false;
    }

    if (data[0]==16){
    // Формирование строк
    String freq = "  " + String(1000 / (millis() - last_p)) + " pps   ";
    last_p = millis();

    int num = data[NP_LSB] * 256 + data[NP_MSB];
    char number_packet[5];
    sprintf(number_packet, "%05d" , num);
    String num_str = String(number_packet);

    int pressure = data[PR_LSB] * 256 + data[PR_MSB];
    String pressure_str = "  PR:" + String(pressure) + "   ";

    //у температуры игнорируем пустые пакеты, чтобы они не отражались на экране (но в память будет записаны и "пустые" значения!)
    float temp_ext = (data[TN_LSB] * 256 + 
    data[TN_MSB]) / 10.0 - 55;
    if (temp_ext != 130) {
      temperature[0] = temp_ext;
    }
    String temp_ext_str = "  TN:" + String(temperature[0]) + "   ";

    float temp_int = (data[TV_LSB] * 256 + data[TV_MSB]) / 10.0 - 55;
    if (temp_int != 130) {
      temperature[1] = temp_int;
    }
    String temp_int_str = "  TV:" + String(temperature[1]) + "   ";

    byte hum = data[HM];
    String hum_str = "  HM:" + String(hum) + "   ";

    int height = data[HE_LSB] * 256 + data[HE_MSB] - 500;
    String height_str = "  HE:" + String(hum) + "   ";

    float accel_x = (data[AX] - 127) * 1.25;
    float accel_y = (data[AY] - 127) * 1.25;
    float accel_z = (data[AZ] - 127) * 1.25;
    String accel_str = "   A:" + String(accel_x) + " " + String(accel_y) + " " + String(accel_z) + "   ";

    int he = data[HE_LSB]*256+data[HE_MSB]-500;
    String he_str = "  HE:" + String(hum) + "     ";

    int lv = data[LV_LSB]*256+data[LV_MSB];
    String lv_str = "  LV:" + String(lv) + "   ";

    int li = data[LI_LSB]*256+data[LI_MSB];
    String li_str = "  LI:" + String(li) + "   ";
    
    float lat = (data[LA_0] * 1000000 + data[LA_1] * 10000 + data[LA_2] * 100 + data[LA_3]) / 1000000;
    String lat_str = "  LA:" + String(lat) + "   ";
    
    float lon = (data[LO_0] * 1000000 + data[LO_1] * 10000 + data[LO_2] * 100 + data[LO_3]) / 1000000;
    String lon_str = "  LO:" + String(lon) + "   ";
   
    int mins = data[HOMI];
    int secs = data[SE];
    int ms = data[MS]*10;
    String tme = "   Time: " + String(mins) + ":" + String(secs) + "." + String(ms) + "   ";

    int al = data[AL_LSB]*256+data[AL_MSB];
    String al_str = "  AL:" + String(al) + "   ";


    // Вывод строк на дисплей
    myGLCD.print(num_str, CENTER, 36);
    myGLCD.print(pressure_str, LEFT, 60);
    

    if (fullprint) {
      myGLCD.print(temp_ext_str, CENTER, 60);
      myGLCD.print(temp_int_str, RIGHT, 60);
      myGLCD.print(hum_str, LEFT, 72);
      myGLCD.print(accel_str, CENTER, 84);
      myGLCD.print(lv_str, LEFT, 96); 
      myGLCD.print(li_str, RIGHT, 96); 
      myGLCD.print(lat_str, LEFT, 108);
      myGLCD.print(lon_str, RIGHT, 108);
      myGLCD.print(tme, CENTER, 120);
      myGLCD.print(height_str, LEFT, 132);
      myGLCD.print(al_str, RIGHT, 132);

    }

    myGLCD.print(freq, CENTER, 192);
    

    // Вывод данных в Serial
#if SERIAL_ON
    //Serial.print(millis()/1000);   Serial.print(" ");
    //for (byte i=0; i<32;i++){
    //Serial.print(data[i]); Serial.print(" ");}
    Serial.print(num);
    Serial.print(" PR: "); Serial.print(pressure);      Serial.print(" ");
    
    Serial.print("TN: ");  Serial.print(temperature[0]); Serial.print(" ");
    Serial.print("TV: ");  Serial.print(temperature[1]); Serial.print(" ");
    
    Serial.print("HM: ");  Serial.print(hum);            Serial.print(" ");
    
    Serial.print("AX: ");  Serial.print(accel_x);        Serial.print(" ");
    Serial.print("AY: ");  Serial.print(accel_y);        Serial.print(" ");
    Serial.print("AZ: ");  Serial.print(accel_z);        Serial.print(" ");
    
    Serial.print("HE: ");  Serial.print(height);         Serial.print(" ");
    
    Serial.print("LV: ");  Serial.print(lv);             Serial.print(" ");
    Serial.print("LI: ");  Serial.print(li);             Serial.print(" ");
    
    Serial.print("MI: ");  Serial.print(mins);           Serial.print(" ");
    Serial.print("SE: ");  Serial.print(secs);           Serial.print(" ");
    Serial.print("MS: ");  Serial.print(ms);             Serial.print(" ");
    
    Serial.print("LA: ");  Serial.print(lat);            Serial.print(" ");
    Serial.print("LO: ");  Serial.print(lon);            Serial.print(" ");
    Serial.print("AL: ");  Serial.print(al);             Serial.print(" ");
    //Serial.print(tme);           // Serial.print(" ");
    //Serial.print("HE: ");  Serial.print(height);         Serial.print(" ");

    Serial.println("");
#endif

   String data_sd = String(number_packet) + " P:" + String(pressure);
   data_sd += " TN: " + String(temperature[0]) + " TV: " + String(temperature[1]) + " HM: " + String(hum);   
   data_sd += " AX: " + String(accel_x) + " AY: " + String(accel_y) + " AZ: " + String(accel_z); 
   data_sd += " HE: " + String(height);
   data_sd += " LV: " + String(lv) + " LI: " + String(li);  
   data_sd += " MI: " + String(mins) + " SE: " + String(secs) + " MS: " + String(ms); 
   data_sd += " LA: " + String(lat) + " LO: " + String(lon) + " AL: " + String(al);  

   if (sd_rec) sd_save(data_sd);
   }
  }
}

void sd_save(String dat){
  if (sd_work) {
      myFile = SD.open(filename, FILE_WRITE);
      myFile.println(dat);
      myFile.close();
    }
}
