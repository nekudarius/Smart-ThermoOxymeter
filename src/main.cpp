#include <Arduino.h>
#include <Wire.h>
#include <MAX30100_PulseOximeter.h>
#include <Adafruit_I2CDevice.h>
#include <Adafruit_MLX90614.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_GFX.h>
#include <SPI.h>
#include <SD.h>
#include <FS.h>

#include <WiFi.h>
// #include <Firebase_ESP_Client.h>
#include <FirebaseESP32.h>
#include <NTPClient.h>
#include <WiFiUdp.h>


//-------------------------------UNTUK OLED-------------------------------//
#define SCREEN_WIDTH 128 // OLED display width,  in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

Adafruit_SSD1306 oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
//------------------------------------------------------------------------//

//-------------------------------UNTUK PUSH BUTTON-------------------------------//

const unsigned long shortPress = 100;               //0.1s
const unsigned long  longPress = 1000;              //1s
const unsigned long  time_execute_program = 10000;  //10s

#define RESET_PRESS 0
#define SHORT_PRESS 1
#define LONG_PRESS  2

typedef struct Buttons {
    byte pin;                                       //0
    int debounce;                                   //1
    unsigned long counter;                          //2
    bool prevState;                                 //3
    bool currentState;                              //4
    int flag;                                       //5
    unsigned long cnt_debounce;                     //6
    int press_option;                               //7
    unsigned long cnt_time_execute_program;         //8
    int flag_execute_program;                       //9

} Button;
//              0   1   2   3   4   5   6   7   8   9
Button btn1 = {34,  10, 0,  0,  0,  0,  0,  0,  0,  0};
Button btn2 = {35,  10, 0,  0,  0,  0,  0,  0,  0,  0};
Button btn3 = {32,  10, 0,  0,  0,  0,  0,  0,  0,  0};

typedef struct Cnt_Delay {
    unsigned long time_delay;      //0           
    unsigned long cnt_delay;       //1
    int flag_delay;                //2

} CNT_DELAY;
//                  0   1   2
CNT_DELAY WIFI = {300,  0,  0};
CNT_DELAY SpO2 = {1000, 0,  0};

void BTN1_ALGORITHM();
void BTN2_ALGORITHM();
void BTN3_ALGORITHM();


//-------------------------------UNTUK WIFI-------------------------------//
#define WIFI_SSID "yuyun"
#define WIFI_PASSWORD "12345678"

// #define WIFI_SSID "nekudarius"
// #define WIFI_PASSWORD "evandragneel01"

//-------------------------------UNTUK FIREBASE-------------------------------//
//Provide the token generation process info.
#include "addons/TokenHelper.h"
//Provide the RTDB payload printing info and other helper functions.
#include "addons/RTDBHelper.h"

// Insert Firebase project API Key
#define API_KEY "AIzaSyAHpVrMmR3rLMgQA49avMQdTLq-ML3hH9g"

// Insert RTDB URLefine the RTDB URL */
#define DATABASE_URL "https://thermooxy-default-rtdb.asia-southeast1.firebasedatabase.app"

#define DATABASE_SECRET "W1vS2hqrGELOdhpzEmkUEswC7ZXcHjdgfgjcqxR9"

// Firebase Realtime Database Object
FirebaseData fbdo;
// Firebase Authentication Object
FirebaseAuth auth;
// Firebase configuration Object
FirebaseConfig config;
// Firebase Unique Identifier
String fuid = "";

unsigned long sendDataPrevMillis = 0;
int count = 0;
bool signupOK = false;

//-------------------------------UNTUK TIMESTAMP-------------------------------//
// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

// Variables to save date and time
String formattedDate;
String dayStamp;
String timeStamp;
String dataMessage_Time;

// Firebase string path
String SPO2_VALUE_PATH = "value/SpO2";
String TEMP_VALUE_PATH = "value/temp";
String SPO2_HISTORY_PATH;
String TEMP_HISTORY_PATH;

//-------------------------------UNTUK MLX906-------------------------------//
Adafruit_MLX90614 mlx = Adafruit_MLX90614();
 
