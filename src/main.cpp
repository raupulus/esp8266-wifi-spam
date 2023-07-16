#include <Arduino.h>
#include <SPI.h>
#include <ESP8266WiFi.h>
#include <ssids.h>
#include <vars.h>
#include <beacon.h>

const bool DEBUG = false;

const uint8_t channels[] = {2, 4, 10};

const bool cleanSsids = true; // Indica si limpiar los nombres de redes, 32 chars max y espacios.
const bool wpa = true;
const bool wpa2 = false;

//
extern "C"
{
#include "user_interface.h"
  typedef void (*freedom_outside_cb_t)(uint8 status);
  int wifi_register_send_pkt_freedom_cb(freedom_outside_cb_t cb);
  void wifi_unregister_send_pkt_freedom_cb(void);
  int wifi_send_pkt_freedom(uint8 *buf, int len, bool sys_seq);
}

//

// Almacena las direcciones macs para todos los SSID
String macs[5]; // TOFIX: Contemplar que es variable desde ssids

void generateRandomMac()
{
  int length = sizeof(macs);

  for (int i = 0; i < length; i++)
  {
    String newMac = String(random(256), HEX) +
                    ":" + String(random(256), HEX) +
                    ":" + String(random(256), HEX) +
                    ":" + String(random(256), HEX);

    Serial.print("Nueva Mac: ");
    Serial.println(newMac);

    macs[i] = newMac;
  }

  // Dirección MAC principal
  for (int i = 0; i < 6; i++)
  {
    macAddr[i] = random(256);
  }
}

/**
 * Cambia el canal de trabajo.
 */
void changeChannel()
{
  if (sizeof(channels) > 1)
  {
    uint8_t ch = channels[channelIndex];

    channelIndex++;

    if (channelIndex > sizeof(channels))
    {
      channelIndex = 0;
    }

    if (ch != wifi_channel && ch >= 1 && ch <= 14)
    {
      wifi_channel = ch;
      wifi_set_channel(wifi_channel);
    }
  }
}

void setup()
{
  if (DEBUG)
  {
    Serial.begin(115200);
    Serial.println();
  }

  // generateRandomMac();

  randomSeed(os_random());

  packetSize = sizeof(beaconPacket);

  if (wpa)
  {
    beaconPacket[34] = 0x21;
    packetSize -= 26;
  }
  else if (wpa2)
  {
    beaconPacket[34] = 0x31;
  }

  // Tiempo desde encendido
  currentTime = millis();

  // Iniciando WiFi
  WiFi.mode(WIFI_OFF);           // Detiene el Wifi
  wifi_set_opmode(STATION_MODE); // Establece modo AP

  // Set to default WiFi channel
  wifi_set_channel(channels[0]); // Cambia al primer canal del array

  // Muestro todos los SSIDs
  if (DEBUG)
  {
    Serial.println("SSIDs:");

    int i = 0;
    int len = sizeof(ssids);

    while (i < len)
    {
      if (DEBUG)
      {
        Serial.print((char)pgm_read_byte(ssids + i));
      }

      i++;
    }

    Serial.println();
    Serial.println("Started \\o/");
    Serial.println();
  }
}

void loop()
{
  currentTime = millis();

  // send out SSIDs
  if (currentTime - attackTime > 100)
  {
    attackTime = currentTime;

    // temp variables
    int i = 0;
    int j = 0;
    int ssidNum = 1;
    char tmp;
    int ssidsLen = strlen_P(ssids);
    bool sent = false;

    // Cambiar canal de trabajo
    changeChannel();

    while (i < ssidsLen)
    {
      // Get the next SSID
      j = 0;
      do
      {
        if (DEBUG)
        {
          Serial.println('______NEXT SSID______');
        }

        tmp = pgm_read_byte(ssids + i + j);
        j++;
      } while (tmp != '\n' && j <= 32 && i + j < ssidsLen);

      uint8_t ssidLen = j - 1;

      // set MAC address
      macAddr[5] = ssidNum;
      ssidNum++;

      // write MAC address into beacon frame
      memcpy(&beaconPacket[10], macAddr, 6);
      memcpy(&beaconPacket[16], macAddr, 6);

      // reset SSID
      memcpy(&beaconPacket[38], emptySSID, 32);

      // write new SSID into beacon frame
      memcpy_P(&beaconPacket[38], &ssids[i], ssidLen);

      // set channel for beacon frame
      beaconPacket[82] = wifi_channel;

      // send packet
      if (cleanSsids)
      {
        for (int k = 0; k < 3; k++)
        {
          packetCounter += wifi_send_pkt_freedom(beaconPacket, packetSize, 0) == 0;
          delay(1);
        }
      }

      // remove spaces
      else
      {

        uint16_t tmpPacketSize = (packetSize - 32) + ssidLen;                // calc size
        uint8_t *tmpPacket = new uint8_t[tmpPacketSize];                     // create packet buffer
        memcpy(&tmpPacket[0], &beaconPacket[0], 38 + ssidLen);               // copy first half of packet into buffer
        tmpPacket[37] = ssidLen;                                             // update SSID length byte
        memcpy(&tmpPacket[38 + ssidLen], &beaconPacket[70], wpa2 ? 39 : 13); // copy second half of packet into buffer

        // send packet
        for (int k = 0; k < 3; k++)
        {
          packetCounter += wifi_send_pkt_freedom(tmpPacket, tmpPacketSize, 0) == 0;
          delay(1);
        }

        delete tmpPacket; // Libera buffer de memoria
      }

      i += j;
    }
  }

  // Cálculo de paquetes por segundos
  if (currentTime - packetRateTime > 1000)
  {
    packetRateTime = currentTime;

    if (DEBUG)
    {

      Serial.print("Packets/s: ");
      Serial.println(packetCounter);
    }

    packetCounter = 0;
  }
}
