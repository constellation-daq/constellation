#ifndef H_AIDATLUHARDWARE_HH
#define H_AIDATLUHARDWARE_HH

#include <string>
#include <iostream>
#include <vector>
#include "AidaTluI2c.hh"

/*
  This file contains classes for chips used on the new TLU design.
  Paolo.Baesso@bristol.ac.uk - 2017
*/
//namespace tluHw{
//////////////////////////////////////////////////////////////////////
class AD5665R{
private:
  char m_i2cAddr;
  int m_DACn;
  i2cCore * m_DACcore;
  //i2cCore myi2c;
public:
  //AD5665R::AD5665R()
  AD5665R();
  void testme();
  void SetI2CPar( i2cCore  *mycore , char addr);
  void SetIntRef(bool intRef, uint8_t verbose);
  void SetDACValue(unsigned char channel, uint32_t value, uint8_t verbose);
};
//////////////////////////////////////////////////////////////////////
class PCA9539PW{
private:
  char m_i2cAddr;
  i2cCore * m_IOXcore;

  void writeReg(unsigned int memAddr, unsigned char regContent, uint8_t verbose);
  char readReg(unsigned int memAddr, uint8_t verbose);
public:
  PCA9539PW();
  void SetI2CPar( i2cCore  *mycore , char addr);
  void setInvertReg(unsigned int memAddr, unsigned char polarity, uint8_t verbose);
  char getInvertReg(unsigned int memAddr, uint8_t verbose);
  void setIOReg(unsigned int memAddr, unsigned char direction, uint8_t verbose);
  char getIOReg(unsigned int memAddr, uint8_t verbose);
  char getInputs(unsigned int memAddr, uint8_t verbose);
  void setOutputs(unsigned int memAddr, unsigned char direction, uint8_t verbose);
  char getOutputs(unsigned int memAddr, uint8_t verbose);
};
//////////////////////////////////////////////////////////////////////
class Si5345{
private:
  char m_i2cAddr;
  i2cCore * m_Clkcore;

  void setPage(unsigned int page, uint8_t verbose);
  char getPage(uint8_t verbose);

public:
  Si5345();
  void SetI2CPar( i2cCore  *mycore , char addr);
  unsigned int getDeviceVersion(uint8_t verbose);
  char readRegister(unsigned int myaddr, uint8_t verbose);
  void writeRegister(unsigned int myaddr, unsigned char data, uint8_t verbose);
  std::string checkDesignID(uint8_t verbose);
  std::vector< std::vector< unsigned int> > parseClkFile(const std::string & filename, uint8_t verbose);
  void writeConfiguration(std::vector< std::vector< unsigned int> > regSetting, uint8_t verbose);
};

//}
#endif