double temp_amb;
double temp_obj;
double calibration = 2.36;
//-----------------------------------------------------------------------------//

PulseOximeter pox;
int BPM, Saturasi, BPM_VALUE, SAT_VALUE;

void onBeatDetected()
{   Serial.println("Beat!");    }

void Read_SPO2();
void Read_MLX();

//-------------------------- function for SD Card--------------//
#define SD_CS 5
String dataMessage;
int Flag_store_data;

void listDir(fs::FS &fs, const char * dirname, uint8_t levels);
void createDir(fs::FS &fs, const char * path);
void removeDir(fs::FS &fs, const char * path);
void readFile(fs::FS &fs, const char * path);
void writeFile(fs::FS &fs, const char * path, const char * message);
void appendFile(fs::FS &fs, const char * path, const char * message);
void deleteFile(fs::FS &fs, const char * path);
void sdcard_init ();
void logSDCard();
//--------------------------------------------------------------------//

void init_OLED();
void init_WiFi();
void init_SpO2();
void init_Firebase();
void Send_Firebase();
void Get_Time();

void Display_SPO2();
void Display_MLX();
void Display_Button();
void Display_Interface();

void setup()
{
    
    Serial.begin(115200);
    pinMode(btn1.pin,INPUT_PULLDOWN);
    pinMode(btn2.pin,INPUT_PULLDOWN);
    pinMode(btn3.pin,INPUT_PULLDOWN);
    
    init_OLED();    //inisialisasi OLED
    sdcard_init();  //inisialisasi SD card
    init_WiFi();    //inisialisasi  WiFi
    init_Firebase();//inisialisasi  Firebase

    timeClient.begin();
    timeClient.setTimeOffset(3600*7);

    mlx.begin();    //inisialisasi MLX
    init_SpO2();    //inisialisasi SpO2

}

void loop()
{
    pox.update();
    if (SpO2.flag_delay == 0) { SpO2.cnt_delay = millis(); SpO2.flag_delay = 1;}
    else if (SpO2.flag_delay == 1)
    {
        if ((millis() - SpO2.cnt_delay) > SpO2.time_delay)
        {
            BPM = pox.getHeartRate();
            Saturasi = pox.getSpO2();
            SpO2.flag_delay = 0;
        }
    }
    BTN1_ALGORITHM();
    BTN2_ALGORITHM();
    BTN3_ALGORITHM();

    if (btn1.press_option == LONG_PRESS)
    {
        if (btn1.flag_execute_program == 0) {btn1.cnt_time_execute_program = millis(); btn1.flag_execute_program = 1;}
        else if (btn1.flag_execute_program == 1)
        {
            if ( (millis() - btn1.cnt_time_execute_program) > time_execute_program )
            {
                btn1.flag_execute_program = 0;
                btn1.press_option = RESET_PRESS;
            }
            Read_MLX();
            oled.display();
        }
    }

    if (btn2.press_option == LONG_PRESS)
    {
        if (btn2.flag_execute_program == 0) {btn2.cnt_time_execute_program = millis(); btn2.flag_execute_program = 1;}
        else if (btn2.flag_execute_program == 1)
        {
            if ( (millis() - btn2.cnt_time_execute_program) > time_execute_program )
            {
                btn2.flag_execute_program = 0;
                btn2.press_option = RESET_PRESS;
            }
            Read_SPO2();
            oled.display();
        }
    }

    if (btn3.press_option == LONG_PRESS)
    {
        if (btn3.flag_execute_program == 0) {btn3.cnt_time_execute_program = millis(); btn3.flag_execute_program = 1;}
        else if (btn3.flag_execute_program == 1)
        {
            if ( (millis() - btn3.cnt_time_execute_program) > time_execute_program )
            {
                btn3.flag_execute_program = 0;
                Flag_store_data = 0;
                btn3.press_option = RESET_PRESS;
            }
            if (Flag_store_data == 0)
            {
                Get_Time();
                logSDCard();
                oled.display(); 
                Send_Firebase();
                Flag_store_data = 1;
            }
        }
    }

    if (btn1.press_option != LONG_PRESS && btn2.press_option != LONG_PRESS && btn3.press_option != LONG_PRESS)
    {
        Display_Interface();
        oled.display();
    }
}


