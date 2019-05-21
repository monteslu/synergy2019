

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#include <Wire.h>
#include "SSD1306Wire.h" // legacy include: `#include "SSD1306.h"`
// #include <Firmata.h>

// from Firmata.h until the board gets added
#define PIN_MODE_INPUT          0x00
#define PIN_MODE_OUTPUT         0x01
#define PIN_MODE_ANALOG         0x02 // analog pin in analogInput mode
#define PIN_MODE_PWM            0x03 // digital pin in PWM output mode
#define PIN_MODE_SERVO          0x04 // digital pin in Servo output mode
#define PIN_MODE_SHIFT          0x05 // shiftIn/shiftOut mode
#define PIN_MODE_I2C            0x06 // pin included in I2C setup
#define PIN_MODE_ONEWIRE        0x07 // pin configured for 1-wire
#define PIN_MODE_STEPPER        0x08 // pin configured for stepper motor
#define PIN_MODE_ENCODER        0x09 // pin configured for rotary encoders
#define PIN_MODE_SERIAL         0x0A // pin configured for serial communication
#define PIN_MODE_PULLUP         0x0B // enable internal pull-up resistor for pin
#define PIN_MODE_IGNORE         0x7F // pin configured to be ignored by digitalWrite and capabilityResponse
#define REPORT_ANALOG           0xC0 // enable analog input by pin #
#define REPORT_DIGITAL          0xD0 // enable digital input by port pair
#define SET_PIN_MODE            0xF4 // set a pin to INPUT/OUTPUT/PWM/etc
#define SAMPLING_INTERVAL       0x7A // set the poll rate of the main loop
#define SERVO_CONFIG            0x70 // set max angle, minPulse, maxPulse, freq

#define TOTAL_PINS                   40  //just a guess
#define MAX_SERVOS                   40  // i have no idea

#define lowByte(w) ((uint8_t) ((w) & 0xff))
#define highByte(w) ((uint8_t) ((w) >> 8))


#define I2C_WRITE                   B00000000
#define I2C_READ                    B00001000
#define I2C_READ_CONTINUOUSLY       B00010000
#define I2C_STOP_READING            B00011000
#define I2C_READ_WRITE_MODE_MASK    B00011000
#define I2C_10BIT_ADDRESS_MODE_MASK B00100000
#define I2C_END_TX_MASK             B01000000
#define I2C_STOP_TX                 1
#define I2C_RESTART_TX              0
#define I2C_MAX_QUERIES             8
#define I2C_REGISTER_NOT_SPECIFIED  -1loo

#define PIXEL_OFF               0x00 // set strip to be off
#define PIXEL_CONFIG            0x01 // set pin, length
#define PIXEL_SHOW              0x02 // latch the pixels and show them
#define PIXEL_SET_PIXEL         0x03 // set the color value of pixel n using 32bit packed color value
#define PIXEL_SET_STRIP         0x04 // set color of whole strip
#define PIXEL_SHIFT             0x05 // shift all pixels n places along the strip
#define IMU_TOGGLE              0x20 // turn on or off the IMU

#define SERVICE_UUID        "bada5555-e91f-1337-a49b-8675309fb099"

char macString[] = "abcd";

BLECharacteristic *digitalChar;
BLECharacteristic *analogChar;
BLECharacteristic *configChar;

bool deviceConnected = false;


// Initialize the OLED display using Wire library
SSD1306Wire  display(0x3c, 5, 4);

int fontSize = 16;
int drawX = 10;
int drawY = 30;

int tick = 0;


struct PinState {
  uint8_t mode;
  uint16_t value;
  bool reportDigital;
  bool reportAnalog;
  uint8_t ledcChannel;
};

uint8_t ledcChannels = 0;

uint16_t samplingInterval = 333;      // how often (milliseconds) to report analog data
long currentMillis;     // store the current value from millis()
long previousMillis;    // for comparison with currentMillis

uint16_t imuSamplingInterval = 500;      // how often (milliseconds) to report analog data
long imuCurrentMillis;     // store the current value from millis()
long imuPreviousMillis;    // for comparison with currentMillis


/* i2c data */
struct i2c_device_info {
  byte addr;
  int reg;
  byte bytes;
  byte stopTX;
};

