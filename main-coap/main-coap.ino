#include <RF24Network.h>
#include <RF24.h>
#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <CoapMessage.h>
#include <CoapUtils.h>
#include <DebugUtils.h>

#include "CoapResource.h"

#define MAX_BUFFER 60

#define ACK_TIMEOUT 2
#define ACK_RANDOM_FACTOR 1.5
#define MAX_RETRANSMIT 4

byte mac[] = {0x00, 0xaa, 0xbb, 0xcc, 0xde, 0xf8};
byte ip [] = {192, 168, 0, 25};
short localPort = 5683;
uint8_t OUR_CHANNEL = 120;
uint8_t THIS_NODE = 00;
char packetBuffer[MAX_BUFFER];
int blockNumber = 0;

unsigned long currentTime = 0;
unsigned long previousTime = 0;

int sendCon = 0;
int recAck = 0;
int observersNumber = 0;

enum option {LampOff, LampOn, LampStatus, PotStatus};

struct Request {
  short option;             //0 - lampka off, 1 - lampka ON, 2 - stan lampki, 3 - stan potencjometru
};

struct Response {
  short option;             //0 - lampka off, 1 - lampka ON, 2 - stan lampki, 3 - stan potencjometru
  short value;
};

struct observer
{
  bool isEmpty;
  IPAddress remoteAddress;
  unsigned char * token;
  int tokenLength;
  int remotePort;
  int sequenceNumber;
};

EthernetUDP Udp;
RF24 radio(7, 8);
RF24Network network(radio);

struct observer observersList[10];            // 10 observerów może być

void setup() {
  Serial.begin(115200);
  SPI.begin();
  Ethernet.begin(mac);
  radio.begin();
  network.begin(OUR_CHANNEL, THIS_NODE);
  Udp.begin(localPort);
  
  Serial.println(Ethernet.localIP());
  initializeObserversList(observersList);
}


void loop() {


  //==============UDP COAP COMMUNICATION =================

  //nasłuchuje na pakiety udp coapa
  int packetSize = Udp.parsePacket();

  //gdy przyszedł pakiet
  if (packetSize) {


    CoapMessage * coapMessage = new CoapMessage();
    unsigned char * packet = new unsigned char[packetSize];

    Udp.read(packetBuffer, MAX_BUFFER); //wczytanie pakietu do bufora
    memcpy(packet, packetBuffer, packetSize);  //skopiowanie do zmiennej packet
   // printPacketInHex(packet, packetSize);
    
    coapMessage->parse(packet, packetSize, Udp.remoteIP(), Udp.remotePort());// "wypełnienie obiektu CoapMessage" danymi pakietu
    delete packet;
    

    if (coapMessage->getCodeDetails() == CoapUtils::RequestMethod::GET) {
      //Serial.println("GET");
      handleGetRequest(*coapMessage); //IN MAIN-COAP-GET-HANDLING.INO FILE :)
    }

    if (coapMessage->getCodeDetails() == CoapUtils::RequestMethod::PUT) {
      handlePutRequest(*coapMessage);//IN MAIN-COAP-PUT-HANDLING.INO FILE :)
    }
    delete coapMessage;
  }


  //===========RADIO COMMUNICATION =======================
  // Nasłuchuje na wiadomości radiowe od CoAP-resource (poziom potencjometru, status lampki)
  network.update();
  while (network.available()) {
     //  Serial.println("Odebrano");
    struct Response message;
    RF24NetworkHeader header;
    
    network.read(header, &message, sizeof(struct Response));
      // Serial.println(message.value, DEC);

     handleRadioRequest(message.option, message.value);  // W MAIN-COAP-RADIO-COMMUNICATION.INO FILE :)
  }
}


//========== UTILS =========================================


void initializeObserversList(struct observer observers[]) {
  for (int i = 0; i < 10; i++) {
    observers[i].isEmpty = true;
    observers[i].remoteAddress = IPAddress(0, 0, 0, 0);
    observers[i].token = nullptr;
    observers[i].tokenLength = 0;
    observers[i].remotePort = 0;
    observers[i].sequenceNumber = 2;
  }
}

void debugPayload(unsigned char* payload, uint8_t length ) {
  Serial.print("payload=");

  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
}
