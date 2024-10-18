#ifndef H_AIDATLUCONTROLLER_HH
#define H_AIDATLUCONTROLLER_HH

#include <cstdint>
#include <deque>
#include <iostream>
#include <string>
#include <vector>
#include "AidaTluDisplay.hh"
#include "AidaTluHardware.hh"
#include "AidaTluI2c.hh"
#include "AidaTluPowerModule.hh"
#include "constellation/core/log/Logger.hpp"

typedef unsigned char uchar_t;

namespace uhal {
    class HwInterface;
}
using namespace uhal;
using namespace constellation;

namespace tlu {

    class fmctludata;

    class AidaTluController {
    public:
        AidaTluController(const std::string& connectionFilename, const std::string& deviceName);
        ~AidaTluController() { ResetEventsBuffer(); };

        void compareWriteRead(uint32_t written, uint32_t readback, uint32_t mask, const std::string& regName);
        void configureHDMI(unsigned int hdmiN, unsigned int enable, uint8_t verbose);
        void enableHDMI(unsigned int dutN, bool enable, uint8_t verbose);
        uint32_t PackBits(std::vector<unsigned int> rawValues);
        void SetSerdesRst(int value) { SetWRegister("triggerInputs.SerdesRstW", value); };
        void SetInternalTriggerInterval(int value) { SetWRegister("triggerLogic.InternalTriggerIntervalW", value); };
        void SetInternalTriggerFrequency(uint32_t user_freq, uint8_t verbose);
        // void SetTriggerMask(int value) { SetWRegister("triggerLogic.TriggerMaskW",value); };
        void SetTriggerMask(uint64_t value);
        void SetTriggerMask(uint32_t maskHi, uint32_t maskLo);
        void SetTriggerPolarity(uint64_t value);
        // void SetTriggerVeto(int value) { SetWRegister("triggerLogic.TriggerVetoW",value); };
        void SetTriggerVeto(int value);
        void SetPulseStretch(int value) { SetWRegister("triggerLogic.PulseStretchW", value); };
        void SetPulseDelay(int value) { SetWRegister("triggerLogic.PulseDelayW", value); };
        void SetPulseStretchPack(std::vector<unsigned int> valuesVec, uint8_t verbose);
        void SetPulseDelayPack(std::vector<unsigned int> valuesVec, uint8_t verbose);
        void SetDUTMask(int32_t value, uint8_t verbose);
        void SetDUTMaskMode(int32_t value, uint8_t verbose);
        void SetDUTMaskModeModifier(int32_t value, uint8_t verbose);
        void SetDUTIgnoreBusy(int32_t value, uint8_t verbose);
        void SetDUTIgnoreShutterVeto(int32_t value, uint8_t verbose);
        void SetShutterControl(int32_t value, uint8_t verbose);
        void SetShutterInternalInterval(int32_t value, uint8_t verbose);
        void SetShutterSource(int32_t value, uint8_t verbose);
        void SetShutterOffTime(int32_t value, uint8_t verbose);
        void SetShutterOnTime(int32_t value, uint8_t verbose);
        void SetShutterVetoOffTime(int32_t value, uint8_t verbose);

        // uint32_t GetDUTMask() { return ReadRRegister("DUTInterfaces.DUTMaskR"); };

        void SetEventFifoCSR(int value) { SetWRegister("eventBuffer.EventFifoCSR", value); };
        void SetLogicClocksCSR(int value) { SetWRegister("logic_clocks.LogicClocksCSR", value); };

        void SetEnableRecordData(int value) { SetWRegister("Event_Formatter.Enable_Record_Data", value); };

        uint32_t GetLogicClocksCSR() { return ReadRRegister("logic_clocks.LogicClocksCSR"); };
        // uint32_t GetInternalTriggerInterval() { return ReadRRegister("triggerLogic.InternalTriggerIntervalR"); };
        uint32_t GetInternalTriggerFrequency(uint8_t verbose);
        uint32_t GetPulseStretch() { return ReadRRegister("triggerLogic.PulseStretchR"); };
        uint32_t GetPulseDelay() { return ReadRRegister("triggerLogic.PulseDelayR"); };
        // uint32_t GetTriggerMask() { return ReadRRegister("triggerLogic.TriggerMaskR"); };
        uint64_t GetTriggerMask();
        uint32_t GetDUTMask(uint8_t verbose);
        uint32_t GetDUTMaskMode(uint8_t verbose);
        uint32_t GetDUTMaskModeModifier(uint8_t verbose);
        uint32_t GetDUTIgnoreBusy(uint8_t verbose);
        uint32_t GetDUTIgnoreShutterVeto(uint8_t verbose);
        uint32_t GetShutterControl(uint8_t verbose);
        uint32_t GetShutterInternalInterval(uint8_t verbose);
        uint32_t GetShutterSource(uint8_t verbose);
        uint32_t GetShutterOnTime(uint8_t verbose);
        uint32_t GetShutterOffTime(uint8_t verbose);
        uint32_t GetShutterVetoOffTime(uint8_t verbose);
        uint32_t GetTriggerVeto();
        uint32_t GetPreVetoTriggers() { return ReadRRegister("triggerLogic.PreVetoTriggersR"); };
        uint32_t GetPostVetoTriggers() { return ReadRRegister("triggerLogic.PostVetoTriggersR"); };
        uint64_t GetCurrentTimestamp() {
            uint64_t time = ReadRRegister("Event_Formatter.CurrentTimestampHR");
            time = time << 32;
            time = time + ReadRRegister("Event_Formatter.CurrentTimestampLR");
            return time;
        }