/* for i2c read continuous more */
i2c_device_info query[I2C_MAX_QUERIES];

byte i2cRxData[64];
boolean isI2CEnabled = false;
signed char queryIndex = -1;
// default delay time between i2c read request and Wire.requestFrom()
unsigned int i2cReadDelayTime = 0;


PinState states[TOTAL_PINS];

//until supported by firmata:
int getPinMode(byte pin) {
  Serial.print("getPinMode ");
  Serial.print(pin);
  Serial.print(" ");
  if(pin < TOTAL_PINS) {
    Serial.println(states[pin].mode);
    return states[pin].mode;
  }
  Serial.println(-1);
  return -1;
}

//until supported by esp32-arduino
void analogWrite(byte pin, byte value) {
  
}


class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      Serial.println("device connected...");
      //digitalWrite(5, LOW);
      //writeOled("connected...");
      //writeOled("connected");
      fillRect();
      deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
      Serial.println("device disconnected...");
      //digitalWrite(5, HIGH);
      writeOled(macString);
      deviceConnected = false;
    };

};



class DigitalCallback: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string value = pCharacteristic->getValue();

      
      uint8_t len = value.length();
    
      for (int i=0; i < (len / 2); i++) {
    
        Serial.print("digitalWrite pin: ");
        uint8_t p = value[(i*2)+0];
        Serial.println(p);
        uint8_t v= value[(i*2)+1];
    
//        if(IS_PIN_DIGITAL(p)){
          if(v > 0){
            Serial.print("writing: ");
            Serial.println(HIGH);
            digitalWrite(p, HIGH);
          }
          else{
            Serial.print("writing: ");
            Serial.println(LOW);
            digitalWrite(p, LOW);
          }
          Serial.print("value written: ");
          Serial.println(v);
//        }
    
    
      }

    }
};

class AnalogCallback: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string value = pCharacteristic->getValue();

      Serial.print("analogWrite bytes: ");
      uint8_t len = value.length();
      uint8_t loopLen;
      Serial.println(len);
    
      //do at least 1 loop if only 2 bytes are sent
      if(len == 2){
        loopLen = 3;
      }
      else{
        loopLen = len;
      }
      
      for (int i=0; i < (loopLen / 3); i++) {
    
        uint8_t p = value[(i*3)+0];
        uint8_t v1= value[(i*3)+1];
        uint8_t v2 = value[(i*3)+2];
      
        Serial.print("p: ");
        Serial.println(p);
        Serial.print("v1: ");
        Serial.println(v1);
        Serial.print("v2: ");
        Serial.println(v2);
      
      
        if (p < TOTAL_PINS) {
          uint16_t val = 0;
          val+= v1;
          if(len > 2){
            val+= v2 << 8;
          }
          Serial.println("pwm/servo pin mode ");
//          Serial.println(Firmata.getPinMode(p));
      
          if(getPinMode(p) == PIN_MODE_PWM){
//            analogWrite(PIN_TO_PWM(p), val);
//            analogWrite(p, val);
            ledcWrite(states[p].ledcChannel, val);
            
            
            Serial.print("pwm wrote channel:");
            Serial.print(states[p].ledcChannel);
            Serial.print(" val:");
            Serial.println(val);
          }
//          Firmata.setPinState(p, val);
        }
      }


      

    }
};


void setPinMode(byte pin, int mode)
{

  Serial.print("setPinModeCallback: ");
  Serial.println(pin);
  Serial.print("mode: ");
  Serial.println(mode);

  switch (mode) {
    case PIN_MODE_ANALOG:
        pinMode(pin, INPUT);
        states[pin].mode = mode;
      break;
    case PIN_MODE_INPUT:
        pinMode(pin, INPUT); //PIN_TO_DIGITAL(pin), INPUT);    // disable output driver
        states[pin].mode = mode;
      break;
    case PIN_MODE_PULLUP:
        pinMode(pin, INPUT_PULLUP); //PIN_TO_DIGITAL(pin), INPUT_PULLUP);
        states[pin].mode = mode;
      break;
    case PIN_MODE_OUTPUT:
        pinMode(pin, OUTPUT);
        states[pin].mode = mode;
      break;
    case PIN_MODE_PWM:
      pinMode(pin, OUTPUT);
      if(states[pin].ledcChannel == 255) {
        states[pin].ledcChannel = ledcChannels;
        ledcChannels++;
        ledcAttachPin(pin, states[pin].ledcChannel);
        ledcSetup(states[pin].ledcChannel, 12000, 8);
      }
      states[pin].mode = mode;
      break;
    case PIN_MODE_I2C:
//      if (IS_PIN_I2C(pin)) {
        // mark the pin as i2c
        // the user must call I2C_CONFIG to enable I2C for a device
//        Firmata.setPinMode(pin, PIN_MODE_I2C);
//      }
      break;

  }
}


