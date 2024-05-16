#ifndef H_AIDATLUDISPLAY_HH
#define H_AIDATLUDISPLAY_HH

#include <string>
#include <iostream>
#include <vector>
#include "AidaTluI2c.hh"

class LCD09052{
  /*
  Class to control the LCD display installed on the 19" rack unit of the TLU.
  We tested various displays but for now settled on the SparkFun LCD-09052 and them
  backpack I2CLCDBPV2.
  */
private:
  i2cCore * disp_i2c_core;
  char disp_i2c_addr;
  unsigned int nRows;
  unsigned int nCols;

public:
  LCD09052();
  LCD09052(i2cCore * thisCore, char thisAddr, unsigned int theseRows, unsigned int theseCols);
  void setParameters( i2cCore  *mycore , char thisAddr, unsigned int theseRows, unsigned int theseCols);
  void test();

  //void dispString();
  //void writeString();
  void posCursor(unsigned int line, unsigned int pos);
  void clearLine(unsigned int line);
  void clear();
  //void setLCDtype();
  void setBrightness(unsigned int value);
  void writeChar(char mychar);
  void writeString(const std::string & myString);
  void writeAll(const std::string & topLine, const std::string & bottomLine);
  //void createChar();
  //void writeSomething();
  void pulseLCD(unsigned int nCycles);
};

#endif
