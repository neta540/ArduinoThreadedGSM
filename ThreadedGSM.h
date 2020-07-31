/*
* ThreadedGSM.h
*
* Created: 20/09/2016 11:14:02
* Author: Neta Yahav
* Modified: 10/05/2020 By ChoN
*/


#ifndef __THREADEDGSM_H__
#define __THREADEDGSM_H__

#include <Arduino.h>
#include "DTE.h"

// Defaults
#define THREADEDGSM_DEF_DTE_BUF_SIZ		512
#define THREADEDGSM_DEF_AT_TIMEOUT		2000//5000
#define THREADEDGSM_DEF_STA_PON			15000//10000 - Le doy un poco mas de tiempo para que despues del reset levante el SIM
#define THREADEDGSM_DEF_STA_POF			1000

// Use custom values or default ones
#ifndef THREADEDGSM_DTE_BUFFER_SIZE
#define THREADEDGSM_DTE_BUFFER_SIZE THREADEDGSM_DEF_DTE_BUF_SIZ
#endif
#ifndef THREADEDGSM_AT_TIMEOUT
#define THREADEDGSM_AT_TIMEOUT THREADEDGSM_DEF_AT_TIMEOUT
#endif
#ifndef THREADEDGSM_STARTUP_DELAY
#define THREADEDGSM_STARTUP_DELAY THREADEDGSM_DEF_STA_PON
#endif
#ifndef THREADEDGSM_STARTUP_POWER_OFF_DELAY
#define THREADEDGSM_STARTUP_POWER_OFF_DELAY THREADEDGSM_DEF_STA_POF
#endif

#define THREADEDGSM_INTERVAL_COUNT		3

//#define THREADEDGSM_DEBUG

#ifdef THREADEDGSM_DEBUG
	#define DEBUG_GSM_PRINT(x)  	DEBUG_PRINT(x)
	#define DEBUG_GSM_PRINTLN(x)  	DEBUG_PRINTLN(x)
#else
	#define DEBUG_GSM_PRINT(x)
	#define DEBUG_GSM_PRINTLN(x)
#endif

class ThreadedGSM
{
//variables
public:
	struct NetworkTime
	{
		int year;
		int month;
		int day;
		int hour;
		int minute;
		int second;
	};
	enum ReadMessagesListTypeE {
		READ_TYPE_UNREAD = 0,
		READ_TYPE_READ = 1,
		READ_TYPE_UNSENT = 2,
		READ_TYPE_SENT = 3,
		READ_TYPE_ALL = 4
	};
	enum IntervalSourceE
	{
		INTERVAL_CLOCK,
		INTERVAL_INBOX,
		INTERVAL_SIGNAL,
		INTERVAL_GPRS
	};
	struct SignalLevel
	{
		int Dbm;
		int Value;
	};

	typedef void (*ThreadedGSMCallbackSignal)(ThreadedGSM&, SignalLevel&);
	typedef void (*ThreadedGSMCallbackClock)(ThreadedGSM&, NetworkTime&);
	typedef void (*ThreadedGSMCallbackIncomingSMS)(ThreadedGSM&, String&, String&);
	typedef void (*ThreadedGSMCallbackBool)(ThreadedGSM&, bool);
	typedef void (*ThreadedGSMCallback)(ThreadedGSM&);
	struct conf
	{
		ThreadedGSMCallbackSignal signal;
		ThreadedGSMCallbackClock clock;
		ThreadedGSMCallbackIncomingSMS incoming;
		ThreadedGSMCallback ready;
		ThreadedGSMCallback outgoing;
		ThreadedGSMCallbackBool power;
	};
protected:
private:
	enum StatesStartup
	{
		STARTUP_POWER_OFF,
		STARTUP_POWER_OFF_DELAY,
		STARTUP_POWER_ON,
		STARTUP_DELAY,
		STARTUP_ENTER_AT,
		STARTUP_CHK_CPIN,
		STARTUP_CHK_CREG,
		STARTUP_CHK_CLTS,
		STARTUP_CHK_CENG
	};

	enum StatesClock
	{
		CLOCK_REQ,
		CLOCK_VERIFY
	};

	enum StatesSignal
	{
		SIGNAL_REQ,
		SIGNAL_VERIFY
	};

