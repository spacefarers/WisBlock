#include <Arduino.h>
#include <LoRaWan-RAK4630.h>  //http://librarymanager/All#SX126x
#include <Wire.h>
#include <NCP5623.h>	//http://librarymanager/All#NCP5623 By:RAKWireless
#include "TCS3772.h"  // Click here to get the library: http://librarymanager/All#TCS37725
// It use WB_IO2 to power up and is conflicting with INT1, so better use in SlotA/SlotC/SlotD.

NCP5623 rgb;
TCS3772 tcs3772;
TCS3772_DataScaled tcs3772_data = {0};
  
uint16_t scale_factor;

uint16_t redColor;
uint16_t greenColor;
uint16_t blueColor;

#ifdef RAK4630
#define BOARD "RAK4631 "
#define RAK4631_BOARD true
#else
#define RAK4631_BOARD false
#endif

bool doOTAA = true;                                               // OTAA is used by default.
#define SCHED_MAX_EVENT_DATA_SIZE APP_TIMER_SCHED_EVENT_DATA_SIZE /**< Maximum size of scheduler events. */
#define SCHED_QUEUE_SIZE 60                                       /**< Maximum number of events in the scheduler queue. */
#define LORAWAN_DATERATE DR_0                                     /*LoRaMac datarates definition, from DR_0 to DR_5*/
#define LORAWAN_TX_POWER TX_POWER_5                               /*LoRaMac tx power definition, from TX_POWER_0 to TX_POWER_15*/
#define JOINREQ_NBTRIALS 3                                        /**< Number of trials for the join request. */
DeviceClass_t g_CurrentClass = CLASS_A;                           /* class definition*/
LoRaMacRegion_t g_CurrentRegion = LORAMAC_REGION_US915;           /* Region:EU868*/
lmh_confirm g_CurrentConfirm = LMH_CONFIRMED_MSG;                 /* confirm/unconfirm packet definition*/
uint8_t gAppPort = LORAWAN_APP_PORT;                              /* data port*/

/**@brief Structure containing LoRaWan parameters, needed for lmh_init()
*/
static lmh_param_t g_lora_param_init = { LORAWAN_ADR_ON, LORAWAN_DATERATE, LORAWAN_PUBLIC_NETWORK, JOINREQ_NBTRIALS, LORAWAN_TX_POWER, LORAWAN_DUTYCYCLE_OFF };

// Forward declaration
static void lorawan_has_joined_handler(void);
static void lorawan_join_failed_handler(void);
static void lorawan_rx_handler(lmh_app_data_t *app_data);
static void lorawan_confirm_class_handler(DeviceClass_t Class);
static void send_lora_frame(void);

/**@brief Structure containing LoRaWan callback functions, needed for lmh_init()
*/
static lmh_callback_t g_lora_callbacks = { BoardGetBatteryLevel, BoardGetUniqueId, BoardGetRandomSeed,
                                           lorawan_rx_handler, lorawan_has_joined_handler, lorawan_confirm_class_handler, lorawan_join_failed_handler };
//OTAA keys !!!! KEYS ARE MSB !!!!
uint8_t nodeDeviceEUI[8] = { 0xAC, 0x1F, 0x09, 0xFF, 0xFE, 0x05, 0x3E, 0x3E };
uint8_t nodeAppEUI[8] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
uint8_t nodeAppKey[16] = { 0x96, 0xBD, 0xC5, 0x98, 0x17, 0x69, 0x8D, 0xFA, 0x1F, 0x64, 0xFE, 0x1C, 0xF9, 0x26, 0x7F, 0x8D };

// ABP keys
uint32_t nodeDevAddr = 0x260116F8;
uint8_t nodeNwsKey[16] = { 0x7E, 0xAC, 0xE2, 0x55, 0xB8, 0xA5, 0xE2, 0x69, 0x91, 0x51, 0x96, 0x06, 0x47, 0x56, 0x9D, 0x23 };
uint8_t nodeAppsKey[16] = { 0xFB, 0xAC, 0xB6, 0x47, 0xF3, 0x58, 0x45, 0xC7, 0x50, 0x7D, 0xBF, 0x16, 0x8B, 0xA8, 0xC1, 0x7C };

// Private definition
#define LORAWAN_APP_DATA_BUFF_SIZE 64                                            /**< buffer size of the data to be transmitted. */
static uint8_t m_lora_app_data_buffer[LORAWAN_APP_DATA_BUFF_SIZE];               //< Lora user application data buffer.
static lmh_app_data_t m_lora_app_data = { m_lora_app_data_buffer, 0, 0, 0, 0 };  //< Lora user application data structure.