class ConfigCallback: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string value = pCharacteristic->getValue();

      Serial.print("configWrite bytes: ");
      uint8_t len = value.length();
      Serial.println(len);
      uint8_t cmd = value[0];
      uint8_t p = value[1];
      uint8_t val = value[2];
      
    
      Serial.print("cmd: ");
      Serial.println(cmd);
    
      Serial.print("p: ");
      Serial.println(p);
    
      Serial.print("val: ");
      Serial.println(val);
    
      switch (cmd) {
        case SET_PIN_MODE:
          if(p < TOTAL_PINS) {
//            Serial.print("SET_PIN_MODE: ");
//            Serial.println(p);
            setPinMode(p, val);
          }

          break;
          
        case REPORT_ANALOG:
          reportAnalogCallback(p, val);
          break;
    
        case REPORT_DIGITAL:
          Serial.println("REPORT_DIGITAL");
          if(true){ //IS_PIN_DIGITAL(p)){
            states[p].reportDigital = (val == 0) ? 0 : 1;
            states[p].reportAnalog = !states[p].reportDigital;
            Serial.print("report digital setting: ");
            Serial.println(states[p].reportDigital);
            Serial.println("REPORT_DIGITAL END");
          }else{
            Serial.print("pin NOT digital: ");
            Serial.println(p);
          }
          break;
    
        case SAMPLING_INTERVAL:
          Serial.print("SAMPLING_INTERVAL start ");
          Serial.println(samplingInterval);
          samplingInterval = (uint16_t) p;
          if(len > 2){
            samplingInterval+= val << 8;
          }
          Serial.print("SAMPLING_INTERVAL end ");
          Serial.println(samplingInterval);
          break;
         
      }
      
    }
};

void writeOled(String message) {

  display.clear();
  display.display();

  display.setTextAlignment(TEXT_ALIGN_LEFT);
  if (fontSize == 10) {
    display.setFont(ArialMT_Plain_10);
  }
  else if (fontSize == 16) {
    display.setFont(ArialMT_Plain_16);
  }
  else if (fontSize == 24) {
    display.setFont(ArialMT_Plain_24);
  }

  display.drawStringMaxWidth(drawX, drawY, 128, message);
  display.display();
}