	enum StatesInbox
	{
		READ_REQ,
		READ_CHK_CMGF,
		READ_CHK_CPMS,
		READ_CHK_CMGL,
		READ_DELAY_CLEAR_BUFF,
		READ_CHK_CMGR,
		READ_CHK_CMGD
	};

	enum StatesOutbox
	{
		SEND_REQ,
		SEND_CHK_CMGF,
		SEND_CHK_RDY,
		SEND_CHK_OK
	};

	unsigned long tick;

	struct
	{
		int Index;
	}Message;

	// SMS Data
	struct
	{
		String InboxMsgContents;
		String InboxNumber;
		String OutboxMsgContents;
		String OutboxNumber;
	}SMS;

	Stream& stream;
	DTE dte;

	unsigned long tickSync[THREADEDGSM_INTERVAL_COUNT];
	unsigned long Intervals[THREADEDGSM_INTERVAL_COUNT];

	// callbacks
	conf configuration = {NULL, NULL, NULL, NULL, NULL, NULL};

	enum ReqTypes
	{
		REQ_CLOCK = 1,
		REQ_SIG = 2,
		REQ_INBOX = 4,
		REQ_OUTBOX = 8,
		REQ_STARTUP = 16
	};
	int requests;
	int state;
	int job;
//functions
public:
	ThreadedGSM(Stream& stream) : stream(stream), dte(stream, THREADEDGSM_DTE_BUFFER_SIZE)
	{
		for(int i=0;i<THREADEDGSM_INTERVAL_COUNT;i++)
			Intervals[i] = 0;

		job = state = requests = 0;
	}
	~ThreadedGSM(){};
	void nextJob()
	{
		job = 0;
	}
	void setHandlers(conf config)
	{
		this->configuration = config;
	}
	void setInterval(IntervalSourceE source, unsigned long interval)
	{
		Intervals[source] = interval;
		tickSync[source] = millis();
	}
	// Initialization
	void begin()
	{
		requests = (REQ_STARTUP);
	}
	// Call this function for executing thread
	void loop()
	{
		if(dte.getIsBusy()) return;

		// intervals
		for(int i=0;i<THREADEDGSM_INTERVAL_COUNT;i++)
		{
			if(Intervals[i])
			{
				if(millis() - tickSync[i] >= Intervals[i])
				{
					switch (i) {
						case INTERVAL_CLOCK:
						requests |= REQ_CLOCK;
						break;
						case INTERVAL_INBOX:
						requests |= REQ_INBOX;
						break;
						case INTERVAL_SIGNAL:
						requests |= REQ_SIG;
						break;
					}
					tickSync[i] = millis();
				}
			}
		}

		if(job == 0)
		{
			// no assigned job, assign it
			if(requests & REQ_CLOCK)
				job = REQ_CLOCK;
			else if(requests & REQ_SIG)
				job = REQ_SIG;
			else if(requests & REQ_INBOX)
				job = REQ_INBOX;
			else if(requests & REQ_OUTBOX)
				job = REQ_OUTBOX;
			else if(requests & REQ_STARTUP)
				job = REQ_STARTUP;

			if(job)
			{
				state = 0;
				DEBUG_GSM_PRINT("Job ID: ");
				DEBUG_GSM_PRINTLN(job);
			}
		}

		// execute current job
		if(job == REQ_STARTUP)
			Startup();
		else if(job == REQ_CLOCK)
			Clock();
		else if(job == REQ_SIG)
			Signal();
		else if(job == REQ_INBOX)
			Inbox();
		else if(job == REQ_OUTBOX)
			Outbox();
	}
	
	// Requests

	// Send SMS
	void sendSMS(String& Number, String& Message){
		requests |= (REQ_OUTBOX);
		SMS.OutboxMsgContents = Message;
		SMS.OutboxNumber = Number;
	}

	// Get Number
	void getNumber(String& Number){

	}

	// If return any different from 0, there is a job pending
	int getBusy(){
		return job;
	}

protected:
private:

