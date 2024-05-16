#ifndef H_AIDATLUPOWERMODULE_HH
#define H_AIDATLUPOWERMODULE_HH

#include <string>
#include <iostream>
#include <vector>
#include "AidaTluI2c.hh"
#include "AidaTluHardware.hh"
#include <array>

class PWRLED{
private:
  char pwr_i2c_DACaddr;
  char pwr_i2c_exp1Add;
  char pwr_i2c_exp2Add;
  char pwr_i2c_eeprom;
  i2cCore * pwr_i2c_core;
  AD5665R pwr_zeDAC;
  PCA9539PW pwr_ledExp1, pwr_ledExp2;
  std::array<std::array<int, 3>, 11> indicatorXYZ;

  /*
  std::array<std::array<int, 3>, 11> indicatorXYZ { { { {30, 29, 31} }, { {27, 26, 28} }, { {24, 23, 25} },
                                                      { {21, 20, 22} }, { {18, 17, 19} }, { {15, 14, 16} },
                                                      { {12, 11, 13} }, { {  9, 8, 10} }, { {   6, 5, 7} },
                                                      { {3, 2, 4} }, { {1, 0, -1} } } };
  */
  //i2cCore myi2c;
public:
  //AD5665R::AD5665R()
  PWRLED();
  PWRLED( i2cCore  *mycore , char DACaddr, char Exp1Add, char Exp2Add, char IdAdd);
  void led_allBlue();
  void led_allGreen();
  void led_allRed();
  void led_allOff();
  void led_allWhite();
  void initI2Cslaves(bool intRef, uint8_t verbose);
  uint32_t _set_bit(uint32_t v, int index, bool x);
  void setIndicatorRGB(int indicator, const std::array<int, 3>& RGB, uint8_t verbose);
  void setI2CPar( i2cCore  *mycore , char DACaddr, char Exp1Add, char Exp2Add, char IdAdd, uint8_t verbose);
  void setVchannel(int channel, float voltage, uint8_t verbose);
  void testme();
  void testLED();
  //void SetIntRef(bool intRef, bool verbose);
  //void SetDACValue(unsigned char channel, uint32_t value);
};
#endif
