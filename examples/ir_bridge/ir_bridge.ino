// Import libraries (BLEPeripheral depends on SPI)
#include <SPI.h>
#include <BLEPeripheral.h>

// https://github.com/shirriff/Arduino-IRremote
#include <IRremote.h>

// define pins (varies per shield/board)
#define BLE_REQ     9
#define BLE_RDY     8
#define BLE_RST     7

#define IR_SEND_PIN 13 // pin 3 on Uno
#define IR_RECV_PIN 2

#define LED_PIN     3

struct IRValue {
  char type;
  char bits;
  unsigned int address;
  unsigned long value;
};

// create IR send and recv instance, see pinouts above
IRsend                           irSend                      = IRsend(/*IR_SEND_PIN*/);
IRrecv                           irRecv                      = IRrecv(IR_RECV_PIN);
IRValue                          irValue;

// create peripheral instance, see pinouts above
BLEPeripheral                    blePeripheral               = BLEPeripheral(BLE_REQ, BLE_RDY, BLE_RST);

// create service and characteristics
BLEService                       irService                   = BLEService("4952");
BLEFixedLengthCharacteristic     irOutputCharacteristic      = BLEFixedLengthCharacteristic("4953", BLEWrite, sizeof(irValue));
BLEFixedLengthCharacteristic     irInputCharacteristic       = BLEFixedLengthCharacteristic("4954", BLENotify, sizeof(irValue));


void setup() {
  Serial.begin(115200);
#ifdef __AVR_ATmega32U4__
//  while(!Serial);
#endif

  // set advertised local name and service UUID
  blePeripheral.setLocalName("IR");
  blePeripheral.setAdvertisedServiceUuid(irService.uuid());

  // add service and characteristics
  blePeripheral.addAttribute(irService);
  blePeripheral.addAttribute(irOutputCharacteristic);
  blePeripheral.addAttribute(irInputCharacteristic);

   // assign event handlers for connected, disconnected to peripheral
  blePeripheral.setEventHandler(BLEConnected, blePeripheralConnectHandler);
  blePeripheral.setEventHandler(BLEDisconnected, blePeripheralDisconnectHandler);

  // begin initialization
  blePeripheral.begin();

  Serial.println(F("BLE IR Peripheral"));

  // enable the IR receiver
  irRecv.enableIRIn();

  pinMode(LED_PIN, OUTPUT);
}

void loop() {
  // poll peripheral
  blePeripheral.poll();

  // poll the ouput characteristic
  pollIrOutput();

  // poll the IR receiver
  pollIrInput();
}

void blePeripheralConnectHandler(BLECentral& central) {
  // central connected event handler
  digitalWrite(LED_PIN, HIGH);
}

void blePeripheralDisconnectHandler(BLECentral& central) {
  // central disconnected event handler
  digitalWrite(LED_PIN, LOW);
}

void pollIrOutput() {
    // check if central has written to the output value
  if (irOutputCharacteristic.written()) {
    // copy the value written
    memcpy(&irValue, irOutputCharacteristic.value(), sizeof(irValue));

    // extract the data
    char          irOutputType    = irValue.type;
    char          irOutputBits    = irValue.bits;
    unsigned int  irOutputAddress = irValue.address;
    unsigned long irOutputValue   = irValue.value;

    int sendCount;

    // calculate how many times to send the value, depends on the type
    if (irOutputType == SONY || irOutputType == RC5 || irOutputType == RC6) {
      sendCount = 3;
    } else {
      sendCount = 1;
    }

    for (int i = 0; i < sendCount; i++) {
      switch (irOutputType) {
        case NEC:
          irSend.sendNEC(irOutputValue, irOutputBits);
          break;

        case SONY:
          irSend.sendSony(irOutputValue, irOutputBits);
          break;

        case RC5:
          irSend.sendRC5(irOutputValue, irOutputBits);
          break;

        case RC6:
          irSend.sendRC6(irOutputValue, irOutputBits);
          break;

        case DISH:
          irSend.sendDISH(irOutputValue, irOutputBits);
          break;

        case SHARP:
          irSend.sendSharpRaw(irOutputValue, irOutputBits);
          break;

        case PANASONIC:
          irSend.sendPanasonic(irOutputAddress, irOutputValue);
          break;

        case JVC:
          irSend.sendJVC(irOutputValue, irOutputBits, 0);
          break;

        case SAMSUNG:
          irSend.sendSAMSUNG(irOutputValue, irOutputBits);
          break;

        case SANYO:
        case MITSUBISHI:
        case LG:
        default:
          // not implemented
          break;
      }

      delay(40);
    }

    // re-enable the IR receiver
    irRecv.enableIRIn();
  }
}

void pollIrInput() {
  decode_results irDecodeResults;

  // check if IR recv has a result that can be decoded
  if (irRecv.decode(&irDecodeResults)) {

    // must have non-zero number of bits and known decode type
    if (irDecodeResults.bits && irDecodeResults.decode_type != UNKNOWN) {

      // extract the decoded value
      irValue.type = irDecodeResults.decode_type;
      irValue.address = (irValue.type == PANASONIC) ? irDecodeResults.panasonicAddress : 0;
      irValue.bits = irDecodeResults.bits;
      irValue.value = irDecodeResults.value;

      // update the IR characteristic with the result
      irInputCharacteristic.setValue((unsigned char *)&irValue, sizeof(irValue));
    }

    // resume IR receiving
    irRecv.resume();
  }
}
