/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ThreadedGSM.h                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: Neta Yahav <neta540@gmail.com>             +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2016/09/20 11:14:02 by Neta Yahav        #+#    #+#             */
/*   Updated: 2020/07/31 18:36:30 by Neta Yahav       ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef __THREADEDGSM_H__
#define __THREADEDGSM_H__

#include "DTE.h"
#include <Arduino.h>

// Defaults
#define THREADEDGSM_DEF_DTE_BUF_SIZ 512
#define THREADEDGSM_DEF_AT_TIMEOUT 2000
#define THREADEDGSM_DEF_STA_PON 15000
#define THREADEDGSM_DEF_STA_POF 1000

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

#define THREADEDGSM_INTERVAL_COUNT 3

#ifdef THREADEDGSM_DEBUG
#define DEBUG_PRINT(x) THREADEDGSM_DEBUG.print(x)
#define DEBUG_PRINTLN(x) THREADEDGSM_DEBUG.println(x)
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINTLN(x)
#endif

class ThreadedGSM {
  // variables
public:
  struct NetworkTime {
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
  enum IntervalSourceE { INTERVAL_CLOCK, INTERVAL_INBOX, INTERVAL_SIGNAL };
  struct SignalLevel {
    int Dbm;
    int Value;
  };

  typedef void (*ThreadedGSMCallbackSignal)(ThreadedGSM &, SignalLevel &);
  typedef void (*ThreadedGSMCallbackClock)(ThreadedGSM &, NetworkTime &);
  typedef void (*ThreadedGSMCallbackIncomingSMS)(ThreadedGSM &, String &);
  typedef void (*ThreadedGSMCallbackBool)(ThreadedGSM &, bool);
  typedef void (*ThreadedGSMCallback)(ThreadedGSM &);
  struct conf {
    ThreadedGSMCallbackSignal signal;
    ThreadedGSMCallbackClock clock;
    ThreadedGSMCallbackIncomingSMS incoming;
    ThreadedGSMCallback ready;
    ThreadedGSMCallback outgoing;
    ThreadedGSMCallbackBool power;
  };

protected:
private:
  enum StatesStartup {
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

  enum StatesClock { CLOCK_REQ, CLOCK_VERIFY };

  enum StatesSignal { SIGNAL_REQ, SIGNAL_VERIFY };

  enum StatesInbox {
    READ_REQ,
    READ_CHK_CMGF,
    READ_CHK_CPMS,
    READ_CHK_CMGL,
    READ_DELAY_CLEAR_BUFF,
    READ_CHK_CMGR,
    READ_CHK_CMGD
  };

  enum StatesOutbox { SEND_REQ, SEND_CHK_CMGF, SEND_CHK_RDY, SEND_CHK_OK };

  unsigned long tick;

  struct {
    int Index;
  } Message;

  struct {
    String InboxMsgContents;
    String OutboxMsgContents;
  } SMS;

  Stream &stream;
  DTE dte;

  unsigned long tickSync[THREADEDGSM_INTERVAL_COUNT];
  unsigned long Intervals[THREADEDGSM_INTERVAL_COUNT];

  // callbacks
  conf configuration = {NULL, NULL, NULL, NULL, NULL, NULL};

  enum ReqTypes {
    REQ_CLOCK = 1,
    REQ_SIG = 2,
    REQ_INBOX = 4,
    REQ_OUTBOX = 8,
    REQ_STARTUP = 16
  };
  int requests;
  int state;
  int job;
  // functions
public:
  ThreadedGSM(Stream &stream);
  ~ThreadedGSM();
  void nextJob();
  void setHandlers(conf config);
  void setInterval(IntervalSourceE source, unsigned long interval);
  // Initialization
  void begin();
  // Call this function for executing thread
  void loop();
  // Requests
  void sendSMS(String &PDU);

protected:
private:
  // States
  void Startup();

  // Threads
  void Clock();
  void Signal();
  void clearReq(int req);
  void Inbox();
  void Outbox();
}; // ThreadedGSM

#endif //__THREADEDGSM_H__
