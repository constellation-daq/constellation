#include "aida_include/AidaTluHardware.hh"
#include <iostream>
#include <ostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <string>
#include <iomanip>
#include "uhal/uhal.hpp"

/*
  This file contains classes for chips used on the new TLU design.
  Paolo.Baesso@bristol.ac.uk - 2017
*/

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
// DAC

void AD5665R::testme(){
  std::cout << "TEST TEST TEST" << std::endl;
}

AD5665R::AD5665R(){

}

void AD5665R::SetI2CPar( i2cCore *mycore, char addr ) {
  m_DACcore = mycore;
  m_i2cAddr= addr;
}

void AD5665R::SetIntRef(bool intRef, uint8_t verbose){
  // Configure the voltage reference for the DACs.
  // Normally we want to use the external one
  std::string stat;
  unsigned char chrsToSend[2];
  chrsToSend[0]= 0x00;

  if (intRef) {
    stat= "INTERNAL";
    chrsToSend[1]= 0x01;
    //cmdDAC= [0x38,0x00,0x01]
  }
  else{
    stat= "EXTERNAL";
    chrsToSend[1]= 0x00;
    //cmdDAC= [0x38,0x00,0x00]
  }
  m_DACcore->WriteI2CCharArray(m_i2cAddr, 0x38, chrsToSend, 2);
  if (verbose > 0){
    std::cout << "  DAC (0x"<< std::hex<< (int)m_i2cAddr << std::dec <<") reference set to " << stat << std::endl;
  }
}

void AD5665R::SetDACValue(unsigned char channel, uint32_t value, uint8_t verbose){
  unsigned char chrsToSend[2];

  if (verbose > 1){
    std::cout << "\tSetting DAC channel " << (unsigned int)channel << " to " << value << std::endl;
  }
  if (( (unsigned int)channel < 0 ) || ( 7 < (unsigned int)channel )){
    std::cout << "\tAD5665R - ERROR: channel " << int(channel)  << " not in range 0-7" << std::endl;
    return;
  }

  if (value<0){
    std::cout << "\tAD5665R - ERROR: value" << value << "<0. Default to 0" << std::endl;
    value=0;
  }
  if (value>0xFFFF){
    std::cout << "\tAD5665R - ERROR: value" << value << ">0xFFFF. Default to 0xFFFF" << std::endl;
    value=0xFFFF;
  }
  // set the value
  chrsToSend[1] = value&0xff;
  chrsToSend[0] = (value>>8)&0xff;
  m_DACcore->WriteI2CCharArray(m_i2cAddr, 0x18+(channel&0x7), chrsToSend, 2);//here
}


//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
// I/O EXPANDER

PCA9539PW::PCA9539PW(){}

void PCA9539PW::SetI2CPar( i2cCore *mycore, char addr ) {
  m_IOXcore = mycore;
  m_i2cAddr= addr;
}

void PCA9539PW::writeReg(unsigned int memAddr, unsigned char regContent, uint8_t verbose){
  //Basic functionality to write to register.
  if ((memAddr < 0) || (memAddr > 7)){
    std::cout << "PCA9539PW - ERROR: register number should be in range [0:7]" << std::endl;
    return;
  }
  regContent= regContent & 0xFF;
  m_IOXcore->WriteI2CChar(m_i2cAddr, (unsigned char)memAddr, (unsigned char)regContent);
}

char PCA9539PW::readReg(unsigned int memAddr, uint8_t verbose){
  //Basic functionality to read from register.
  char res;
  if ((memAddr < 0) || (memAddr > 7)){
    std::cout << "PCA9539PW - ERROR: register number should be in range [0:7]" << std::endl;
    return -1;
  }
  res = m_IOXcore->ReadI2CChar(m_i2cAddr, (unsigned char)memAddr);
  return res;
}

//The functions below should really be implemented with just one function and a switch/case...
void PCA9539PW::setInvertReg(unsigned int memAddr, unsigned char polarity= 0x00, uint8_t verbose=0){
  //Set the content of register 4 or 5 which determine the polarity of the
  //ports (0= normal, 1= inverted).
  if ((memAddr != 0) && (memAddr != 1)){
    std::cout << "PCA9539PW - ERROR: regN should be 0 or 1" << std::endl;
    return;
  }
  if (verbose > 2){
    std::cout << "\tPCA9539PW - Setting register " << (int)memAddr+4 << " to " << std::hex << (int)polarity << std::dec<< std::endl;
  }
  polarity = polarity & 0xFF;
  m_IOXcore->WriteI2CChar(m_i2cAddr, (unsigned char)memAddr+4, (unsigned char)polarity);
}

char PCA9539PW::getInvertReg(unsigned int memAddr, uint8_t verbose){
  //Read the content of register 4 or 5 which determine the polarity of the
  //ports (0= normal, 1= inverted).
  char res;
  if ((memAddr != 0) && (memAddr != 1)){
    std::cout << "PCA9539PW - ERROR: regN should be 0 or 1" << std::endl;
    return -1;
  }
  res= m_IOXcore->ReadI2CChar(m_i2cAddr, (unsigned char)memAddr+4);
  return res;
}

