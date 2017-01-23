/*
* ThreadedGSM.h
*
* Created: 20/09/2016 11:14:02
* Author: Neta Yahav
*/


#ifndef __THREADEDGSM_H__
#define __THREADEDGSM_H__

#include <Arduino.h>
#include "DTE.h"

// Defaults
#define THREADEDGSM_DEF_DTE_BUF_SIZ		512
#define THREADEDGSM_DEF_AT_TIMEOUT		5000
#define THREADEDGSM_DEF_STA_PON				10000
#define THREADEDGSM_DEF_STA_POF				1000

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

#define THREADEDGSM_INTERVAL_COUNT						3

#ifdef THREADEDGSM_DEBUG
	#define DEBUG_PRINT(x)  THREADEDGSM_DEBUG.print (x)
	#define DEBUG_PRINTLN(x)  THREADEDGSM_DEBUG.println (x)
#else
	#define DEBUG_PRINT(x)
	#define DEBUG_PRINTLN(x)
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
		INTERVAL_SIGNAL
	};
	struct SignalLevel
	{
		int Dbm;
		int Value;
	};
	enum SendMessageResult
	{
		SMS_SEND_OK,
		SMS_SEND_FAIL,
		SMS_SEND_WAIT
	};
	typedef void (*ThreadedGSMCallbackSignal)(ThreadedGSM&, SignalLevel&);
	typedef void (*ThreadedGSMCallbackClock)(ThreadedGSM&, NetworkTime&);
	typedef void (*ThreadedGSMCallbackIncomingSMS)(ThreadedGSM&, String&);
	typedef void (*ThreadedGSMCallbackOutgoingSMS)(ThreadedGSM&, bool);
	typedef void (*ThreadedGSMCallbackBool)(ThreadedGSM&, bool);
	typedef void (*ThreadedGSMCallback)(ThreadedGSM&);
	struct conf
	{
		ThreadedGSMCallbackSignal signal;
		ThreadedGSMCallbackClock clock;
		ThreadedGSMCallbackIncomingSMS incoming;
		ThreadedGSMCallback ready;
		ThreadedGSMCallbackOutgoingSMS outgoing;
		ThreadedGSMCallbackBool power;
	};
protected:
private:
	enum StartupStateE
	{
		STARTUP_OK,
		STARTUP_UNINITIALIZED,
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

	enum SyncClockE
	{
		CLOCK_IDLE,
		CLOCK_OK,
		CLOCK_REQ,
		CLOCK_VERIFY,
		CLOCK_FAIL
	};

	enum ReadSignalE
	{
		SIGNAL_IDLE,
		SIGNAL_OK,
		SIGNAL_REQ,
		SIGNAL_VERIFY,
		SIGNAL_FAIL
	};

	enum ReadMessagesE
	{
		READ_IDLE,
		READ_REQ,
		READ_FAIL,
		READ_CHK_CMGF,
		READ_CHK_CPMS,
		READ_CHK_CMGL,
		READ_DELAY_CLEAR_BUFF,
		READ_CHK_CMGR,
		READ_CHK_CMGD,
		READ_OK
	};

	enum SendMessagesE
	{
		SEND_IDLE,
		SEND_OK,
		SEND_FAIL,
		SEND_REQ,
		SEND_CHK_CMGF,
		SEND_CHK_RDY,
		SEND_CHK_OK
	};

	unsigned long tick;

	struct
	{
		StartupStateE startupState;
		ReadMessagesE readMsgState;
		SyncClockE syncClockState;
		ReadSignalE readSignalState;
		SendMessagesE sendMessageState;
	}State;

	struct
	{
		int Index;
		ReadMessagesListTypeE ListMsgType;
	}Message;

	struct
	{
		String InboxMsgContents;
		String OutboxMsgContents;
	}SMS;

	SignalLevel GsmSignal;

	NetworkTime ClockTime;
	Stream& stream;
	DTE dte;

	unsigned long tickSync[THREADEDGSM_INTERVAL_COUNT];
	unsigned long Intervals[THREADEDGSM_INTERVAL_COUNT];

	// callbacks
	conf configuration = {NULL, NULL, NULL, NULL, NULL, NULL};
//functions
public:
	ThreadedGSM(Stream& stream) : stream(stream), dte(stream, THREADEDGSM_DTE_BUFFER_SIZE)
	{
		State.startupState = STARTUP_UNINITIALIZED;
		State.syncClockState = CLOCK_IDLE;
		State.readSignalState = SIGNAL_IDLE;
		State.readMsgState = READ_IDLE;
		State.sendMessageState = SEND_IDLE;
		ClockTime.year = 2000;
		ClockTime.month = ClockTime.day = 1;
		ClockTime.hour = ClockTime.minute = ClockTime.second = 0;
		for(int i=0;i<THREADEDGSM_INTERVAL_COUNT;i++)
			Intervals[i] = 0;

	}
	~ThreadedGSM(){};
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
		State.startupState = STARTUP_POWER_OFF;
	}
	// Call this function for executing thread
	void loop()
	{
		loop2();
		events();
	}
	// Requests
	bool sendSMS(String& PDU)
	{
		if((State.sendMessageState == SEND_IDLE))
		{
			State.sendMessageState = SEND_REQ;
			SMS.OutboxMsgContents = PDU;
			return true;
		}
		return false;
	}
