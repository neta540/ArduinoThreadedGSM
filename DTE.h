/*
* DTE.h
*
* Created: 20/09/2016 15:40:51
* Author: Neta Yahav
*/

#ifndef __DTE_H__
#define __DTE_H__

#include <Arduino.h>

class DTE
{
//variables
public:
	enum CommandResult
	{
		EXPECT_BUSY,
		EXPECT_TIMEOUT,
		EXPECT_DELAY,
		EXPECT_RESULT
	};

protected:
private:
	String buffer;
	Stream& stream;
	unsigned int bufferIndex;
	unsigned int bufferSize;
	String response[3];
	unsigned long timeout;
	unsigned long tick;
	unsigned int match;
	CommandResult result;
//functions
public:
	DTE(Stream& stream, unsigned int size) : stream(stream), bufferSize(size), result(EXPECT_RESULT) { buffer.reserve(size); }
	~DTE() {};
	void SendCommand(const char* command, unsigned long timeout, const char* response1, const char* response2 = 0, const char* response3 = 0)
	{
		match = 0;
		result = EXPECT_BUSY;
		response[0] = response1;
		response[1] = response2;
		response[2] = response3;
		this->timeout = timeout;
		// clear rx buffer
		while (stream.available())
		{
			stream.read();
		}
		// send command
		stream.print(command);
		buffer = "";
		tick = millis();
		proccess();
	}
	void Delay(unsigned long delay)
	{
		timeout = delay;
		result = EXPECT_DELAY;
		tick = millis();
		proccess();
	}
	bool getIsBusy()
	{
		proccess();
		return (result == EXPECT_BUSY) || (result == EXPECT_DELAY);
	}
	CommandResult getResult() { return result ; }
	unsigned int getMatch() { return match; }
	String& getBuffer() { return buffer; }
protected:
private:
	void proccess()
	{
		if(result == EXPECT_DELAY)
		{
			if(millis() - tick >= timeout)
				result = EXPECT_RESULT;

			return;
		}

		if(result != EXPECT_BUSY) return;

		char c;
		unsigned long now = millis();

		while(millis() - tick < timeout)
		{
			while(stream.available() && (buffer.length() < (bufferSize)))
			{
				c = stream.read();
				buffer += c;
				if(buffer.endsWith(response[0]))
				{
					match = 0;
					result = EXPECT_RESULT;
					return;
				}else if(response[1].length() != 0)
				{
					if(buffer.endsWith(response[1]))
					{
						match = 1;
						result = EXPECT_RESULT;
						return;
					}
				}else if(response[2].length() != 0)
				{
					if(buffer.endsWith(response[2]))
					{
						match = 2;
						result = EXPECT_RESULT;
						return;
					}
				}
			}
			if(millis() - now > 5)
				return;
		}

		// time out
		result = EXPECT_TIMEOUT;
	}
}; //DTE

#endif //__DTE_H__
