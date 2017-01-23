# ArduinoThreadedGSM


Use this library to:

 * Send/Read SMS
 * Read Network Time
 * Read GSM signal level
 
### Quick-start

Initialize modem:

```c++
ThreadedGSM modem(Serial1);
...
void setup()
{
	modem.begin();
}
```
Set automatic intervals for syncing clock and signal levels:
```c++
modem.setInterval(ThreadedGSM::INTERVAL_CLOCK, 60000);
modem.setInterval(ThreadedGSM::INTERVAL_SIGNAL, 60000);
modem.setInterval(ThreadedGSM::INTERVAL_INBOX, 30000);
```
Create your event handlers
```c++
void clock(ThreadedGSM& modem, ThreadedGSM::NetworkTime& Time)
{
	/* Network time */
}

void signal(ThreadedGSM& modem, ThreadedGSM::SignalLevel& Signal)
{
	/* Signal */
}

void sms(ThreadedGSM& modem, String& Msg)
{
	/* Message received in PDU  */
}

void startup(ThreadedGSM& modem)
{
	/* READY */
}
```
Set event handlers for modem events as you like:
```c++
modem.setHandlers({
	.signal = signal,
	.clock = clock,
	.incoming = sms,
	.ready = startup,
	.outgoing = NULL,
	.power = NULL
});
```
and finally, let the modem loop as a thread in your Arduino loop()
```c++
void loop()
{ 
  // "Non-blocking" loop
  modem.loop();
}
```

To send SMS messages, request to send them using sendSMS:
```c++
modem.sendSMS(PDU); // PDU as String, hexadecimal
```
*Don't forget to check whether your messages were sent successfuly, before sending more.*

*Use power event callbacks to set modem pins (EN/RST) on power on/off when needed*


Project hosted on GitHub: [https://github.com/neta540/ArduinoThreadedGSM](https://github.com/neta540/ArduinoThreadedGSM).
