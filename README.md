# ArduinoThreadedGSM


Use this library to:

 * Send/Read SMS
 * Read Network Time
 * Read GSM signal level
 
### Quick-start

Initialize modem:

```c++
ThreadedGSM modem(Serial1, 8, 9);
...
void setup()
{
	modem.begin();
	while(!modem.ready()) modem.loop();
}
```
Set automatic intervals for syncing clock and signal levels:
```c++
modem.setInterval(ThreadedGSM::INTERVAL_CLOCK, 60000);
modem.setInterval(ThreadedGSM::INTERVAL_SIGNAL, 60000);
modem.setInterval(ThreadedGSM::INTERVAL_INBOX, 30000);
```
and finally, let the modem loop as a thread in your Arduino loop(), and look for Signal, Clock and Incoming SMS:
```c++
void loop()
{ 
  // "Non-blocking" loop
  modem.loop();
  
  // Read SMS
  String PDU;
  if(modem.readSMS(PDU))
  { /* Incoming SMS */ }
  
  // Network time
  ThreadedGSM::NetworkTime Time;
  if(modem.readClock(Time))
  { /* your code */ }
  
  // Signal results
  ThreadedGSM::SignalLevel Signal;
  if(modem.readSignal(Signal))
  { /* your code */ }
}
```

To send SMS messages, request to send them using sendSMS:
```c++
modem.sendSMS(PDU); // PDU as String, hexadecimal
```
and *always* make sure to check whether message was sent successfuly:
```c++
void loop()
{
	modem.loop();
    ThreadedGSM::SendMessageResult outbox = modem.checkOutbox();
	if(outbox == SMS_SEND_OK)
    { /* message was sent */ }
    else if(outbox == SMS_SEND_FAIL)
    { /* message sending failed */ }
    else { /* keep waiting */ }
}
```
It is also possible to use a blocking code, as follows:
```c++
modem.sendSMS(PDU);
ThreadedGSM::SendMessageResult outbox;
do{
	modem.loop();
    outbox = modem.checkOutbox();
}while(outbox != SMS_SEND_WAIT);
```

Project hosted on GitHub: [https://github.com/neta540/ArduinoThreadedGSM](https://github.com/neta540/ArduinoThreadedGSM).