        uint32_t GetFW();
        uint32_t GetEventFifoCSR(uint8_t verbose = 0);
        uint32_t GetEventFifoFillLevel();
        uint32_t GetI2CStatus() { return ReadRRegister("i2c_master.i2c_cmdstatus"); };
        uint32_t GetI2CRX() { return ReadRRegister("i2c_master.i2c_rxtx"); };
        uint32_t GetFirmwareVersion() { return ReadRRegister("version"); };
        void GetScaler(uint32_t& s0, uint32_t& s1, uint32_t& s2, uint32_t& s3, uint32_t& s4, uint32_t& s5) {
            s0 = ReadRRegister("triggerInputs.ThrCount0R");
            s1 = ReadRRegister("triggerInputs.ThrCount1R");
            s2 = ReadRRegister("triggerInputs.ThrCount2R");
            s3 = ReadRRegister("triggerInputs.ThrCount3R");
            s4 = ReadRRegister("triggerInputs.ThrCount4R");
            s5 = ReadRRegister("triggerInputs.ThrCount5R");
        }

        std::string parseURI();

        void SetI2CClockPrescale(int value) {
            SetWRegister("i2c_master.i2c_pre_lo", value & 0xff);
            SetWRegister("i2c_master.i2c_pre_hi", (value >> 8) & 0xff);
        };

        uint32_t I2C_enable(char EnclustraExpAddr);
        unsigned int* SetBoardID();

        void SetI2CControl(int value) { SetWRegister("i2c_master.i2c_ctrl", value & 0xff); };
        void SetI2CCommand(int value) { SetWRegister("i2c_master.i2c_cmdstatus", value & 0xff); };
        void SetI2CTX(int value) { SetWRegister("i2c_master.i2c_rxtx", value & 0xff); };

        void ResetBoard() { SetWRegister("logic_clocks.LogicRst", 1); };
        void ResetTimestamp() { SetWRegister("Event_Formatter.ResetTimestampW", 1); };

        bool I2CCommandIsDone() { return ((GetI2CStatus()) >> 1) & 0x1; };
        uint32_t GetBoardID();
        void ResetFIFO() { SetEventFifoCSR(0x0); };

        void ResetCounters() {
            SetSerdesRst(0x2);
            SetSerdesRst(0x0);
        };

        void ResetSerdes() {
            SetSerdesRst(0x3);
            SetSerdesRst(0x0);
            SetSerdesRst(0x4);
            SetSerdesRst(0x0);
        };

        fmctludata* PopFrontEvent();
        bool IsBufferEmpty() { return m_data.empty(); };
        void ReceiveEvents(uint8_t verbose);
        void ResetEventsBuffer();
        void DefineConst(int nDUTs, int nTrigInputs);
        void DumpEventsBuffer();

        void enableClkLEMO(bool enable, uint8_t verbose);
        // void InitializeI2C(char DACaddr, char IDaddr);
        float GetDACref() { return m_vref; };
        int InitializeClkChip(const std::string& filename, uint8_t verbose);
        void InitializeDAC(bool intRef, float Vref, uint8_t verbose);
        void InitializeIOexp(uint8_t verbose);
        void InitializeI2C(uint8_t verbose);
        void pwrled_Initialize(uint8_t verbose, unsigned int type);
        void pwrled_setVoltages(float v1, float v2, float v3, float v4, uint8_t verbose);
        void SetRunActive(uint8_t state);

        void SetDACValue(unsigned char channel, uint32_t value, uint8_t verbose);
        void SetShutterParameters(bool status,
                                  int8_t source,
                                  int32_t onTime,
                                  int32_t offTime,
                                  int32_t vetoOffTime,
                                  int32_t intInterval,
                                  uint8_t verbose);
        void SetThresholdValue(unsigned char channel, float thresholdVoltage, uint8_t verbose);
        void SetDutClkSrc(unsigned int hdmiN, unsigned int source, uint8_t verbose);
        void SetDACref(float vre, uint8_t verbosef);

        void SetWRegister(const std::string& name, int value);
        uint32_t ReadRRegister(const std::string& name);