void init_OLED()
{
    if (!oled.begin(SSD1306_SWITCHCAPVCC, 0x3C))  // JIKA TIDAK TERDETEKSI ADANYA OLED
    {
        Serial.println(F("SSD1306 allocation failed"));
        while (true);
    }
    oled.clearDisplay();
    oled.setTextColor(WHITE);
}

void init_WiFi()
{
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Connecting to Wi-Fi");
    oled.setCursor(0,30);   oled.setTextSize(1);    oled.print("Connecting to Wi-Fi");
    oled.setCursor(0,40);   oled.setTextSize(1);    oled.print("Please Wait");
    oled.display();

    while (WiFi.status() != WL_CONNECTED)   
    {
        if (WIFI.flag_delay == 0) {WIFI.cnt_delay = millis();   WIFI.flag_delay = 1;}
        else if (WIFI.flag_delay == 1)
        {
            if ((millis() - WIFI.cnt_delay) > WIFI.time_delay)
            {
                Serial.print("."); oled.print("."); oled.display();WIFI.flag_delay = 0;
            }
        }
    }
    Serial.println();
    Serial.print("Connected with IP: ");
    Serial.println(WiFi.localIP());
    Serial.println();
}

void init_Firebase()
{
    Firebase.begin(DATABASE_URL, DATABASE_SECRET);
    Firebase.reconnectWiFi(true);
    Firebase.setMaxRetry(fbdo, 3);
    Firebase.setMaxErrorQueue(fbdo, 30);
}

void Send_Firebase()
{

    SPO2_HISTORY_PATH = "history/" + dayStamp + "/" + timeStamp + "/SpO2";
    TEMP_HISTORY_PATH = "history/" + dayStamp + "/" + timeStamp + "/temp";

    if (Firebase.RTDB.setInt(&fbdo, SPO2_VALUE_PATH, SAT_VALUE))
    {
        Serial.println("PASSED");
        Serial.println("PATH: " + fbdo.dataPath());
        Serial.println("TYPE: " + fbdo.dataType());
    }
    else
    {
        Serial.println("FAILED");
        Serial.println("REASON: " + fbdo.errorReason());
    }
    if (Firebase.RTDB.setInt(&fbdo, TEMP_VALUE_PATH, (temp_obj + calibration)))
    {
        Serial.println("PASSED");
        Serial.println("PATH: " + fbdo.dataPath());
        Serial.println("TYPE: " + fbdo.dataType());
    }
    else
    {
        Serial.println("FAILED");
        Serial.println("REASON: " + fbdo.errorReason());
    }
    if (Firebase.RTDB.setInt(&fbdo, SPO2_HISTORY_PATH, SAT_VALUE))
    {
        Serial.println("PASSED");
        Serial.println("PATH: " + fbdo.dataPath());
        Serial.println("TYPE: " + fbdo.dataType());
    }
    else
    {
        Serial.println("FAILED");
        Serial.println("REASON: " + fbdo.errorReason());
    }
    if (Firebase.RTDB.setInt(&fbdo, TEMP_HISTORY_PATH, (temp_obj + calibration)))
    {
        Serial.println("PASSED");
        Serial.println("PATH: " + fbdo.dataPath());
        Serial.println("TYPE: " + fbdo.dataType());
    }
    else
    {
        Serial.println("FAILED");
        Serial.println("REASON: " + fbdo.errorReason());
    }
}

void init_SpO2()
{
    Serial.print("Initializing pulse oximeter..");   
    if (!pox.begin()) { Serial.println("FAILED");   for(;;);    }
    else {  Serial.println("SUCCESS");  }
    pox.setIRLedCurrent(MAX30100_LED_CURR_7_6MA);
    pox.setOnBeatDetectedCallback(onBeatDetected);
}

