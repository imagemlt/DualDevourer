#ifndef RTL8812ADEVICE_H
#define RTL8812ADEVICE_H

#include <functional>
#include <iostream>
#include <iomanip>

#include "logger.h"
#include "HalModule.h"
#include "ParsedRadioPacket.h"
#include "SelectedChannel.h"
#include "EepromManager.h"
#include "RadioManagementModule.h"
#include "FrameParser.h"




using Action_ParsedRadioPacket = std::function<void(const ParsedRadioPacket&)>;

class Rtl8812aDevice {
  std::shared_ptr<EepromManager> _eepromManager;
  std::shared_ptr<RadioManagementModule> _radioManagement;
  SelectedChannel _channel;
  RtlUsbAdapter _device;
  HalModule _halModule;
  Logger_t _logger;
  uint8_t debug;
  Action_ParsedRadioPacket _packetProcessor = nullptr;

public:
  Rtl8812aDevice(RtlUsbAdapter device, Logger_t logger);
  void Init(Action_ParsedRadioPacket packetProcessor, SelectedChannel channel);
  void SetMonitorChannel(SelectedChannel channel);
  void InitWrite(SelectedChannel channel);
  bool send_packet(const uint8_t* packet, size_t length);
  SelectedChannel GetSelectedChannel();

private:
  void StartWithMonitorMode(SelectedChannel selectedChannel);
  bool NetDevOpen(SelectedChannel selectedChannel);
  
};

#endif /* RTL8812ADEVICE_H */
