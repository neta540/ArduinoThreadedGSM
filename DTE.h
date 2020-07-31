/*
 * DTE.h
 *
 * Created: 20/09/2016 15:40:51
 * Author: Neta Yahav
 */

#ifndef __DTE_H__
#define __DTE_H__

#include <Arduino.h>

class DTE {
  // variables
public:
  enum CommandResult {
    EXPECT_BUSY,
    EXPECT_TIMEOUT,
    EXPECT_DELAY,
    EXPECT_RESULT
  };

protected:
private:
  String buffer;
  Stream &stream;
  unsigned int bufferIndex;
  unsigned int bufferSize;
  String response[3];
  unsigned long timeout;
  unsigned long tick;
  unsigned int match;
  CommandResult result;
  // functions
public:
  DTE(Stream &stream, unsigned int size);
  ~DTE(){};
  void SendCommand(const char *command, unsigned long timeout,
                   const char *response1, const char *response2 = 0,
                   const char *response3 = 0);
  void Delay(unsigned long delay);
  bool getIsBusy();
  CommandResult getResult();
  unsigned int getMatch();
  String &getBuffer();

protected:
private:
  void proccess();
}; // DTE

#endif //__DTE_H__