	// States
	void Startup()
	{
		int lastState = state;
		switch(state)
		{
			case STARTUP_POWER_OFF:
				if(this->configuration.power != NULL)
					this->configuration.power(*this, false);
				tick = millis();
				state = STARTUP_POWER_OFF_DELAY;
				break;
			case STARTUP_POWER_OFF_DELAY:
				if(millis() - tick >= THREADEDGSM_STARTUP_POWER_OFF_DELAY)
					state = STARTUP_POWER_ON;
				break;
			case STARTUP_POWER_ON:
				if(this->configuration.power != NULL)
					this->configuration.power(*this, true);
				// begin delay
				tick = millis();
				state = STARTUP_DELAY;
				break;
			case STARTUP_DELAY:
				if(millis() - tick >= THREADEDGSM_STARTUP_DELAY)
				{
					dte.SendCommand("AT\r", THREADEDGSM_AT_TIMEOUT, "OK\r");
					state = STARTUP_ENTER_AT;
				}
				break;
			case STARTUP_ENTER_AT:
				if(dte.getResult() ==  DTE::EXPECT_RESULT)
				{
					dte.SendCommand("AT+CPIN?\r", 10000, "OK\r");	// Estaba en 10000
					state = STARTUP_CHK_CPIN;
				}
				else
				{
					state = STARTUP_POWER_OFF;
				}
				break;
			case STARTUP_CHK_CPIN:
				if(dte.getResult() == DTE::EXPECT_RESULT)
				{
					if(dte.getBuffer().indexOf("+CPIN: READY") != -1)
					{
						dte.SendCommand("AT+CREG?\r", THREADEDGSM_AT_TIMEOUT, "OK\r");
						state = STARTUP_CHK_CREG;
					}else
					{
						state = STARTUP_POWER_OFF;
					}
				}else{
					state = STARTUP_POWER_OFF;
				}
				break;
			case STARTUP_CHK_CREG:
				if(dte.getResult() == DTE::EXPECT_RESULT)
				{
					if((dte.getBuffer().indexOf(",1") >= 0) || (dte.getBuffer().indexOf(",5") >= 0))
					{
						dte.SendCommand("AT+CLTS=1\r", THREADEDGSM_AT_TIMEOUT, "OK\r");
						state = STARTUP_CHK_CLTS;
					}else
						state = STARTUP_POWER_OFF;
				}else
					state = STARTUP_POWER_OFF;
				break;
			case STARTUP_CHK_CLTS:
				if(dte.getResult() == DTE::EXPECT_RESULT)
				{
					dte.SendCommand("AT+CENG=3\r", THREADEDGSM_AT_TIMEOUT, "OK\r");
					state = STARTUP_CHK_CENG;
				}else
					state = STARTUP_POWER_OFF;
				break;
			case STARTUP_CHK_CENG:
				if(dte.getResult() == DTE::EXPECT_RESULT)
				{
					requests |= ((REQ_CLOCK)|(REQ_SIG));
					clearReq(REQ_STARTUP);
					for(int i=0;i<THREADEDGSM_INTERVAL_COUNT;i++)
						tickSync[i] = millis();
					if(this->configuration.ready != NULL)
						this->configuration.ready(*this);
				}else
					state = STARTUP_POWER_OFF;
				break;
		}
		if(state != lastState)
		{
				DEBUG_GSM_PRINT(F("STARTUP_STATE: "));
				DEBUG_GSM_PRINTLN(state);
		}
	}