void PCA9539PW::setIOReg(unsigned int memAddr, unsigned char direction= 0xFF, uint8_t verbose=0){
  //Set the content of register 6 or 7 which determine the direction of the
  //ports (0= output, 1= input).
  if ((memAddr != 0) && (memAddr != 1)){
    std::cout << "PCA9539PW - ERROR: regN should be 0 or 1" << std::endl;
    return;
  }
  if (verbose > 2){
    std::cout << "\tPCA9539PW - Setting register " << (int)memAddr+6 << " to " << std::hex <<(int)direction << std::dec<< std::endl;
  }
  direction = direction & 0xFF;
  m_IOXcore->WriteI2CChar(m_i2cAddr, (unsigned char)memAddr+6, (unsigned char)direction);
}

char PCA9539PW::getIOReg(unsigned int memAddr,uint8_t  verbose){
  //Read the content of register 6 or 7 which determine the direction of the
  //ports (0= output, 1= input).
  char res;
  if ((memAddr != 0) && (memAddr != 1)){
    std::cout << "PCA9539PW - ERROR: regN should be 0 or 1" << std::endl;
    return -1;
  }
  res= m_IOXcore->ReadI2CChar(m_i2cAddr, (unsigned char)memAddr+6);
  return res;
}

char PCA9539PW::getInputs(unsigned int memAddr, uint8_t verbose){
  //Read the incoming values of the pins for one of the two 8-bit banks.
  char res;
  if ((memAddr != 0) && (memAddr != 1)){
    std::cout << "PCA9539PW - ERROR: regN should be 0 or 1" << std::endl;
    return -1;
  }
  res= m_IOXcore->ReadI2CChar(m_i2cAddr, (unsigned char)memAddr);
  return res;
}

void PCA9539PW::setOutputs(unsigned int memAddr, unsigned char direction= 0xFF, uint8_t verbose=0){
  //Set the content of register 6 or 7 which determine the direction of the
  //ports (0= output, 1= input).
  if ((memAddr != 0) && (memAddr != 1)){
    std::cout << "PCA9539PW - ERROR: regN should be 0 or 1" << std::endl;
    return;
  }
  if (verbose > 2){
    std::cout << "PCA9539PW - Setting register " << (int)memAddr+2 << " to " << std::hex << (int)direction << std::dec<< std::endl;
  }
  direction = direction & 0xFF;
  m_IOXcore->WriteI2CChar(m_i2cAddr, (unsigned char)memAddr+2, (unsigned char)direction);
}

char PCA9539PW::getOutputs(unsigned int memAddr, uint8_t verbose){
  //Read the incoming values of the pins for one of the two 8-bit banks.
  char res;
  if ((memAddr != 0) && (memAddr != 1)){
    std::cout << "PCA9539PW - ERROR: regN should be 0 or 1" << std::endl;
    return -1;
  }
  res= m_IOXcore->ReadI2CChar(m_i2cAddr, (unsigned char)memAddr+2);
  return res;
}


//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
// CLOCK CHIP

Si5345::Si5345(){}

void Si5345::SetI2CPar( i2cCore *mycore, char addr ) {
  m_Clkcore = mycore;
  m_i2cAddr= addr;
}

void Si5345::setPage(unsigned int page, uint8_t verbose){
  //Configure the chip to perform operations on the specified address page.
  m_Clkcore->WriteI2CChar(m_i2cAddr, 0x01, (unsigned char)page);
  if (verbose > 2){
    std::cout << "  Si5345 Set Reg Page: " <<  page << std::endl;
  }
}

char Si5345::getPage(uint8_t verbose){
  //Read the current address page
  char res;
  res= m_Clkcore->ReadI2CChar(m_i2cAddr, 0x01);
  if (verbose > 2){
    std::cout << "\tPage read: " << res << std::endl;
  }
  return res;
}

unsigned int Si5345::getDeviceVersion(uint8_t verbose){
  //Read registers containing chip information
  int nwords=2;
  unsigned char myaddr = 0x02;
  unsigned int chipID;
  setPage(0, verbose);
  for(int i=0; i< nwords; i++) {
    char nibble = m_Clkcore->ReadI2CChar(m_i2cAddr, myaddr);
    chipID = ((((uint64_t)nibble)&0xff)<<(i*8))|chipID;
    myaddr++;
  }
  if (verbose > 0){
    std::cout <<  "  Si5345 EPROM: " << std::endl;
    std::cout << std::hex << "\t" <<chipID << std::dec<< std::endl;
  }
  return chipID;
}

