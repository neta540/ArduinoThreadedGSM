/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   DTE.cpp                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: Neta Yahav <neta540@gmail.com>             +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2020/07/31 17:53:23 by Neta Yahav        #+#    #+#             */
/*   Updated: 2020/07/31 17:59:45 by Neta Yahav       ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "DTE.h"

DTE::DTE(Stream &stream, unsigned int size)
    : stream(stream), bufferSize(size), result(EXPECT_RESULT) {
  buffer.reserve(size);
}

void DTE::SendCommand(const char *command, unsigned long timeout,
                      const char *response1, const char *response2,
                      const char *response3) {
  match = 0;
  result = EXPECT_BUSY;
  response[0] = response1;
  response[1] = response2;
  response[2] = response3;
  this->timeout = timeout;
  // clear rx buffer
  while (stream.available()) {
    stream.read();
  }
  // send command
  stream.print(command);
  buffer = "";
  tick = millis();
  proccess();
}

void DTE::Delay(unsigned long delay) {
  timeout = delay;
  result = EXPECT_DELAY;
  tick = millis();
  proccess();
}

bool DTE::getIsBusy() {
  proccess();
  return (result == EXPECT_BUSY) || (result == EXPECT_DELAY);
}

DTE::CommandResult DTE::getResult() { return result; }

unsigned int DTE::getMatch() { return match; }

String &DTE::getBuffer() { return buffer; }

void DTE::proccess() {
  if (result == EXPECT_DELAY) {
    if (millis() - tick >= timeout)
      result = EXPECT_RESULT;

    return;
  }

  if (result != EXPECT_BUSY)
    return;

  char c;
  unsigned long now = millis();

  while (millis() - tick < timeout) {
    while (stream.available() && (buffer.length() < (bufferSize))) {
      c = stream.read();
      buffer += c;
      if (buffer.endsWith(response[0])) {
        match = 0;
        result = EXPECT_RESULT;
        return;
      } else if (response[1].length() != 0) {
        if (buffer.endsWith(response[1])) {
          match = 1;
          result = EXPECT_RESULT;
          return;
        }
      } else if (response[2].length() != 0) {
        if (buffer.endsWith(response[2])) {
          match = 2;
          result = EXPECT_RESULT;
          return;
        }
      }
    }
    if (millis() - now > 5)
      return;
  }

  // time out
  result = EXPECT_TIMEOUT;
}