void setup() {
  Serial.begin(115200);

  uint8_t chipid[6];
  esp_efuse_read_mac(chipid);
  sprintf(macString, "%02x%02x", chipid[4], chipid[5]);
  Serial.printf("%02x:%02x:%02x:%02x:%02x:%02x\n", chipid[0], chipid[1], chipid[2], chipid[3], chipid[4], chipid[5]);

  for (int i=0; i < TOTAL_PINS; i++) {
    states[i].value = 0;
    states[i].reportAnalog = 0;
    states[i].reportDigital = 0;
    states[i].ledcChannel = 255; //max because zero is a valid channel
    
  }

  




  // FOR LOLIN32 NODEBOTS!
//  setPinMode(19, PIN_MODE_PWM);
//  setPinMode(23, PIN_MODE_OUTPUT);
//  setPinMode(18, PIN_MODE_OUTPUT);
//
//  setPinMode(17, PIN_MODE_OUTPUT);
//  setPinMode(16, PIN_MODE_OUTPUT);
//  setPinMode(4, PIN_MODE_PWM);


  // FOR LOLIN32 OLED NODEBOTS!
  setPinMode(0, PIN_MODE_PWM);
  setPinMode(2, PIN_MODE_OUTPUT);
  setPinMode(14, PIN_MODE_OUTPUT);

  setPinMode(12, PIN_MODE_OUTPUT);
  setPinMode(13, PIN_MODE_OUTPUT);
  setPinMode(15, PIN_MODE_PWM);
  
 // setPinMode(5, PIN_MODE_OUTPUT);
 // digitalWrite(5, HIGH);

  
 

  // Create the BLE Device
  // keep name 5 characters or less:
  BLEDevice::init(macString);

  // Create the BLE Server
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  digitalChar = pService->createCharacteristic(
                  BLEUUID((uint16_t)0x2A56),
                  BLECharacteristic::PROPERTY_READ   |
                  BLECharacteristic::PROPERTY_WRITE  |
                  BLECharacteristic::PROPERTY_WRITE_NR  |
                  BLECharacteristic::PROPERTY_NOTIFY |
                  BLECharacteristic::PROPERTY_INDICATE
                );
                    
  analogChar = pService->createCharacteristic(
                BLEUUID((uint16_t)0x2A58),
                BLECharacteristic::PROPERTY_READ   |
                BLECharacteristic::PROPERTY_WRITE  |
                BLECharacteristic::PROPERTY_WRITE_NR  |
                BLECharacteristic::PROPERTY_NOTIFY |
                BLECharacteristic::PROPERTY_INDICATE
              );
              
  configChar = pService->createCharacteristic(
                BLEUUID((uint16_t)0x2A59),
                BLECharacteristic::PROPERTY_READ   |
                BLECharacteristic::PROPERTY_WRITE  |
                BLECharacteristic::PROPERTY_WRITE_NR
              );


  analogChar->addDescriptor(new BLE2902());
  digitalChar->addDescriptor(new BLE2902());
  configChar->addDescriptor(new BLE2902());
  
  analogChar->setCallbacks(new AnalogCallback());
  digitalChar->setCallbacks(new DigitalCallback());
  configChar->setCallbacks(new ConfigCallback());

  // Start the service
  pServer->getAdvertising()->addServiceUUID(SERVICE_UUID);
  pService->start();

  // Start advertising
  pServer->getAdvertising()->start();
  Serial.println("Waiting a client connection to notify...");

  // Initialising the UI will init the display too.
  display.init();

  display.flipScreenVertically();
  //display.setFont(ArialMT_Plain_10);

  writeOled(macString);
  
  currentMillis = millis();
}

void fillRect() {

    display.clear();
    display.drawRect(12, 12, 100, 100);

    // Fill the rectangle
    display.fillRect(14, 14, 87, 87);

    display.display();
}

void loop() {

  bool notify = 0;


  currentMillis = millis();
  if (currentMillis - previousMillis > samplingInterval) {
    notify = 1;
    previousMillis += samplingInterval;
//    Serial.println("notify");
  }



  for (int i=0; i < TOTAL_PINS; i++) {
    if(states[i].reportDigital){
      int val = digitalRead(i);
      if(states[i].value != val) {
        unsigned char report[2] = {(unsigned char)i, (unsigned char)val};
        digitalChar->setValue(report, 2);
        digitalChar->notify();
        delay(5);
      }
      states[i].value = val;
    }

    if(notify == 1 && states[i].reportAnalog == 1){
      Serial.print("notify analog: ");
      int val = analogRead(i); //PIN_TO_ANALOG(i));
      Serial.println((int) val);
      unsigned char report[3] = {(unsigned char)i, lowByte(val), highByte(val)};
      analogChar->setValue(report, 3);
      analogChar->notify();
      delay(5);

    }

  }


}




/*==============================================================================
 * FUNCTIONS
 *============================================================================*/


void reportAnalogCallback(byte p, int val)
{
    Serial.println("REPORT_ANALOG");
//    if(IS_PIN_ANALOG(p)){
      Serial.print("current report analog setting: ");
      Serial.println(states[p].reportAnalog);
      states[p].reportAnalog = (val == 0) ? 0 : 1;
      states[p].reportDigital = !states[p].reportAnalog;
      Serial.print("report analog setting: ");
      Serial.println(states[p].reportAnalog);
//    }else{
//      Serial.print("pin NOT analog: ");
//      Serial.println(p);
//    }
    Serial.println("REPORT_ANALOG END");
}