	// Threads
	void Clock()
	{
		String clockTime;
		int lastState = state;
		switch(state)
		{
			case CLOCK_REQ:
				dte.SendCommand("AT+CCLK?\r", THREADEDGSM_AT_TIMEOUT, "OK\r");
				state = CLOCK_VERIFY;
				break;
			case CLOCK_VERIFY:
				int index = dte.getBuffer().indexOf("+CCLK: ");
				if(index >= 0)
				{
					// parse clock
					index+=8;
					int endindex;
					endindex = dte.getBuffer().indexOf("+", index);
					if(endindex >= 0)
						clockTime = dte.getBuffer().substring(index, endindex);
					else
					{
						endindex = dte.getBuffer().indexOf("-", index);
						if(endindex >= 0)
							clockTime = dte.getBuffer().substring(index, endindex);
					}

					if(endindex >= 0)
					{
						NetworkTime ClockTime;
						ClockTime.year = 2000+clockTime.substring(0,2).toInt();
						ClockTime.month = clockTime.substring(3,5).toInt();
						ClockTime.day = clockTime.substring(6,8).toInt();
						ClockTime.hour = clockTime.substring(9,11).toInt();
						ClockTime.minute = clockTime.substring(12,14).toInt();
						ClockTime.second = clockTime.substring(15,17).toInt();
						if(this->configuration.clock != NULL)
							this->configuration.clock(*this, ClockTime);
					}
				}
				clearReq(REQ_CLOCK);
				break;
		}
		if(state != lastState)
		{
				DEBUG_GSM_PRINT(F("CLOCK_STATE: "));
				DEBUG_GSM_PRINTLN(state);
		}
	}
	void Signal()
	{
		int lastState = state;
		switch(state)
		{
			case SIGNAL_REQ:
				dte.SendCommand("AT+CSQ\r", THREADEDGSM_AT_TIMEOUT, "OK\r");
				state = SIGNAL_VERIFY;
				break;
			case SIGNAL_VERIFY:
				int index = dte.getBuffer().indexOf("+CSQ: ");
				if(index >= 0)
				{
					// parse signal
					index+=6;
					SignalLevel GsmSignal;
					GsmSignal.Value = dte.getBuffer().substring(index,index+2).toInt();
					GsmSignal.Dbm = dte.getBuffer().substring(index+3,index+5).toInt();
					if(GsmSignal.Value != 0)
					{
						if(this->configuration.signal != NULL)
							this->configuration.signal(*this, GsmSignal);
					}
				}
				clearReq(REQ_SIG);
				break;
		}
		if(state != lastState)
		{
				DEBUG_GSM_PRINT(F("SIGNAL_STATE: "));
				DEBUG_GSM_PRINTLN(state);
		}
	}
	void clearReq(int req)
	{
		requests &= ~(req);
		nextJob();
	}
	
	// Mensajes en formato de texto
	void Inbox()
	{
		String CMD;
		int lastState = state;
		switch(state)
		{
			case READ_REQ:
				SMS.InboxMsgContents = "";
				dte.SendCommand("AT+CMGF=1\r", THREADEDGSM_AT_TIMEOUT, "OK\r");	//AT+CMGF=0 -> PDU, 1 -> Texto
				state = READ_CHK_CMGF;
				break;
			case READ_CHK_CMGF:
				if(dte.getResult() == DTE::EXPECT_RESULT)
				{
					dte.SendCommand("AT+CPMS=\"SM\"\r", THREADEDGSM_AT_TIMEOUT, "OK\r");
					state = READ_CHK_CPMS;
				}
				else clearReq(REQ_INBOX);
				break;
			case READ_CHK_CPMS:
				if(dte.getResult() == DTE::EXPECT_RESULT)
				{
					dte.SendCommand("AT+CMGL=\"ALL\"\r", THREADEDGSM_AT_TIMEOUT, ",");
					state = READ_CHK_CMGL;
				}
				else clearReq(REQ_INBOX);
				break;
			case READ_CHK_CMGL:
				if(dte.getResult() == DTE::EXPECT_RESULT)
				{
					//fetch index
					int indexStart = dte.getBuffer().indexOf("+CMGL: ");
					if (indexStart >= 0)
					{
						Message.Index = dte.getBuffer().substring(indexStart + 7, dte.getBuffer().indexOf(",")).toInt();
						if(Message.Index != 0)
						{
							dte.Delay(2000);
							state = READ_DELAY_CLEAR_BUFF;
						}
					}
				}
				if(state != READ_DELAY_CLEAR_BUFF)
					clearReq(REQ_INBOX);

				break;
			case READ_DELAY_CLEAR_BUFF:
				CMD = "AT+CMGR=";
				CMD += Message.Index;
				CMD += "\r";
				dte.SendCommand(CMD.c_str(), THREADEDGSM_AT_TIMEOUT, "OK\r");
				state = READ_CHK_CMGR;
				break;
			case READ_CHK_CMGR:
				if(dte.getResult() == DTE::EXPECT_RESULT)
				{
					// DEBUG_GSM_PRINTLN(dte.getBuffer());
					// AT+CMGR=1
					// +CMGR: "REC READ","+543516510632","Luciano","20/02/05,10:53:09-12"
					// Hola

					// OK
					int indexStart = dte.getBuffer().indexOf("+CMGR: ");
					if(indexStart >= 0)
					{
						// int firstComa = dte.getBuffer().indexOf("\",\"", indexStart);
						// int indexStartNumber = firstComa + 3;
						// int secondComa = dte.getBuffer().indexOf("\",\"", indexStartNumber);
						// int indexEndNumber = secondComa;
						// String Number = dte.getBuffer().substring(indexStartNumber, indexEndNumber);
						// DEBUG_GSM_PRINT("Number: ");
						// DEBUG_GSM_PRINTLN(Number);
						// int indexStartFrom = secondComa + 3;
						// int thirdComa = dte.getBuffer().indexOf("\",\"", indexStartFrom);
						// int indexEndFrom = thirdComa;
						// String From = dte.getBuffer().substring(indexStartFrom, indexEndFrom);
						// DEBUG_GSM_PRINT("From: ");
						// DEBUG_GSM_PRINTLN(From);
						// int indexStartDate = thirdComa + 3;
						// int indexEndDate = dte.getBuffer().indexOf("\"", indexStartDate);
						// String Date = dte.getBuffer().substring(indexStartDate, indexEndDate);
						// DEBUG_GSM_PRINT("Date: ");
						// DEBUG_GSM_PRINTLN(Date);

						int indexStartNumber = dte.getBuffer().indexOf("\",\"", indexStart);
						if(indexStartNumber >= 0){
							indexStartNumber+=3;
							int indexEndNumber = dte.getBuffer().indexOf("\",\"", indexStartNumber);
							if(indexEndNumber >= 0){
								SMS.InboxNumber = dte.getBuffer().substring(indexStartNumber, indexEndNumber);
							}
						}
												
						int indexStartTEXT = dte.getBuffer().indexOf("\r\n", indexStart);
						if (indexStartTEXT >= 0)
						{
							indexStartTEXT+=2;
							int indexEndTEXT = dte.getBuffer().indexOf("\r", indexStartTEXT);
							if(indexEndTEXT >= 0)
								SMS.InboxMsgContents = dte.getBuffer().substring(indexStartTEXT, indexEndTEXT);
						}
					}
					// Borra el mensaje una vez leído
					CMD = "AT+CMGD=";
					CMD += Message.Index;
					CMD += "\r";
					dte.SendCommand(CMD.c_str(), THREADEDGSM_AT_TIMEOUT, "OK\r");
					state = READ_CHK_CMGD;
				}else
					clearReq(REQ_INBOX);
				break;
			case READ_CHK_CMGD:
				if( (dte.getResult() == DTE::EXPECT_RESULT) && (SMS.InboxMsgContents != ""))
				{
					if(this->configuration.incoming != NULL)
						this->configuration.incoming(*this, SMS.InboxNumber, SMS.InboxMsgContents);
				}
				clearReq(REQ_INBOX);
				break;
		}
		if(state != lastState)
		{
				DEBUG_GSM_PRINT(F("INBOX_STATE: "));
				DEBUG_GSM_PRINTLN(state);
		}
	}