static uint32_t count = 0;
static uint32_t count_fail = 0;

void setup() {

  // Initialize Serial for debug output
	pinMode(LED_BLUE, OUTPUT);
	digitalWrite(LED_BLUE, HIGH);

  // Sensor Power Switch
  pinMode(WB_IO2, OUTPUT);
  digitalWrite(WB_IO2, HIGH);

  // enable RAK14001
  pinMode(WB_IO6, OUTPUT);
  digitalWrite(WB_IO6, HIGH);

  // Initialize LoRa chip.
  lora_rak4630_init();

  // Initialize Serial for debug output
  time_t timeout = millis();
  Serial.begin(115200);
  while (!Serial) {
    if ((millis() - timeout) < 5000) {
      delay(100);
    } else {
      break;
    }
  }
  Serial.println("=====================================");
  Serial.println("Welcome to RAK4630 LoRaWan!!!");
  if (doOTAA) {
    Serial.println("Type: OTAA");
  } else {
    Serial.println("Type: ABP");
  }

  switch (g_CurrentRegion) {
    case LORAMAC_REGION_AS923:
      Serial.println("Region: AS923");
      break;
    case LORAMAC_REGION_AU915:
      Serial.println("Region: AU915");
      break;
    case LORAMAC_REGION_CN470:
      Serial.println("Region: CN470");
      break;
    case LORAMAC_REGION_EU433:
      Serial.println("Region: EU433");
      break;
    case LORAMAC_REGION_IN865:
      Serial.println("Region: IN865");
      break;
    case LORAMAC_REGION_EU868:
      Serial.println("Region: EU868");
      break;
    case LORAMAC_REGION_KR920:
      Serial.println("Region: KR920");
      break;
    case LORAMAC_REGION_US915:
      Serial.println("Region: US915");
      break;
  }
  Serial.println("=====================================");

  // Setup the EUIs and Keys
  if (doOTAA) {
    lmh_setDevEui(nodeDeviceEUI);
    lmh_setAppEui(nodeAppEUI);
    lmh_setAppKey(nodeAppKey);
  } else {
    lmh_setNwkSKey(nodeNwsKey);
    lmh_setAppSKey(nodeAppsKey);
    lmh_setDevAddr(nodeDevAddr);
  }

  uint32_t err_code;
  // Initialize LoRaWan
  err_code = lmh_init(&g_lora_callbacks, g_lora_param_init, doOTAA, g_CurrentClass, g_CurrentRegion);
  if (err_code != 0) {
    Serial.printf("lmh_init failed - %d\n", err_code);
    return;
  }

  // Start Join procedure
  lmh_join();

  Serial.println("================================");
  Serial.println("RAK12021 + RAK14001 LoRaWAN Code");
  Serial.println("================================");

  // If using Native I2C
  Wire.begin();
  Wire.setClock(100000);

  // Serial.println("RAK14001 + RAK12021");

  if (!rgb.begin())
  {
    Serial.println("RAK14001 not found on the I2C line");
    while (1);
  }
  else
  {
    Serial.println("RAK14001 Found. Begining execution");
  }

  // set the current output level max, the range is 1 to 31
  rgb.setCurrent(25);

  if(tcs3772.begin() == true)
  {
    Serial.println("Found sensor.");
  }
  else
  {
    Serial.println("TCS37725 not found ... check your connections.");
    while(1)
    {
      delay(10);  
    }
  }
  delay(1000);
}

