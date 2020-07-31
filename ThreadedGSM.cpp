/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ThreadedGSM.cpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: Neta Yahav <neta540@gmail.com>             +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2020/07/31 18:03:10 by Neta Yahav        #+#    #+#             */
/*   Updated: 2020/07/31 18:10:20 by Neta Yahav       ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ThreadedGSM.h"

ThreadedGSM::ThreadedGSM(Stream &stream)
    : stream(stream), dte(stream, THREADEDGSM_DTE_BUFFER_SIZE) {
  for (int i = 0; i < THREADEDGSM_INTERVAL_COUNT; i++)
    Intervals[i] = 0;

  job = state = requests = 0;
}

ThreadedGSM::~ThreadedGSM() {}

void ThreadedGSM::nextJob() { job = 0; }

void ThreadedGSM::setHandlers(conf config) { this->configuration = config; }

void ThreadedGSM::setInterval(IntervalSourceE source, unsigned long interval) {
  Intervals[source] = interval;
  tickSync[source] = millis();
}

void ThreadedGSM::begin() { requests = (REQ_STARTUP); }

void ThreadedGSM::loop() {
  if (dte.getIsBusy())
    return;

  // intervals
  for (int i = 0; i < THREADEDGSM_INTERVAL_COUNT; i++) {
    if (Intervals[i]) {
      if (millis() - tickSync[i] >= Intervals[i]) {
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

  if (job == 0) {
    // no assigned job, assign it
    if (requests & REQ_CLOCK)
      job = REQ_CLOCK;
    else if (requests & REQ_SIG)
      job = REQ_SIG;
    else if (requests & REQ_INBOX)
      job = REQ_INBOX;
    else if (requests & REQ_OUTBOX)
      job = REQ_OUTBOX;
    else if (requests & REQ_STARTUP)
      job = REQ_STARTUP;

    if (job) {
      state = 0;
      DEBUG_PRINT("Job ID: ");
      DEBUG_PRINTLN(job);
    }
  }

  // execute current job
  if (job == REQ_STARTUP)
    Startup();
  else if (job == REQ_CLOCK)
    Clock();
  else if (job == REQ_SIG)
    Signal();
  else if (job == REQ_INBOX)
    Inbox();
  else if (job == REQ_OUTBOX)
    Outbox();
}

void ThreadedGSM::sendSMS(String &PDU) {
  requests |= (REQ_OUTBOX);
  SMS.OutboxMsgContents = PDU;
}

void ThreadedGSM::Startup() {
  int lastState = state;
  switch (state) {
  case STARTUP_POWER_OFF:
    if (this->configuration.power != NULL)
      this->configuration.power(*this, false);
    tick = millis();
    state = STARTUP_POWER_OFF_DELAY;
    break;
  case STARTUP_POWER_OFF_DELAY:
    if (millis() - tick >= THREADEDGSM_STARTUP_POWER_OFF_DELAY)
      state = STARTUP_POWER_ON;
    break;
  case STARTUP_POWER_ON:
    if (this->configuration.power != NULL)
      this->configuration.power(*this, true);
    // begin delay
    tick = millis();
    state = STARTUP_DELAY;
    break;
  case STARTUP_DELAY:
    if (millis() - tick >= THREADEDGSM_STARTUP_DELAY) {
      dte.SendCommand("AT\r", THREADEDGSM_AT_TIMEOUT, "OK\r");
      state = STARTUP_ENTER_AT;
    }
    break;
  case STARTUP_ENTER_AT:
    if (dte.getResult() == DTE::EXPECT_RESULT) {
      dte.SendCommand("AT+CPIN?\r", 10000, "OK\r");
      state = STARTUP_CHK_CPIN;
    } else {
      state = STARTUP_POWER_OFF;
    }
    break;
  case STARTUP_CHK_CPIN:
    if (dte.getResult() == DTE::EXPECT_RESULT) {
      if (dte.getBuffer().indexOf("+CPIN: READY") != -1) {
        dte.SendCommand("AT+CREG?\r", THREADEDGSM_AT_TIMEOUT, "OK\r");
        state = STARTUP_CHK_CREG;
      } else {
        state = STARTUP_POWER_OFF;
      }
    } else
      state = STARTUP_POWER_OFF;
    break;
  case STARTUP_CHK_CREG:
    if (dte.getResult() == DTE::EXPECT_RESULT) {
      if ((dte.getBuffer().indexOf(",1") >= 0) ||
          (dte.getBuffer().indexOf(",5") >= 0)) {
        dte.SendCommand("AT+CLTS=1\r", THREADEDGSM_AT_TIMEOUT, "OK\r");
        state = STARTUP_CHK_CLTS;
      } else
        state = STARTUP_POWER_OFF;
    } else
      state = STARTUP_POWER_OFF;
    break;
  case STARTUP_CHK_CLTS:
    if (dte.getResult() == DTE::EXPECT_RESULT) {
      dte.SendCommand("AT+CENG=3\r", THREADEDGSM_AT_TIMEOUT, "OK\r");
      state = STARTUP_CHK_CENG;
    } else
      state = STARTUP_POWER_OFF;
    break;
  case STARTUP_CHK_CENG:
    if (dte.getResult() == DTE::EXPECT_RESULT) {
      requests |= ((REQ_CLOCK) | (REQ_SIG));
      clearReq(REQ_STARTUP);
      for (int i = 0; i < THREADEDGSM_INTERVAL_COUNT; i++)
        tickSync[i] = millis();
      if (this->configuration.ready != NULL)
        this->configuration.ready(*this);
    } else
      state = STARTUP_POWER_OFF;
    break;
  }
  if (state != lastState) {
    DEBUG_PRINT(F("STARTUP_STATE: "));
    DEBUG_PRINTLN(state);
  }
}

void ThreadedGSM::Clock() {
  String clockTime;
  int lastState = state;
  switch (state) {
  case CLOCK_REQ:
    dte.SendCommand("AT+CCLK?\r", THREADEDGSM_AT_TIMEOUT, "OK\r");
    state = CLOCK_VERIFY;
    break;
  case CLOCK_VERIFY:
    int index = dte.getBuffer().indexOf("+CCLK: ");
    if (index >= 0) {
      // parse clock
      index += 8;
      int endindex;
      endindex = dte.getBuffer().indexOf("+", index);
      if (endindex >= 0)
        clockTime = dte.getBuffer().substring(index, endindex);
      else {
        endindex = dte.getBuffer().indexOf("-", index);
        if (endindex >= 0)
          clockTime = dte.getBuffer().substring(index, endindex);
      }

      if (endindex >= 0) {
        NetworkTime ClockTime;
        ClockTime.year = 2000 + clockTime.substring(0, 2).toInt();
        ClockTime.month = clockTime.substring(3, 5).toInt();
        ClockTime.day = clockTime.substring(6, 8).toInt();
        ClockTime.hour = clockTime.substring(9, 11).toInt();
        ClockTime.minute = clockTime.substring(12, 14).toInt();
        ClockTime.second = clockTime.substring(15, 17).toInt();
        if (this->configuration.clock != NULL)
          this->configuration.clock(*this, ClockTime);
      }
    }
    clearReq(REQ_CLOCK);
    break;
  }
  if (state != lastState) {
    DEBUG_PRINT(F("CLOCK_STATE: "));
    DEBUG_PRINTLN(state);
  }
}

void ThreadedGSM::Signal() {
  int lastState = state;
  switch (state) {
  case SIGNAL_REQ:
    dte.SendCommand("AT+CSQ\r", THREADEDGSM_AT_TIMEOUT, "OK\r");
    state = SIGNAL_VERIFY;
    break;
  case SIGNAL_VERIFY:
    int index = dte.getBuffer().indexOf("+CSQ: ");
    if (index >= 0) {
      // parse signal
      index += 6;
      SignalLevel GsmSignal;
      GsmSignal.Value = dte.getBuffer().substring(index, index + 2).toInt();
      GsmSignal.Dbm = dte.getBuffer().substring(index + 3, index + 5).toInt();
      if (GsmSignal.Value != 0) {
        if (this->configuration.signal != NULL)
          this->configuration.signal(*this, GsmSignal);
      }
    }
    clearReq(REQ_SIG);
    break;
  }
  if (state != lastState) {
    DEBUG_PRINT(F("SIGNAL_STATE: "));
    DEBUG_PRINTLN(state);
  }
}

void ThreadedGSM::clearReq(int req) {
  requests &= ~(req);
  nextJob();
}

void ThreadedGSM::Inbox() {
  String CMD;
  int lastState = state;
  switch (state) {
  case READ_REQ:
    SMS.InboxMsgContents = "";
    dte.SendCommand("AT+CMGF=0\r", THREADEDGSM_AT_TIMEOUT, "OK\r");
    state = READ_CHK_CMGF;
    break;
  case READ_CHK_CMGF:
    if (dte.getResult() == DTE::EXPECT_RESULT) {
      dte.SendCommand("AT+CPMS=\"SM\"\r", THREADEDGSM_AT_TIMEOUT, "OK\r");
      state = READ_CHK_CPMS;
    } else
      clearReq(REQ_INBOX);
    break;
  case READ_CHK_CPMS:
    if (dte.getResult() == DTE::EXPECT_RESULT) {
      CMD = "AT+CMGL=";
      CMD += (int)READ_TYPE_ALL;
      CMD += "\r";
      dte.SendCommand(CMD.c_str(), THREADEDGSM_AT_TIMEOUT, ",");
      state = READ_CHK_CMGL;
    } else
      clearReq(REQ_INBOX);
    break;
  case READ_CHK_CMGL:
    if (dte.getResult() == DTE::EXPECT_RESULT) {
      // fetch index
      int indexStart = dte.getBuffer().indexOf("+CMGL: ");
      if (indexStart >= 0) {
        Message.Index =
            dte.getBuffer()
                .substring(indexStart + 7, dte.getBuffer().indexOf(","))
                .toInt();
        if (Message.Index != 0) {
          dte.Delay(2000);
          state = READ_DELAY_CLEAR_BUFF;
        }
      }
    }
    if (state != READ_DELAY_CLEAR_BUFF)
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
    if (dte.getResult() == DTE::EXPECT_RESULT) {
      int indexStart = dte.getBuffer().indexOf("+CMGR: ");
      if (indexStart >= 0) {
        int indexStartPDU = dte.getBuffer().indexOf("\r\n", indexStart);
        if (indexStartPDU >= 0) {
          indexStartPDU += 2;
          int indexEndPDU = dte.getBuffer().indexOf("\r", indexStartPDU);
          if (indexEndPDU >= 0)
            SMS.InboxMsgContents =
                dte.getBuffer().substring(indexStartPDU, indexEndPDU);
        }
      }
      CMD = "AT+CMGD=";
      CMD += Message.Index;
      CMD += "\r";
      dte.SendCommand(CMD.c_str(), THREADEDGSM_AT_TIMEOUT, "OK\r");
      state = READ_CHK_CMGD;
    } else
      clearReq(REQ_INBOX);
    break;
  case READ_CHK_CMGD:
    if ((dte.getResult() == DTE::EXPECT_RESULT) &&
        (SMS.InboxMsgContents != "")) {
      if (this->configuration.incoming != NULL)
        this->configuration.incoming(*this, SMS.InboxMsgContents);
    }
    clearReq(REQ_INBOX);
    break;
  }
  if (state != lastState) {
    DEBUG_PRINT(F("INBOX_STATE: "));
    DEBUG_PRINTLN(state);
  }
}

void ThreadedGSM::Outbox() {
  String CMD;
  int lastState = state;
  switch (state) {
  case SEND_REQ:
    dte.SendCommand("AT+CMGF=0\r", THREADEDGSM_AT_TIMEOUT, "OK\r");
    state = SEND_CHK_CMGF;
    break;
  case SEND_CHK_CMGF:
    if (dte.getResult() == DTE::EXPECT_RESULT) {
      char smsc_len_b;
      int smsc_length = 0;
      for (int i = 0; i <= 1; i++) {
        smsc_length <<= 4;
        smsc_len_b = SMS.OutboxMsgContents.charAt(i);
        if (smsc_len_b >= '0' && smsc_len_b <= '9')
          smsc_length |= smsc_len_b - '0';
        else if (smsc_len_b >= 'a' && smsc_len_b <= 'f')
          smsc_length |= smsc_len_b - 'a' + 10;
        else if (smsc_len_b >= 'A' && smsc_len_b <= 'F')
          smsc_length |= smsc_len_b - 'A' + 10;
      }
      CMD = "AT+CMGS=";
      CMD += (SMS.OutboxMsgContents.length() / 2 - (smsc_length + 1));
      CMD += "\r";
      dte.SendCommand(CMD.c_str(), 15000, "> ");
      state = SEND_CHK_RDY;
    } else
      clearReq(REQ_OUTBOX);
    break;
  case SEND_CHK_RDY:
    if (dte.getResult() == DTE::EXPECT_RESULT) {
      CMD = SMS.OutboxMsgContents;
      CMD += (char)26;
      dte.SendCommand(CMD.c_str(), 10000, "OK\r");
      state = SEND_CHK_OK;
    } else
      clearReq(REQ_OUTBOX);
    break;
  case SEND_CHK_OK:
    if (dte.getResult() == DTE::EXPECT_RESULT) {
      if (this->configuration.outgoing != NULL)
        this->configuration.outgoing(*this);
    }
    clearReq(REQ_OUTBOX);
    break;
  }
  if (state != lastState) {
    DEBUG_PRINT(F("OUTBOX_STATE: "));
    DEBUG_PRINTLN(state);
  }
}
