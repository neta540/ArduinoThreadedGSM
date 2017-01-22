/**
 * ThreadedGSM Example: Read SMS, Network Time and Signal level
 * Reads SMS messages in PDU mode and prints them to debug serial
 */
#include <Arduino.h>
#define SerialDebug Serial1
#define SerialModem Serial
#define THREADEDGSM_DEBUG SerialDebug
#include <ThreadedGSM.h>

// GSM modem pins
#define GSM_EN 9
#define GSM_RST 8

ThreadedGSM modem(SerialModem, GSM_EN, GSM_RST);

void setup()
{
  SerialDebug.begin(115200);
  SerialModem.begin(115200);

  SerialDebug.println("STARTUP\r\n");

  pinMode(GSM_EN, OUTPUT);
  pinMode(GSM_RST, OUTPUT);
  modem.begin();
  modem.setInterval(ThreadedGSM::INTERVAL_CLOCK, 60000);
  modem.setInterval(ThreadedGSM::INTERVAL_SIGNAL, 60000);
  modem.setInterval(ThreadedGSM::INTERVAL_INBOX, 30000);
  // Let modem start up
  while(!modem.ready()) modem.loop();
}

void loop()
{
  String PDU;
  ThreadedGSM::SignalLevel Signal;
  ThreadedGSM::NetworkTime Time;
  
  modem.loop();
  
  if(modem.readSignal(Signal))
  {
    SerialDebug.print("Modem signal: Dbm:");
    SerialDebug.print(Signal.Dbm);
	SerialDebug.print(" value: ");
	SerialDebug.println(Signal.Value);
  }
  
  if(modem.readClock(Time))
  {
    SerialDebug.print("Modem Time: ");
    SerialDebug.print(Time.day);
    SerialDebug.print("/");
    SerialDebug.print(Time.month);
    SerialDebug.print("/");
    SerialDebug.print(Time.year);
    SerialDebug.print(" ");
    SerialDebug.print(Time.hour);
    SerialDebug.print(":");
    SerialDebug.print(Time.minute);
    SerialDebug.print(":");
    SerialDebug.println(Time.second);
  }
  if(modem.readSMS(PDU))
  {
    SerialDebug.print("Received SMS: ");
    SerialDebug.println(PDU);
  }
}