void loop() {
  // Put your application tasks here, like reading of sensors,
  // Controlling actuators and/or other functions.

  tcs3772_data = tcs3772.getMeasurement();

  scale_factor = tcs3772.autoGain(tcs3772_data.clear);

  redColor = tcs3772_data.red;
  greenColor = tcs3772_data.green;
  blueColor = tcs3772_data.blue;

  rgb.setColor(0,0,0);     // Initially OFF

  Serial.println("Sending frame now...");
  send_lora_frame();

  Serial.print("  R: ");
  Serial.println(redColor);
  Serial.print("  G: ");
  Serial.println(greenColor);
  Serial.print("  B: ");
  Serial.println(blueColor);

  // The values of redColor, greenColor and blueColor can be varied during the sensing calibration of RAK12021 module

  if (((redColor >= 9000) && (redColor <= 65535)) && ((greenColor >= 10000) && (greenColor <= 65535)) && ((blueColor >= 12000) && (blueColor <= 65535)))
  {
    rgb.setColor(255,255,255); // WHITE
  }

  else if (((redColor >= 4000) && (redColor <= 18000)) && ((greenColor >= 1300) && (greenColor <= 5000)) && ((blueColor >= 950) && (blueColor <= 2000)))
  {
    rgb.setColor(255,255,0);  // YELLOW
  }

  else if (((redColor >= 600) && (redColor <= 2000)) && ((greenColor >= 1700) && (greenColor <= 10000)) && ((blueColor >= 6800) && (blueColor <= 27000)))
  {
    rgb.setColor(0,0,255);    // BLUE
  }

  else if (((redColor >= 1400) && (redColor <= 4000)) && ((greenColor >= 1300) && (greenColor <= 10000)) && ((blueColor >= 950) && (blueColor <= 2700)))
  {
    rgb.setColor(0,255,0);    // GREEN
  }

  else if (((redColor >= 3000) && (redColor <= 20000)) && ((greenColor >= 700) && (greenColor <= 2400)) && ((blueColor >= 950) && (blueColor <= 3000)))
  {
    rgb.setColor(255,0,0);    // RED
  }

  else if ((redColor < 1500) && (greenColor < 1400) && (blueColor < 900))
  {
    rgb.setColor(0,0,0);  // OFF
  }
  
  delay(5000);
}

/**@brief LoRa function for handling HasJoined event.
 */
void lorawan_has_joined_handler(void) {
  Serial.println("OTAA Mode, Network Joined!");
}

/**@brief LoRa function for handling OTAA join failed
*/
static void lorawan_join_failed_handler(void) {
  Serial.println("OTAA join failed!");
  Serial.println("Check your EUI's and Keys's!");
  Serial.println("Check if a Gateway is in range!");
}
/**@brief Function for handling LoRaWan received data from Gateway
 *
 * @param[in] app_data  Pointer to rx data
 */
void lorawan_rx_handler(lmh_app_data_t *app_data) {
  Serial.printf("LoRa Packet received on port %d, size:%d, rssi:%d, snr:%d, data:%s\n",
                app_data->port, app_data->buffsize, app_data->rssi, app_data->snr, app_data->buffer);
}

void lorawan_confirm_class_handler(DeviceClass_t Class) {
  Serial.printf("switch to class %c done\n", "ABC"[Class]);
  // Informs the server that switch has occurred ASAP
  m_lora_app_data.buffsize = 0;
  m_lora_app_data.port = gAppPort;
  lmh_send(&m_lora_app_data, g_CurrentConfirm);
}

String data = "";

void send_lora_frame(void) {
  if (lmh_join_status_get() != LMH_SET) {
    //Not joined, try again later
    return;
  }

  Serial.print("result: ");
  uint32_t i = 0;
  memset(m_lora_app_data.buffer, 0, LORAWAN_APP_DATA_BUFF_SIZE);
  m_lora_app_data.port = gAppPort;

  // Showing the values of red, green and blue colors from RAK12021
  data = " Red: " + String(redColor) + " Green: " + String(greenColor) + " Blue: " + String(blueColor);

  Serial.println(data);

  uint16_t red_data = redColor;
  uint16_t green_data = greenColor;
  uint16_t blue_data = blueColor;

  m_lora_app_data.buffer[i++] = 0x01;  // byte[0]

  // Red data
  m_lora_app_data.buffer[i++] = (uint8_t)((red_data & 0x0000FF00) >> 8);     // byte[1] 
  m_lora_app_data.buffer[i++] = (uint8_t)(red_data & 0x000000FF);            // byte[2] 

  // Green data
  m_lora_app_data.buffer[i++] = (uint8_t)((green_data & 0x0000FF00) >> 8);   // byte[3] 
  m_lora_app_data.buffer[i++] = (uint8_t)(green_data & 0x000000FF);          // byte[4] 
 
  // Blue data
  m_lora_app_data.buffer[i++] = (uint8_t)((blue_data & 0x0000FF00) >> 8);    // byte[5] 
  m_lora_app_data.buffer[i++] = (uint8_t)(blue_data & 0x000000FF);           // byte[6] 
  
  m_lora_app_data.buffsize = i;

  lmh_error_status error = lmh_send(&m_lora_app_data, g_CurrentConfirm);
  if (error == LMH_SUCCESS) {
    count++;
    Serial.printf("lmh_send ok count %d\n", count);
  } else {
    count_fail++;
    Serial.printf("lmh_send fail count %d\n", count_fail);
  }
}