        void SetUhalLogLevel(uchar_t l);
        void SetI2C_core_addr(char addressa) { m_I2C_address.core = addressa; };
        void SetI2C_clockChip_addr(char addressa) { m_I2C_address.clockChip = addressa; };
        void SetI2C_DAC1_addr(char addressa) { m_I2C_address.DAC1 = addressa; };
        void SetI2C_DAC2_addr(char addressa) { m_I2C_address.DAC2 = addressa; };
        void SetI2C_EEPROM_addr(char addressa) { m_I2C_address.EEPROM = addressa; };
        void SetI2C_expander1_addr(char addressa) { m_I2C_address.expander1 = addressa; };
        void SetI2C_expander2_addr(char addressa) { m_I2C_address.expander2 = addressa; };
        void SetI2C_pwrmdl_addr(char addressa, char addressb, char addressc, char addressd) {
            m_I2C_address.pwraddr = addressa;
            m_I2C_address.ledxp1addr = addressb;
            m_I2C_address.ledxp2addr = addressc;
            m_I2C_address.pwrId = addressd;
        };
        void SetI2C_disp_addr(char addressa) { m_I2C_address.lcdDisp = addressa; };

    private:
        struct I2C_addresses {
            char core;
            char clockChip;
            char DAC1;
            char DAC2;
            char EEPROM;
            char expander1;
            char expander2;
            char pwrId = 0x00; // i2c address of EEPROM on power module. This is only available on new modules.
            char pwraddr;      // i2c address of DAC of power module
            char ledxp1addr;   // i2c address of expander (LED controller)
            char ledxp2addr;   // i2c address of expander (LED controller)
            char lcdDisp;      // i2c address of LCD display
        } m_I2C_address;

        HwInterface* m_hw; // Instance of IPBus
        i2cCore* m_i2c;    // Instance of I2C
        std::string m_IPaddress;

        char m_DACaddr;
        char m_IDaddr;
        // uint32_t m_BoardID=0;
        unsigned int m_BoardID[6] = {0x0, 0x0, 0x0, 0x0, 0x0, 0x0};
        bool m_hasDisplay = false;  // Set to true if the LCD display is detected
        char m_powerModuleType = 0; // 0= old bugged power board, anything else= new power board with eeprom

        // Instantiate on-board hardware (I2C slaves)
        AD5665R m_zeDAC1, m_zeDAC2;
        PCA9539PW m_IOexpander1, m_IOexpander2;
        Si5345 m_zeClock;
        PWRLED* m_pwrled;
        LCD09052* m_lcddisp;

        // Define constants such as number of DUTs and trigger inputs
        int m_nDUTs;
        int m_nTrgIn;
        // Define reference voltage for DACs
        float m_vref;
        // Used for log purposes
        std::string m_myStates[2] = {"disabled", "enabled"};

        std::deque<fmctludata*> m_data;

        log::Logger logger_;
    };

    class fmctludata {
    public:
        fmctludata(uint64_t wl, uint64_t wh, uint64_t we)
            : // wl -> wh
              eventtype((wl >> 60) & 0xf), input0((wl >> 48) & 0x1), input1((wl >> 49) & 0x1), input2((wl >> 50) & 0x1),
              input3((wl >> 51) & 0x1), input4((wl >> 52) & 0x1), input5((wl >> 53) & 0x1), timestamp(wl & 0xffffffffffff),
              sc0((wh >> 56) & 0xff), sc1((wh >> 48) & 0xff), sc2((wh >> 40) & 0xff), sc3((wh >> 32) & 0xff),
              sc4((we >> 56) & 0xff), sc5((we >> 48) & 0xff), eventnumber(wh & 0xffffffff) {}

        fmctludata(uint32_t w0, uint32_t w1, uint32_t w2, uint32_t w3, uint32_t w4)
            : // w0 w1 w2 w3  wl= w0 w1; wh= w2 w3
              eventtype((w0 >> 28) & 0xf), input0((w0 >> 16) & 0x1), input1((w0 >> 17) & 0x1), input2((w0 >> 18) & 0x1),
              input3((w0 >> 19) & 0x1), input4((w0 >> 20) & 0x1), input5((w0 >> 21) & 0x1),
              timestamp(((uint64_t(w0 & 0x0000ffff)) << 32) + w1),
              // timestamp(w1),
              sc0((w2 >> 24) & 0xff), sc1((w2 >> 16) & 0xff), sc2((w2 >> 8) & 0xff), sc3(w2 & 0xff), sc4((w4 >> 24) & 0xff),
              sc5((w4 >> 16) & 0xff), eventnumber(w3), timestamp1(w0 & 0xffff) {}

        uchar_t eventtype;
        uchar_t input0;
        uchar_t input1;
        uchar_t input2;
        uchar_t input3;
        uchar_t input4;
        uchar_t input5;
        uint64_t timestamp;
        uchar_t sc0;
        uchar_t sc1;
        uchar_t sc2;
        uchar_t sc3;
        uchar_t sc4;
        uchar_t sc5;
        uint32_t eventnumber;
        uint64_t timestamp1;
    };

    std::ostream& operator<<(std::ostream& s, fmctludata& d);

} // namespace tlu

#endif