void BTN1_ALGORITHM()
{
    btn1.currentState = digitalRead(btn1.pin);

    if (btn1.currentState != btn1.prevState)
    {
        if (btn1.flag == 0) {btn1.cnt_debounce = millis(); btn1.flag = 1;}
        else if (btn1.flag == 1)
        {
            if ( (millis() - btn1.cnt_debounce) > btn1.debounce )
            {
                btn1.flag = 0;

                btn1.currentState = digitalRead(btn1.pin);
                if (btn1.currentState == HIGH) {btn1.counter = millis();}
                if (btn1.currentState == LOW)
                {
                    unsigned long currentMillis_btn1 = millis();
                    if ((currentMillis_btn1 - btn1.counter >= shortPress) && !(currentMillis_btn1 - btn1.counter >= longPress)) { 
                        btn1.press_option = SHORT_PRESS;
                        oled.clearDisplay();
                        oled.setCursor(10,0);
                        oled.setTextSize(1);
                        oled.print("SHORT PRESS BTN1");
                        oled.display();
                    }
                    if ((currentMillis_btn1 - btn1.counter >= longPress)) {
                        btn1.press_option = LONG_PRESS;
                    }
                }
                btn1.prevState = btn1.currentState;
            }
        }
    }
}

void BTN2_ALGORITHM()
{
    btn2.currentState = digitalRead(btn2.pin);

    if (btn2.currentState != btn2.prevState)
    {
        if (btn2.flag == 0) {btn2.cnt_debounce = millis(); btn2.flag = 1;}
        else if (btn2.flag == 1)
        {
            if ( (millis() - btn2.cnt_debounce) > btn2.debounce )
            {
                btn2.flag = 0;

                btn2.currentState = digitalRead(btn2.pin);
                if (btn2.currentState == HIGH) {btn2.counter = millis();}
                if (btn2.currentState == LOW)
                {
                    unsigned long currentMillis_btn2 = millis();
                    if ((currentMillis_btn2 - btn2.counter >= shortPress) && !(currentMillis_btn2 - btn2.counter >= longPress)) { 
                        btn2.press_option = SHORT_PRESS;
                        oled.clearDisplay();
                        oled.setCursor(10,0);
                        oled.setTextSize(1);
                        oled.print("SHORT PRESS BTN2 ");
                        oled.display();
                    }
                    if ((currentMillis_btn2 - btn2.counter >= longPress)) {
                        btn2.press_option = LONG_PRESS;
                    }
                }
                btn2.prevState = btn2.currentState;
            }
        }
    }
}

void BTN3_ALGORITHM()
{
    btn3.currentState = digitalRead(btn3.pin);

    if (btn3.currentState != btn3.prevState)
    {
        if (btn3.flag == 0) {btn3.cnt_debounce = millis(); btn3.flag = 1;}
        else if (btn3.flag == 1)
        {
            if ( (millis() - btn3.cnt_debounce) > btn3.debounce )
            {
                btn3.flag = 0;

                btn3.currentState = digitalRead(btn3.pin);
                if (btn3.currentState == HIGH) {btn3.counter = millis();}
                if (btn3.currentState == LOW)
                {
                    unsigned long currentMillis_btn3 = millis();
                    if ((currentMillis_btn3 - btn3.counter >= shortPress) && !(currentMillis_btn3 - btn3.counter >= longPress)) { 
                        btn3.press_option = SHORT_PRESS;
                        oled.clearDisplay();
                        oled.setCursor(10,0);
                        oled.setTextSize(1);
                        oled.print("SHORT PRESS BTN3 ");
                        oled.display();
                    }
                    if ((currentMillis_btn3 - btn3.counter >= longPress)) {
                        btn3.press_option = LONG_PRESS;
                    }
                }
                btn3.prevState = btn3.currentState;
            }
        }
    }
}

void Read_MLX()
{
    temp_amb = mlx.readAmbientTempC();
    temp_obj = mlx.readObjectTempC();
    Display_MLX(); 
}
void Read_SPO2()
{
    BPM_VALUE = BPM;
    SAT_VALUE = Saturasi;
    Display_SPO2();
}

