#include "AidaTluI2c.hh"
#include <iostream>
#include <ostream>
#include <thread>
#include <vector>
#include "uhal/uhal.hpp"

i2cCore::i2cCore(HwInterface * hw_int){
  i2c_hw = hw_int;
}

void i2cCore::SetWRegister( const std::string & regname, int value)
{
  try {
    i2c_hw->getNode(regname).write(static_cast< uint32_t >(value));
    i2c_hw->dispatch();
  } catch (...) {
    return;
  }
}

uint32_t i2cCore::ReadRRegister( const std::string & regname)
{
  try {
    ValWord< uint32_t > test = i2c_hw->getNode(regname).read();
    i2c_hw->dispatch();
    if(test.valid()) {
      return test.value();
    } else {
      std::cout << "Error reading " << regname << std::endl;
      return 0;
    }
  } catch (...) {
    return 0;
  }
}

uint32_t i2cCore::GetI2CStatus() {
  return ReadRRegister("i2c_master.i2c_cmdstatus");
};

uint32_t i2cCore::GetI2CRX() {
  return ReadRRegister("i2c_master.i2c_rxtx");
};

void i2cCore::SetI2CControl(int value) {
  SetWRegister("i2c_master.i2c_ctrl", value&0xff);
};

void i2cCore::SetI2CCommand(int value) {
  SetWRegister("i2c_master.i2c_cmdstatus", value&0xff);
};

void i2cCore::SetI2CTX(int value) {
  SetWRegister("i2c_master.i2c_rxtx", value&0xff);
};

bool i2cCore::I2CCommandIsDone() {
  return ((GetI2CStatus())>>1)&0x1;
};

void i2cCore::SetI2CClockPrescale(int value) {
  SetWRegister("i2c_master.i2c_pre_lo", value&0xff);
  SetWRegister("i2c_master.i2c_pre_hi", (value>>8)&0xff);
};


char i2cCore::ReadI2CChar(char deviceAddr, char memAddr) {
  SetI2CTX((deviceAddr<<1)|0x0);
  SetI2CCommand(0x90);
  while(I2CCommandIsDone()) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
  SetI2CTX(memAddr);
  SetI2CCommand(0x10);
  while(I2CCommandIsDone()) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
  SetI2CTX((deviceAddr<<1)|0x1);
  SetI2CCommand(0x90);
  while(I2CCommandIsDone()) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
  SetI2CCommand(0x28);
  while(I2CCommandIsDone()) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
  return GetI2CRX();
}

void i2cCore::WriteI2CChar(char deviceAddr, char memAddr, char value) {
  SetI2CTX((deviceAddr<<1)|0x0);
  SetI2CCommand(0x90);
  while(I2CCommandIsDone()) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
  SetI2CTX(memAddr);
  SetI2CCommand(0x10);
  while(I2CCommandIsDone()) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
  SetI2CTX(value);
  SetI2CCommand(0x50);
  while(I2CCommandIsDone()) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
}

void i2cCore::WriteI2CCharArray(char deviceAddr, char memAddr, unsigned char *values, unsigned int len) {
  unsigned int i;

  SetI2CTX((deviceAddr<<1)|0x0);
  SetI2CCommand(0x90);
  while(I2CCommandIsDone()) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
  SetI2CTX(memAddr);
  SetI2CCommand(0x10);
  while(I2CCommandIsDone()) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
  for(i = 0; i < len-1; ++i) {
    SetI2CTX(values[i]);
    SetI2CCommand(0x10);
    while(I2CCommandIsDone()) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  }
  SetI2CTX(values[len-1]);
  SetI2CCommand(0x50);
  while(I2CCommandIsDone()) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
}
