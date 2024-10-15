#include "AidaTLU/aida_include/AidaTluDisplay.hh"
#include <chrono>
#include <cmath> // std:: abs
#include <fstream>
#include <iomanip>
#include <iostream>
#include <math.h>
#include <ostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include "uhal/uhal.hpp"

LCD09052::LCD09052() {
    // std::cout << "  AIDA_TLU DISPLAY: Instantiated" << std::endl;
}

LCD09052::LCD09052(i2cCore* thisCore, char thisAddr, unsigned int theseRows, unsigned int theseCols) {
    // std::cout << "  AIDA_TLU DISPLAY: Instantiated" << std::endl;
    disp_i2c_core = thisCore;
    disp_i2c_addr = thisAddr;
    nRows = theseRows;
    nCols = theseCols;
}

void LCD09052::setParameters(i2cCore* mycore, char thisAddr, unsigned int theseRows, unsigned int theseCols) {
    std::cout << "  AIDA_TLU DISPLAY: Setting parameters" << std::endl;
    disp_i2c_core = mycore;
    disp_i2c_addr = thisAddr;
    nRows = theseRows;
    nCols = theseCols;
}

void LCD09052::test() {
    std::cout << "TEST TEST TEST" << std::endl;
}

void LCD09052::setBrightness(unsigned int value) {
    // Sets the brightness level of the backlight.
    // Value is an integer in range [0, 250]. 0= no light, 250= maximum light.
    if(value > 250) {
        std::cout << "\tLCD09052 setBrightness: maximum value= 250. Coherced to 250" << std::endl;
        value = 250;
    }
    int myaddr = 7;
    disp_i2c_core->WriteI2CChar(disp_i2c_addr, myaddr, (unsigned char)value);
}

void LCD09052::clear() {
    // Clears the display and locates the curson on position (1,1), i.e. top left
    int myaddr = 4;
    disp_i2c_core->WriteI2CChar(disp_i2c_addr, myaddr, 0x0);
}

void LCD09052::clearLine(unsigned int line) {
    /// Clear line. Place cursor at beginning of line.
    if((1 <= line) && (line <= nRows)) {
        int myaddr = 3;
        disp_i2c_core->WriteI2CChar(disp_i2c_addr, myaddr, line);
    }
}

void LCD09052::posCursor(unsigned int line, unsigned int pos) {
    // Position the cursor on a specific location
    //  line can be 1 (top) or 2 (bottom)
    //  pos can be [1, 16}
    if(((1 <= line) && (line <= nRows)) && ((1 <= pos) && (pos <= nCols))) {
        int myaddr = 2;
        unsigned char chrsToSend[2];
        chrsToSend[0] = line;
        chrsToSend[1] = pos;
        disp_i2c_core->WriteI2CCharArray(disp_i2c_addr, myaddr, chrsToSend, 2);
    } else {
        std::cout << "Cursor line can only be 1 or 2, position must be in range [1, " << nCols << "]" << std::endl;
    }
}

void LCD09052::pulseLCD(unsigned int nCycles) {
    // Pulse the LCD backlight
    int stepsInCycle = 15;
    float nSteps = stepsInCycle * nCycles;
    float stepSize = M_PI / (stepsInCycle);
    float currVal = 0;
    float iBright;

    for(float iCy = 0; iCy < nSteps; iCy++) {
        currVal = currVal + stepSize;
        // iBright= int(250*abs(cos(currVal)));
        iBright = abs((250 * cos(currVal)));
        // std::cout << "\tGNE GNE GNE " << iCy << " " << currVal << " " << iBright << std::endl;
        setBrightness(iBright);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void LCD09052::writeChar(char mychar) {
    // Writes a char in the current cursor position
    // The cursor is then shifted right one position
    // value must be an integer corresponding to the ascii code of the character
    // Example: mychar= 80 will print a "P"
    unsigned int myaddr = 10;
    disp_i2c_core->WriteI2CChar(disp_i2c_addr, myaddr, mychar);
    return;
}

void LCD09052::writeAll(const std::string& topLine, const std::string& bottomLine) {
    // Clears the display and writes the two lines to it.
    // Lines longer than the display width are trimmed.
    clear();
    // if (topLine.length() > nCols){
    //   topLine.erase(nCols, std::string::npos);
    // }
    writeString(topLine);
    posCursor(2, 1);
    // if (bottomLine.length() > nCols){
    //   bottomLine.erase(nCols, std::string::npos);
    // }
    writeString(bottomLine);
}

void LCD09052::writeString(const std::string& myString) {
    // Convert the string to a char array an writes it to the display.
    // NOTE: No check is performed to ensure the string will fit in the line.
    // The writing starts at the current cursor position.
    unsigned int myaddr = 1;
    int strLen = myString.length();
    // unsigned char *myChars = new char[strLen.length() + 1];
    const char* myChars = myString.c_str();
    // strcpy( static_cast <char*>( myChars ), myString );
    // unsigned char myChars[] = { 'H', 'e', 'l', 'l', 'o' };
    disp_i2c_core->WriteI2CCharArray(disp_i2c_addr, myaddr, (unsigned char*)myChars, strLen);
}