void Get_Time()
{
    while(!timeClient.update()) {   timeClient.forceUpdate();   }
    // The formattedDate comes with the following format:   2021-10-20T16:00:13Z
    formattedDate = timeClient.getFormattedDate();
    Serial.println(formattedDate);

    // Extract date
    int splitT = formattedDate.indexOf("T");
    dayStamp = formattedDate.substring(0, splitT);
    Serial.print("DATE: "); Serial.println(dayStamp);
    // Extract time
    timeStamp = formattedDate.substring(splitT+1, formattedDate.length()-1);
    Serial.print("HOUR: "); Serial.println(timeStamp);
}

/**
 * @brief	lists the directories on the SD card
 * @retval	
 */
void listDir(fs::FS &fs, const char * dirname, uint8_t levels)
{
  Serial.printf("Listing directory: %s\n", dirname);

  File root = fs.open(dirname);

  if(!root) { Serial.println("Failed to open directory");       return; }
  if(!root.isDirectory()) { Serial.println("Not a directory");  return; }

  File file = root.openNextFile();
  while(file)
  {
    if(file.isDirectory())
    {
      Serial.print("  DIR : ");
      Serial.println(file.name());
      if(levels)  { listDir(fs, file.name(), levels -1);  }
    }
    else 
    {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("  SIZE: ");
      Serial.println(file.size());
    }
    file = root.openNextFile();
  }
}
/**
 * @brief	create a directories on the SD card
 * @retval	
 */
void createDir(fs::FS &fs, const char * path)
{
  Serial.printf("Creating Dir: %s\n", path);
  if(fs.mkdir(path)) { Serial.println("Dir created"); }
  else { Serial.println("mkdir failed");  }
}
/**
 * @brief	remove a directories from the SD card
 * @retval	
 */
void removeDir(fs::FS &fs, const char * path)
{
  Serial.printf("Removing Dir: %s\n", path);
  if(fs.rmdir(path)) {Serial.println("Dir removed"); }
  else { Serial.println("rmdir failed"); }
}
/**
 * @brief	read file on the SD card
 * @retval	
 */
void readFile(fs::FS &fs, const char * path)
{
  Serial.printf("Reading file: %s\n", path);

  File file = fs.open(path);
  if(!file) { Serial.println("Failed to open file for reading"); return; }

  Serial.print("Read from file: ");
  while(file.available()) { Serial.write(file.read()); }
  file.close();
}

/**
 * @brief	write on a file inside SD card
 * @retval	
 */
void writeFile(fs::FS &fs, const char * path, const char * message)
{
  Serial.printf("Writing file: %s\n", path);

  File file = fs.open(path, FILE_WRITE);
  if(!file) { Serial.println("Failed to open file for writing"); return; }
  if(file.print(message)) { Serial.println("File written"); }
  else { Serial.println("Write failed"); }
  file.close();
}
/**
 * @brief	append something into a file on the SD card
 * @retval	
 */
void appendFile(fs::FS &fs, const char * path, const char * message)
{
  Serial.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if(!file){ Serial.println("Failed to open file for appending"); return; }
  if(file.print(message)) { Serial.println("Message appended"); }
  else { Serial.println("Append failed"); }
  file.close();
}
/**
 * @brief	delete file on the SD card
 * @retval	
 */
void deleteFile(fs::FS &fs, const char * path){
  Serial.printf("Deleting file: %s\n", path);
  if(fs.remove(path)) { Serial.println("File deleted"); }
  else { Serial.println("Delete failed"); }
}

