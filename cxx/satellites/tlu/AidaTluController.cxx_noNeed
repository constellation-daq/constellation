#include "AidaTluController.hh"
#include "eudaq/OptionParser.hh"
#include "eudaq/Utils.hh"
#include "eudaq/Exception.hh"
#ifndef WIN32
#include <unistd.h>
#endif

#include <string>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <csignal>

#include <memory>

using namespace tlu;
using eudaq::to_string;
using eudaq::hexdec;

static sig_atomic_t g_done = 0;
#ifdef WIN32
static const std::string TIME_FORMAT="%M:%S.%3";
#else
static const std::string TIME_FORMAT="%s.%3";
#endif
void ctrlchandler(int) {
  g_done += 1;
}

int main(int /*argc*/, char ** argv) {
  float vthresh_values[4];

  eudaq::OptionParser op("FMC TLU Control Utility", "1.1", "A comand-line tool for controlling the AIDA FMC TLU");

  eudaq::Option<std::string> ConnectionFile(op, "cf", "connectionFile", "connection.xml", "filename",
					    "IPBus/uHAL connection file describing network connection to TLU");
  eudaq::Option<std::string> DeviceName(op, "dn", "deviceName", "fmctlu", "filename",
					"IPBus/uHAL connection file describing network connection to TLU");


  eudaq::Option<uchar_t>  dmask(op, "d", "dutmask", 1, "mask",
				"The mask for enabling the DUT connections");
  eudaq::Option<uchar_t>  dutnobusy(op, "b", "dutnobusy", 7, "mask",
				    "Mask for disable the veto of the DUT busy singals");
  eudaq::Option<uchar_t>  dutmode(op, "m", "dutmode", 60, "mask",
				  "Mask for DUT Mode (2 bits each channel)");
  eudaq::Option<uchar_t>  dutmm(op, "m", "dutmm", 60, "mask",
				"Mask for DUT Mode Modifier (2 bits each channel)");
  eudaq::Option<uint32_t> clk(op, "i", "clk", 0, "1/160MHz",
				 "The interval of internally generated triggers (0 = off)");
  eudaq::Option<uchar_t>  vmask(op, "v", "vetomask", 0, "mask",
				"The mask for vetoing external triggers");
  eudaq::Option<uchar_t>  amask(op, "a", "andmask", 15, "mask",
				"The mask for coincidence of external triggers");


  eudaq::Option<uchar_t>        dacAddr(op, "da" ,"dacAddr" , 0x1F , "I2C address" , "I2C address of threshold DAC");
  eudaq::Option<uchar_t>        identAddr(op, "id" ,"idAddr" , 0x50 , "I2C address" , "I2C address of ID EEPROM");

  eudaq::Option<float>         vthresh_0(op, "thr0", "vthresh0", 0, "Volts", "Threshold 0");
  eudaq::Option<float>         vthresh_1(op, "thr1", "vthresh1", 0, "Volts", "Threshold 1");
  eudaq::Option<float>         vthresh_2(op, "thr2", "vthresh2", 0, "Volts", "Threshold 2");
  eudaq::Option<float>         vthresh_3(op, "thr3", "vthresh3", 0, "Volts", "Threshold 3");


  eudaq::Option<uint32_t>         wait(op, "w", "wait", 1000, "ms",
                                  "Time to wait between updates in milliseconds");

  eudaq::OptionFlag          nots(op, "n", "notimestamp", "Do not read out timestamp buffer");
  eudaq::OptionFlag          quit(op, "q", "quit", "Quit after configuring TLU");
  eudaq::OptionFlag          pause(op, "u", "wait-for-user", "Wait for user input before starting triggers");
  eudaq::Option<std::string> sname(op, "s", "save-file", "", "filename",
                                   "The filename to save trigger numbers and timestamps");
  eudaq::Option<uchar_t>     ipbusDebug(op, "z", "ipbusDebug", 2, "level" "Debugg level of IPBus");

  try {
    op.Parse(argv);
    //TODO:: print config

    std::shared_ptr<std::ofstream> sfile;
    if (sname.Value() != "") {
      sfile = std::make_shared<std::ofstream>(sname.Value().c_str());
      if (!sfile->is_open()) EUDAQ_THROW("Unable to open file: " + sname.Value());
    }
    signal(SIGINT, ctrlchandler);

    AidaTluController TLU(ConnectionFile.Value() , DeviceName.Value() );

    TLU.ResetSerdes();
    TLU.ResetCounters();
    TLU.SetTriggerVeto(1);
    TLU.ResetFIFO();
    TLU.ResetEventsBuffer();
    //TLU.InitializeI2C(dacAddr.Value(),identAddr.Value());
    TLU.InitializeI2C();
    TLU.SetThresholdValue(0, vthresh_0.Value());
    TLU.SetThresholdValue(1, vthresh_1.Value());
    TLU.SetThresholdValue(2, vthresh_2.Value());
    TLU.SetThresholdValue(3, vthresh_3.Value());

    TLU.SetDUTMask(dmask.Value());
    TLU.SetDUTMaskMode(dutmode.Value());
    TLU.SetDUTMaskModeModifier(dutmm.Value());
    TLU.SetDUTIgnoreBusy(dutnobusy.Value());
    TLU.SetDUTIgnoreShutterVeto(1);
    TLU.SetEnableRecordData(1);

    TLU.SetInternalTriggerInterval(clk.Value());
    TLU.SetTriggerMask(amask.Value());
    TLU.SetTriggerVeto(vmask.Value());
    TLU.SetUhalLogLevel(ipbusDebug.Value());

    uint32_t bid = TLU.GetBoardID();
    uint32_t fid = TLU.GetFirmwareVersion();
    std::cout << "Board ID number  = " << eudaq::hexdec(bid, 12) << std::endl
              << "Firmware version = " << fid << std::endl<<std::endl;

    if (quit.IsSet()) return 0;
    // eudaq::Timer totaltime, lasttime;

    if (pause.IsSet()) {
      std::cerr << "Press enter to start triggers." << std::endl;
      std::getchar();
    }
    TLU.SetTriggerVeto(0);
    std::cout << "FMC TLU Started!" << std::endl;

    uint32_t total = 0;
    while (!g_done) {
      TLU.ReceiveEvents();
      int nev = 0;
      while (!TLU.IsBufferEmpty()){
	nev++;
	fmctludata * data = TLU.PopFrontEvent();
	uint32_t evn = data->eventnumber;
	uint64_t t = data->timestamp;

	if (sfile.get()) {
	  *sfile << *data;
	}
      }
      total += nev;
      // double hertz = nev / lasttime.Seconds();
      // double avghertz = total / totaltime.Seconds();
      // lasttime.Restart();
      // std::cout << "Time: " << totaltime.Formatted(TIME_FORMAT) << " s, "
      // 		<< "Freq: " << hertz << " Hz, "
      // 		<< "Average: " << avghertz << " Hz" << std::endl;
      if (wait.Value() > 0) {
	eudaq::mSleep(wait.Value());
      }
    }
    std::cout << "Quitting..." << std::endl;
    TLU.SetTriggerVeto(1);
  } catch (...) {
    return op.HandleMainException();
  }
  return 0;
}