	// Enviar SMS en modo texto
	void Outbox()
	{
		String CMD;
		int lastState = state;
		switch(state)
		{
			case SEND_REQ:
				dte.SendCommand("AT+CMGF=1\r", THREADEDGSM_AT_TIMEOUT, "OK\r");
				state = SEND_CHK_CMGF;
				break;
			case SEND_CHK_CMGF:
				if(dte.getResult() == DTE::EXPECT_RESULT)
				{
					CMD = "AT+CMGS=\"" + SMS.OutboxNumber + "\"\r";
					dte.SendCommand(CMD.c_str(), 15000, "> ");
					state = SEND_CHK_RDY;
				}
				else clearReq(REQ_OUTBOX);
				break;
			case SEND_CHK_RDY:
				if(dte.getResult() == DTE::EXPECT_RESULT)
				{
					CMD = SMS.OutboxMsgContents;
					CMD += (char)26;
					dte.SendCommand(CMD.c_str(), 10000, "OK\r");
					state = SEND_CHK_OK;
				}else clearReq(REQ_OUTBOX);
				break;
			case SEND_CHK_OK:
				if(dte.getResult() == DTE::EXPECT_RESULT)
				{
					if(this->configuration.outgoing != NULL)
						this->configuration.outgoing(*this);
				}
				clearReq(REQ_OUTBOX);
				break;
		}
		if(state != lastState)
		{
				DEBUG_GSM_PRINT(F("OUTBOX_STATE: "));
				DEBUG_GSM_PRINTLN(state);
		}
	}

}; //ThreadedGSM

#endif //__THREADEDGSM_H__