protected:
private:
	void events()
	{
		if((State.readSignalState == SIGNAL_OK) || (State.readSignalState == SIGNAL_OK))
		{
			if((this->configuration.signal != NULL) && (State.readSignalState == SIGNAL_OK))
				this->configuration.signal(*this, GsmSignal);
			State.readSignalState = SIGNAL_IDLE;
		}
		if((State.syncClockState == CLOCK_OK) || (State.syncClockState == CLOCK_FAIL))
		{
			if((this->configuration.clock != NULL) && (State.syncClockState == CLOCK_OK))
				this->configuration.clock(*this, ClockTime);
			State.syncClockState = CLOCK_IDLE;
		}
		if((State.readMsgState == READ_OK) || (State.readMsgState == READ_FAIL))
		{
			if((this->configuration.incoming != NULL) && (State.readMsgState == READ_OK))
				this->configuration.incoming(*this, SMS.InboxMsgContents);
			State.readMsgState = READ_IDLE;
		}
		if((State.sendMessageState == SEND_OK) || (State.sendMessageState == SEND_FAIL))
		{
			if(this->configuration.outgoing != NULL)
				this->configuration.outgoing(*this, State.sendMessageState == SEND_OK ? true : false);
			State.sendMessageState = SEND_IDLE;
		}
	}
	void loop2()
	{
		if(State.startupState == STARTUP_UNINITIALIZED) return;
		if(dte.getIsBusy()) return;
		if(State.startupState != STARTUP_OK)
		{
			StateStartup();
			return;
		}
		// intervals
		for(int i=0;i<THREADEDGSM_INTERVAL_COUNT;i++)
		{
			if(Intervals[i])
			{
				if(millis() - tickSync[i] >= Intervals[i])
				{
					switch (i) {
						case INTERVAL_CLOCK:
						RequestSyncClock();
						break;
						case INTERVAL_INBOX:
						RequestReadMessage(READ_TYPE_ALL);
						break;
						case INTERVAL_SIGNAL:
						RequestReadSignal();
						break;
					}
					tickSync[i] = millis();
				}
			}
		}
		// Clock
		if(State.syncClockState != CLOCK_IDLE)
		{
			Clock();
			return;
		}
		// Signal
		if(State.readSignalState != SIGNAL_IDLE)
		{
			Signal();
			return;
		}
		// Outbox
		if(State.sendMessageState != SEND_IDLE)
		{
			Outbox();
			return;
		}
		// Inbox
		if(State.readMsgState != READ_IDLE)
		{
			Inbox();
			return;
		}
	}
	// Requests
	bool RequestReadMessage(ReadMessagesListTypeE type)
	{
		if( (State.readMsgState == READ_IDLE) )
		{
			State.readMsgState = READ_REQ;
			Message.ListMsgType = type;
			return true;
		}
		return false;
	}
	bool RequestSyncClock()
	{
		if( (State.syncClockState == CLOCK_IDLE))
		{
			State.syncClockState = CLOCK_REQ;
			return true;
		}
		return false;
	}
	bool RequestReadSignal()
	{
		if((State.readSignalState == SIGNAL_IDLE))
		{
			State.readSignalState = SIGNAL_REQ;
			return true;
		}
		return false;
	}
	// States
	void StateStartup()
	{
		StartupStateE lastState = State.startupState;
		switch(State.startupState)
		{
			case STARTUP_UNINITIALIZED:
			case STARTUP_OK:
				break;
			case STARTUP_POWER_OFF:
				if(this->configuration.power != NULL)
					this->configuration.power(*this, false);
				tick = millis();
				State.startupState = STARTUP_POWER_OFF_DELAY;
				break;
			case STARTUP_POWER_OFF_DELAY:
				if(millis() - tick >= THREADEDGSM_STARTUP_POWER_OFF_DELAY)
					State.startupState = STARTUP_POWER_ON;
				break;
			case STARTUP_POWER_ON:
				if(this->configuration.power != NULL)
					this->configuration.power(*this, true);
				// begin delay
				tick = millis();
				State.startupState = STARTUP_DELAY;
				break;
			case STARTUP_DELAY:
				if(millis() - tick >= THREADEDGSM_STARTUP_DELAY)
				{
					dte.SendCommand("AT\r", THREADEDGSM_AT_TIMEOUT, "OK\r");
					State.startupState = STARTUP_ENTER_AT;
				}
				break;
			case STARTUP_ENTER_AT:
				if(dte.getResult() ==  DTE::EXPECT_RESULT)
				{
					dte.SendCommand("AT+CPIN?\r", 10000, "OK\r");
					State.startupState = STARTUP_CHK_CPIN;
				}
				else
				{
					State.startupState = STARTUP_POWER_OFF;
				}
				break;
			case STARTUP_CHK_CPIN:
				if(dte.getResult() == DTE::EXPECT_RESULT)
				{
					if(dte.getBuffer().indexOf("+CPIN: READY") != -1)
					{
						dte.SendCommand("AT+CREG?\r", THREADEDGSM_AT_TIMEOUT, "OK\r");
						State.startupState = STARTUP_CHK_CREG;
					}else
					{
						State.startupState = STARTUP_POWER_OFF;
					}
				}else
					State.startupState = STARTUP_POWER_OFF;
				break;
			case STARTUP_CHK_CREG:
				if(dte.getResult() == DTE::EXPECT_RESULT)
				{
					if((dte.getBuffer().indexOf(",1") >= 0) || (dte.getBuffer().indexOf(",5") >= 0))
					{
						dte.SendCommand("AT+CLTS=1\r", THREADEDGSM_AT_TIMEOUT, "OK\r");
						State.startupState = STARTUP_CHK_CLTS;
					}else
						State.startupState = STARTUP_POWER_OFF;
				}else
					State.startupState = STARTUP_POWER_OFF;
				break;
			case STARTUP_CHK_CLTS:
				if(dte.getResult() == DTE::EXPECT_RESULT)
				{
					dte.SendCommand("AT+CENG=3\r", THREADEDGSM_AT_TIMEOUT, "OK\r");
					State.startupState = STARTUP_CHK_CENG;
				}else
					State.startupState = STARTUP_POWER_OFF;
				break;
			case STARTUP_CHK_CENG:
				if(dte.getResult() == DTE::EXPECT_RESULT)
				{
					RequestSyncClock();
					RequestReadSignal();
					if(this->configuration.ready != NULL)
						this->configuration.ready(*this);
					State.startupState = STARTUP_OK;
				}else
					State.startupState = STARTUP_POWER_OFF;
				break;
		}
		if(State.startupState != lastState)
		{
				DEBUG_PRINT(F("STARTUP_STATE: "));
				DEBUG_PRINTLN(State.startupState);
		}
	}

	// Threads
	void Clock()
	{
		String clockTime;
		SyncClockE lastState = State.syncClockState;
		switch(State.syncClockState)
		{
			case CLOCK_IDLE:
			case CLOCK_OK:
			case CLOCK_FAIL:
				break;
			case CLOCK_REQ:
				dte.SendCommand("AT+CCLK?\r", THREADEDGSM_AT_TIMEOUT, "OK\r");
				State.syncClockState = CLOCK_VERIFY;
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
						ClockTime.year = 2000+clockTime.substring(0,2).toInt();
						ClockTime.month = clockTime.substring(3,5).toInt();
						ClockTime.day = clockTime.substring(6,8).toInt();
						ClockTime.hour = clockTime.substring(9,11).toInt();
						ClockTime.minute = clockTime.substring(12,14).toInt();
						ClockTime.second = clockTime.substring(15,17).toInt();
						State.syncClockState = CLOCK_OK;
					}
				}
				if(State.syncClockState != CLOCK_OK)
					State.syncClockState = CLOCK_FAIL;
				break;
		}
		if(State.syncClockState != lastState)
		{
				DEBUG_PRINT(F("CLOCK_STATE: "));
				DEBUG_PRINTLN(State.syncClockState);
		}
	}
	void Signal()
	{
		ReadSignalE lastState = State.readSignalState;
		switch(State.readSignalState)
		{
			case SIGNAL_IDLE:
			case SIGNAL_OK:
			case SIGNAL_FAIL:
			break;
			case SIGNAL_REQ:
				dte.SendCommand("AT+CSQ\r", THREADEDGSM_AT_TIMEOUT, "OK\r");
				State.readSignalState = SIGNAL_VERIFY;
				break;
			case SIGNAL_VERIFY:
				int index = dte.getBuffer().indexOf("+CSQ: ");
				if(index >= 0)
				{
					// parse signal
					index+=6;
					int tmpValue, tmpDbm;

					tmpValue = dte.getBuffer().substring(index,index+2).toInt();
					tmpDbm = dte.getBuffer().substring(index+3,index+5).toInt();
					if(tmpValue != 0)
					{
						GsmSignal.Dbm = tmpDbm;
						GsmSignal.Value = tmpValue;
						State.readSignalState = SIGNAL_OK;
					}
				}
				if(State.readSignalState != SIGNAL_OK)
					State.readSignalState = SIGNAL_FAIL;
				break;
		}
		if(State.readSignalState != lastState)
		{
				DEBUG_PRINT(F("SIGNAL_STATE: "));
				DEBUG_PRINTLN(State.readSignalState);
		}
	}
	void Inbox()
	{
		String CMD;
		ReadMessagesE lastState = State.readMsgState;
		switch(State.readMsgState)
		{
			case READ_OK:
			case READ_IDLE:
			case READ_FAIL:
				break;
			case READ_REQ:
				SMS.InboxMsgContents = "";
				dte.SendCommand("AT+CMGF=0\r", THREADEDGSM_AT_TIMEOUT, "OK\r");
				State.readMsgState = READ_CHK_CMGF;
				break;
			case READ_CHK_CMGF:
				if(dte.getResult() == DTE::EXPECT_RESULT)
				{
					dte.SendCommand("AT+CPMS=\"SM\"\r", THREADEDGSM_AT_TIMEOUT, "OK\r");
					State.readMsgState = READ_CHK_CPMS;
				}
				else State.readMsgState = READ_IDLE;
				break;
			case READ_CHK_CPMS:
				if(dte.getResult() == DTE::EXPECT_RESULT)
				{
					CMD = "AT+CMGL=";
					CMD += (int)Message.ListMsgType;
					CMD += "\r";
					dte.SendCommand(CMD.c_str(), THREADEDGSM_AT_TIMEOUT, ",");
					State.readMsgState = READ_CHK_CMGL;
				}
				else State.readMsgState = READ_IDLE;
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
							State.readMsgState = READ_DELAY_CLEAR_BUFF;
						}
					}
				}
				if(State.readMsgState != READ_DELAY_CLEAR_BUFF)
					State.readMsgState = READ_FAIL;

				break;
			case READ_DELAY_CLEAR_BUFF:
				CMD = "AT+CMGR=";
				CMD += Message.Index;
				CMD += "\r";
				dte.SendCommand(CMD.c_str(), THREADEDGSM_AT_TIMEOUT, "OK\r");
				State.readMsgState = READ_CHK_CMGR;
				break;
			case READ_CHK_CMGR:
				if(dte.getResult() == DTE::EXPECT_RESULT)
				{
					int indexStart = dte.getBuffer().indexOf("+CMGR: ");
					if(indexStart >= 0)
					{
						int indexStartPDU = dte.getBuffer().indexOf("\r\n", indexStart);
						if (indexStartPDU >= 0)
						{
							indexStartPDU+=2;
							int indexEndPDU = dte.getBuffer().indexOf("\r", indexStartPDU);
							if(indexEndPDU >= 0)
								SMS.InboxMsgContents = dte.getBuffer().substring(indexStartPDU, indexEndPDU);
						}
					}
					CMD = "AT+CMGD=";
					CMD += Message.Index;
					CMD += "\r";
					dte.SendCommand(CMD.c_str(), THREADEDGSM_AT_TIMEOUT, "OK\r");
					State.readMsgState = READ_CHK_CMGD;
				}else
					State.readMsgState = READ_FAIL;
				break;
			case READ_CHK_CMGD:
				if( (dte.getResult() == DTE::EXPECT_RESULT) && (SMS.InboxMsgContents != ""))
				{
					State.readMsgState = READ_OK;
				}
				else
					State.readMsgState = READ_FAIL;
				break;
		}
		if(State.readMsgState != lastState)
		{
				DEBUG_PRINT(F("INBOX_STATE: "));
				DEBUG_PRINTLN(State.readMsgState);
		}
	}

	void Outbox()
	{
		String CMD;
		SendMessagesE lastState = State.sendMessageState;
		switch(State.sendMessageState)
		{
			case SEND_OK:
			case SEND_FAIL:
			case SEND_IDLE:
				break;
			case SEND_REQ:
				dte.SendCommand("AT+CMGF=0\r", THREADEDGSM_AT_TIMEOUT, "OK\r");
				State.sendMessageState = SEND_CHK_CMGF;
				break;
			case SEND_CHK_CMGF:
				if(dte.getResult() == DTE::EXPECT_RESULT)
				{
					char smsc_len_b;
					int smsc_length = 0;
					for(int i=0;i<=1;i++)
					{
						smsc_len_b = SMS.OutboxMsgContents.charAt(i);
						if (smsc_len_b >= '0' && smsc_len_b <= '9')
							smsc_length |= smsc_len_b - '0';
						else if (smsc_len_b >= 'a' && smsc_len_b <='f')
							smsc_length |= smsc_len_b - 'a' + 10;
						else if (smsc_len_b >= 'A' && smsc_len_b <='F')
							smsc_length |= smsc_len_b - 'A' + 10;
						smsc_length = smsc_length<<4;
					}
					CMD = "AT+CMGS=";
					CMD += (SMS.OutboxMsgContents.length() / 2) - (1+smsc_length);
					CMD += "\r";
					dte.SendCommand(CMD.c_str(), 15000, "> ");
					State.sendMessageState = SEND_CHK_RDY;
				}
				else State.sendMessageState = SEND_FAIL;
				break;
			case SEND_CHK_RDY:
				if(dte.getResult() == DTE::EXPECT_RESULT)
				{
					CMD = SMS.OutboxMsgContents;
					CMD += (char)26;
					dte.SendCommand(CMD.c_str(), 10000, "OK\r");
					State.sendMessageState = SEND_CHK_OK;
				}else State.sendMessageState = SEND_FAIL;
				break;
			case SEND_CHK_OK:
				if(dte.getResult() == DTE::EXPECT_RESULT)
				{
					State.sendMessageState = SEND_OK;
				}else State.sendMessageState = SEND_FAIL;
				break;
		}
		if(State.sendMessageState != lastState)
		{
				DEBUG_PRINT(F("OUTBOX_STATE: "));
				DEBUG_PRINTLN(State.sendMessageState);
		}
	}
}; //ThreadedGSM

#endif //__THREADEDGSM_H__