void sdcard_init ()
{
    if(!SD.begin(SD_CS)) 
    {
        Serial.println("Card Mount Failed");
        oled.setCursor(0,0);    oled.setTextSize(1);    oled.print("Card Mount Failed");
        oled.display();
        return;
    }
    uint8_t cardType = SD.cardType();
    if(cardType == CARD_NONE) {Serial.println("No SD card attached");return;}

    Serial.println("Initializing SD card...");
    oled.setCursor(0,0);    oled.setTextSize(1);    oled.print("Initializing SD card...");
    oled.display();

    if (!SD.begin(SD_CS)) {Serial.println("ERROR - SD card initialization failed!");return;}

    File file = SD.open("/thermooxy.txt");
    if(!file)
    {
        Serial.println("File doens't exist");
        Serial.println("Creating file...");
        oled.setCursor(0,10);   oled.setTextSize(1);    oled.print("File doesn't exist");
        oled.setCursor(0,20);   oled.setTextSize(1);    oled.print("Creating file...");
        oled.display();
        writeFile(SD, "/thermooxy.txt", "Welcome to SMART THERMOOXYMETER \r\n");
    }
    else 
    { 
        Serial.println("File already exists");  
        oled.setCursor(0,10);   oled.setTextSize(1);    oled.print("File already exists");
        oled.display();
    }
    file.close();
}

void logSDCard()
{
    dataMessage_Time = "DATE: " + String (dayStamp) + "HOUR: " + String (timeStamp)  +"\r\n";
    appendFile(SD, "/thermooxy.txt", dataMessage_Time.c_str());

    dataMessage =  "SpO2: " + String(SAT_VALUE) + "%  Body Temp: " + String(temp_obj+calibration) + String((char)247) +"C \r\n";
    Serial.print("Save data: ");
    Serial.println(dataMessage);
    appendFile(SD, "/thermooxy.txt", dataMessage.c_str());

    oled.clearDisplay();
    oled.setCursor(0,10);   oled.setTextSize(1);        oled.print("store data to SD Card");

    oled.setCursor(0,20);   oled.setTextSize(1);    oled.print("DATE: ");       oled.print(dayStamp);
    oled.setCursor(0,30);   oled.setTextSize(1);    oled.print("HOUR: ");       oled.print(timeStamp);
    oled.setCursor(0,40);   oled.setTextSize(1);    oled.print("Save data: ");  oled.print(dataMessage);
}

void Display_SPO2()
{
    oled.clearDisplay();
    oled.setCursor(10,10);
    oled.setTextSize(1);
    oled.print("OXYGEN SATURATION");

    // oled.setCursor(10,10);
    // oled.setTextSize(1);
    // oled.print("bpm: ");
    // oled.print(BPM_VALUE);
    
    oled.setCursor(20,32);
    oled.setTextSize(1);
    oled.print("SpO2: ");
    oled.print(SAT_VALUE);
    oled.print(" %");
}
void Display_MLX()
{

    oled.clearDisplay();
    oled.setCursor(15,10);
    oled.setTextSize(1);
    oled.print("BODY TEMPERATURE");

    // oled.setCursor(10,30);
    // oled.setTextSize(1);
    // oled.print("Ambient: ");
    // oled.print(temp_amb);
    // oled.print((char)247);
    // oled.print("C");
    
    oled.setCursor(15,32);
    oled.setTextSize(1);
    oled.print("Object: ");
    oled.print(temp_obj + calibration);
    oled.print((char)247);
    oled.print("C");
}
void Display_Button()
{
    oled.setCursor(10,50);
    oled.setTextSize(1);
    oled.print("btn: ");
    oled.setCursor(35,50);
    oled.setTextSize(1);
    oled.print(btn1.currentState);

    oled.setCursor(45,50);
    oled.setTextSize(1);
    oled.print(btn2.currentState);

    oled.setCursor(55,50);
    oled.setTextSize(1);
    oled.print(btn3.currentState);
}
void Display_Interface()
{
    oled.clearDisplay();
    oled.setCursor(10,10);
    oled.setTextSize(1);
    oled.print("OXYGEN SATURATION");
    
    oled.setCursor(10,20);
    oled.setTextSize(1);
    oled.print("SpO2: ");
    oled.print(SAT_VALUE);
    oled.print(" %");

    oled.setCursor(10,40);
    oled.setTextSize(1);
    oled.print("BODY TEMPERATURE");

    oled.setCursor(10,50);
    oled.setTextSize(1);
    oled.print("Object: ");
    oled.print(temp_obj + calibration);
    oled.print((char)247);
    oled.print("C");
}


