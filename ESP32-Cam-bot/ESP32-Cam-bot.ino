#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include "esp_camera.h"
#include <ArduinoJson.h>
#include "camera_code.h"

// Wifi network station credentials
#define WIFI_SSID "vase"
#define WIFI_PASSWORD "12345678"
// Telegram BOT Token (Get from Botfather)
#define BOT_TOKEN "1834701113:AAGDe6zs57EGITvFULlMOaLWblAHc9xF3k8"
String chatId = "1870124256";

#define FLASH_LED_PIN 4

const unsigned long BOT_MTBS = 1000; // mean time between scan messages

unsigned long bot_lasttime; // last time messages' scan has been done
WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);

bool flashState = LOW;
bool motionDetected = false;

camera_fb_t *fb = NULL;

bool isMoreDataAvailable();
byte *getNextBuffer();
int getNextBufferLen();

bool dataAvailable = false;

// Indicates when motion is detected
static void IRAM_ATTR detectsMovement(void * arg){
  motionDetected = true;
}

void sendPhoto(){
  fb = NULL;
  // Take Picture with Camera
  fb = esp_camera_fb_get();
  if (!fb)
  {
    Serial.println("Camera capture failed");
    bot.sendMessage(chatId, "Camera capture failed", "");
    return;
  }
  dataAvailable = true;
  Serial.println("Sending");
  bot.sendPhotoByBinary(chatId, "image/jpeg", fb->len,
                        isMoreDataAvailable, nullptr,
                        getNextBuffer, getNextBufferLen);
  
  Serial.println("done!");
  
  esp_camera_fb_return(fb);
}

void handleNewMessages(int numNewMessages)
{
  Serial.println("handleNewMessages");
  Serial.println(String(numNewMessages));

  for (int i = 0; i < numNewMessages; i++)
  {
    String chat_id = String(bot.messages[i].chat_id);
    if (chat_id != chatId){
      bot.sendMessage(chat_id, "Unauthorized user", "");
      continue;
    }
    
    String text = bot.messages[i].text;

    String from_name = bot.messages[i].from_name;
    if (from_name == "")
      from_name = "Guest";

    if (text == "/flash")
    {
      flashState = !flashState;
      digitalWrite(FLASH_LED_PIN, flashState);
      bot.sendMessage(chatId, (flashState == LOW) ? "Flash OFF" : "Flash ON", "");
      Serial.println("Toggle flash LED");
    }

    if (text == "/photo")
    {
     sendPhoto();
    }

    if (text == "/start")
    {
      String welcome = "Welcome to the ESP32Cam Telegram bot.\n\n";
      welcome += "/photo : will take a photo\n";
      welcome += "/flash : toggle flash LED (VERY BRIGHT!)\n";
      bot.sendMessage(chat_id, welcome, "Markdown");
    }
  }
}

bool isMoreDataAvailable()
{
  if (dataAvailable)
  {
    dataAvailable = false;
    return true;
  }
  else
  {
    return false;
  }
}

byte *getNextBuffer()
{
  if (fb)
  {
    return fb->buf;
  }
  else
  {
    return nullptr;
  }
}

int getNextBufferLen()
{
  if (fb)
  {
    return fb->len;
  }
  else
  {
    return 0;
  }
}

void setup()
{
  Serial.begin(115200);
  Serial.println();

  pinMode(FLASH_LED_PIN, OUTPUT);
  digitalWrite(FLASH_LED_PIN, flashState); //defaults to low

  if (!setupCamera())
  {
    Serial.println("Camera Setup Failed!");
    while (true)
    {
      delay(100);
    }
  }

  // attempt to connect to Wifi network:
  Serial.print("Connecting to Wifi SSID ");
  Serial.print(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT); // Add root certificate for api.telegram.org
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(500);
  }
  Serial.print("\nWiFi connected. IP address: ");
  Serial.println(WiFi.localIP());

  Serial.print("Retrieving time: ");
  configTime(0, 0, "pool.ntp.org"); // get UTC time via NTP
  time_t now = time(nullptr);
  while (now < 24 * 3600)
  {
    Serial.print(".");
    delay(100);
    now = time(nullptr);
  }
  Serial.println(now);

  // Make the bot wait for a new message for up to 60seconds
  bot.longPoll = 60;

  // PIR Motion Sensor mode INPUT_PULLUP
  //err = gpio_install_isr_service(0); 
  esp_err_t err = gpio_isr_handler_add(GPIO_NUM_13, &detectsMovement, (void *) 13);  
  if (err != ESP_OK){
    Serial.printf("handler add failed with error 0x%x \r\n", err); 
  }
  err = gpio_set_intr_type(GPIO_NUM_13, GPIO_INTR_POSEDGE);
  if (err != ESP_OK){
    Serial.printf("set intr type failed with error 0x%x \r\n", err);
  }
}

void loop()
{
   if(motionDetected){
    bot.sendMessage(chatId, "Motion detected!!", "");
    Serial.println("Motion Detected");
    sendPhoto();
    motionDetected = false;
  }
  if (millis() - bot_lasttime > BOT_MTBS)
  {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

    while (numNewMessages)
    {
      Serial.println("got response");
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }

    bot_lasttime = millis();
  }
}