char Si5345::readRegister(unsigned int myaddr, uint8_t verbose){
  //Read a specific register on the Si5344 chip. There is not check on the validity of the address but
  //the code sets the correct page before reading.

  //First make sure we are on the correct page
  char currentPg= getPage(verbose);
  char requirePg;
  requirePg= (myaddr & 0xFF00) >> 8;
  if (verbose > 2){
    std::cout << "REG" << std::hex << (int)myaddr <<  " CURR PG " << (int)currentPg << " REQ PG " << (int)requirePg << std::dec << std::endl;
  }
  if (currentPg != requirePg){
    setPage(requirePg, verbose);
  }
  //Now read from register
  char res = m_Clkcore->ReadI2CChar(m_i2cAddr, myaddr);
  return res;
}


void Si5345::writeRegister(unsigned int myaddr, unsigned char data, uint8_t verbose){
  /// Write a specific register on the Si5344 chip. There is not check on the validity of the address but
  /// the code sets the correct page before reading.

  //First make sure we are on the correct page
  myaddr= myaddr & 0xFFFF;
  char currentPg= getPage(verbose);
  char requirePg;
  requirePg= (myaddr & 0xFF00) >> 8;
  if (currentPg != requirePg){
    setPage(requirePg, verbose);
  }
  //Now write to register
  m_Clkcore->WriteI2CChar(m_i2cAddr, myaddr, (unsigned char)data);
  if (verbose > 2){
    std::cout << "  Writing: 0x" << std:: hex << (int)data << " to reg 0x" << (int)myaddr << std::dec << std:: endl;
  }
}

std::string Si5345::checkDesignID(uint8_t verbose){
  unsigned int regAddr= 0x026B;
  int nWords= 8;
  std::vector<char> resVec;
  char res;
  std::stringstream ss;

  for (int i=0; i< nWords; i++){
    res= readRegister(regAddr, verbose);
    resVec.push_back(res);
    regAddr++;
  }
  //std::cout << "  Si5345 design Id:\n\t" ;
  for (int i=0; i< nWords; i++){
    //std::cout << resVec[i] ;
    ss << resVec[i] ;
  }
  //std::cout  << std::endl;
  std::string desID = ss.str();
  return desID;
}

std::vector< std::vector< unsigned int> > Si5345::parseClkFile(const std::string & filename, uint8_t verbose){
  //Parse the configuration file produced by Clockbuilder Pro (Silicon Labs)
  //Returns a 2-dimensional vector with each row being a couple (ADDRESS, DATA)
  //It could be improved by using pairs. Not sure if the library can be included.
  std::vector< std::vector< unsigned int> > regSetting;
  if (verbose > 0){
    std::cout << "  Parsing clock configuration file:" << std::endl;
    std::cout << "\t" << filename << std::endl;
  }
  std::ifstream myfile(filename);
  if (myfile.good()){
    std::string onerow;
    while (std::getline(myfile, onerow)){
      // Process line by line
      std::vector< unsigned int > tmpVec;
      std::string regAddress;
      std::string regData;
      if (onerow.at(0) != '#'){//Skip comment lines
	size_t pos = onerow.find(',');
	regAddress = onerow.substr(0, pos);
	regData   = onerow.substr(pos + 1);
	if ( regAddress != "Address"){
	  unsigned int new_address = std::stoul(regAddress, nullptr, 16);
	  unsigned int new_data = std::stoul(regData, nullptr, 16);
	  if (verbose > 2){
	    std::cout << "\tAddress " << std::hex << new_address << " --- Data " << new_data <<  std::endl;
	  }
	  tmpVec.push_back(new_address);
	  tmpVec.push_back(new_data);
	  regSetting.push_back(tmpVec);
	}
      }
    }
  }
  else{
    std::cout << "\tSi5345 - ERROR: Problem with the configuration file. Make sure the file exists and the path is correct." << std::endl;
    return regSetting;
  }

  std::cout << std::dec;
  return regSetting;
}


void Si5345::writeConfiguration(std::vector< std::vector< unsigned int> > regSetting, uint8_t verbose){
  std::ios::fmtflags coutflags( std::cout.flags() );// Store cout flags to be able to restore them

  std::cout << "  Si5345 Writing configuration (" << regSetting.size() << " registers):" << std::endl;
  //std::cout << regSetting[0].size() << std::endl;
  int nRows= regSetting.size();
  if (nRows== 0){
    std::cout << "\tSi5345 - ERROR: empty configuration list. Clock chip not configured." << std::endl;
    return;
  }
  std::cout.setf ( std::ios::hex, std::ios::basefield );  // set hex as the basefield
  std::cout.setf ( std::ios::showbase ); // show radix
  for(int iRow=0; iRow < nRows; iRow++){
    if (verbose > 2){
      std::cout << "\tADDR \t" << std::hex << regSetting[iRow][0] << " DATA \t " <<  regSetting[iRow][1] << std::endl;
    }
    std::cout << std::dec<< "\r\t" << std::setw(3) << std::setfill('0') << iRow+1  << "/"<< regSetting.size() << std::flush;
    writeRegister(regSetting[iRow][0], (unsigned char)regSetting[iRow][1], false);
  }
  std::cout << "\tSuccess" << std::endl;
  std::cout.flags( coutflags ); // Restore cout flag
}
