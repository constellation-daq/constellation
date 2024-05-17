#ifndef H_I2CBUS_HH
#define H_I2CBUS_HH

#include <cstdint>
#include <deque>
#include <string>
#include <iostream>

namespace uhal{
  class HwInterface;
}
using namespace uhal;

class i2cCore{

public:
  i2cCore(HwInterface * hw_int);

  ~i2cCore(){};
  void SetWRegister( const std::string & regname, int value);
  uint32_t ReadRRegister( const std::string & regname);

  uint32_t GetI2CStatus();
  uint32_t GetI2CRX();
  void SetI2CControl(int value);
  void SetI2CCommand(int value);
  void SetI2CTX(int value);
  bool I2CCommandIsDone();

  void SetI2CClockPrescale(int value);
  char ReadI2CChar(char deviceAddr, char memAddr);
  void WriteI2CChar(char deviceAddr, char memAddr, char value);
  void WriteI2CCharArray(char deviceAddr, char memAddr, unsigned char *values, unsigned int len);

private:
  HwInterface * i2c_hw;
};
#endif
