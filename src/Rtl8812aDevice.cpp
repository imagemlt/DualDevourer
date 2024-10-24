#include "Rtl8812aDevice.h"
#include "EepromManager.h"
#include "RadioManagementModule.h"

Rtl8812aDevice::Rtl8812aDevice(RtlUsbAdapter device, Logger_t logger)
    : _device{device},
      _eepromManager{std::make_shared<EepromManager>(device, logger)},
      _radioManagement{std::make_shared<RadioManagementModule>(
          device, _eepromManager, logger)},
      _halModule{device, _eepromManager, _radioManagement, logger},
      _logger{logger} {}

void Rtl8812aDevice::InitWrite(SelectedChannel channel){
	StartWithMonitorMode(channel);
	SetMonitorChannel(channel);
	_logger->info("In Monitor Mode");
	

}

bool Rtl8812aDevice::send_packet(const uint8_t* packet, size_t length) {
    struct tx_desc *ptxdesc;
  uint8_t* usb_frame;
  struct ieee80211_radiotap_header *rtap_hdr;
  int real_packet_length,usb_frame_length,radiotap_length;

  radiotap_length = int(packet[2]);
  real_packet_length = length - radiotap_length;
  uint8_t stbc;
  bool ldpc;
  bool short_gi;
  uint8_t bandwidth;
  uint8_t mcs_index;
  uint8_t rate_id;
  bool vht_mode;
  uint8_t vht_nss;

  usb_frame_length = real_packet_length + TXDESC_SIZE;

  _logger->info("radiotap length is {},80211 length is {},usb_frame length should be {}",radiotap_length,real_packet_length,usb_frame_length);

  if(packet[2] == 0x0d){
    vht_mode = false;
    
    uint8_t flags = packet[MCS_FLAGS_OFF];
    switch (flags & IEEE80211_RADIOTAP_MCS_BW_MASK) {
        case IEEE80211_RADIOTAP_MCS_BW_20:
            bandwidth = 20;
            break;
        case IEEE80211_RADIOTAP_MCS_BW_40:
            bandwidth = 40;
            break;
        case IEEE80211_RADIOTAP_MCS_BW_20L:
        case IEEE80211_RADIOTAP_MCS_BW_20U:
            bandwidth = 20; // Assuming these are variations of 20 MHz
            break;
        default:
            throw std::runtime_error("Unsupported bandwidth in flags");
    }

    // Parse Short GI
    if (flags & IEEE80211_RADIOTAP_MCS_SGI) {
        short_gi = true;
    }

    // Parse STBC
    stbc = (flags & IEEE80211_RADIOTAP_MCS_STBC_MASK) >> IEEE80211_RADIOTAP_MCS_STBC_SHIFT;

    // Parse LDPC
    if (flags & IEEE80211_RADIOTAP_MCS_FEC_LDPC) {
        ldpc = true;
    }
    
    mcs_index = packet[MCS_IDX_OFF];

  }else{
    vht_mode = true;
    uint8_t flags = packet[VHT_FLAGS_OFF];
    if (flags & IEEE80211_RADIOTAP_VHT_FLAG_SGI) {
        short_gi = true;
    }
    if (flags & IEEE80211_RADIOTAP_VHT_FLAG_STBC) {
        stbc = true;
    }

    // 解析带宽
    switch (packet[VHT_BW_OFF]) {
        case IEEE80211_RADIOTAP_VHT_BW_20M:
            bandwidth = 20;
            break;
        case IEEE80211_RADIOTAP_VHT_BW_40M:
            bandwidth = 40;
            break;
        case IEEE80211_RADIOTAP_VHT_BW_80M:
            bandwidth = 80;
            break;
        case IEEE80211_RADIOTAP_VHT_BW_160M:
            bandwidth = 160;
            break;
        default:
            throw std::runtime_error("Unsupported VHT bandwidth in header");
    }

    // 解析LDPC
    if (packet[VHT_CODING_OFF] & IEEE80211_RADIOTAP_VHT_CODING_LDPC_USER0) {
        ldpc = true;
    }

    // 解析MCS索引和NSS
    mcs_index = (packet[VHT_MCSNSS0_OFF] & IEEE80211_RADIOTAP_VHT_MCS_MASK) >> IEEE80211_RADIOTAP_VHT_MCS_SHIFT;
    vht_nss = (packet[VHT_MCSNSS0_OFF] & IEEE80211_RADIOTAP_VHT_NSS_MASK) >> IEEE80211_RADIOTAP_VHT_NSS_SHIFT;
  }
  _logger->info("uint8_t stbc = {};bool ldpc = {};bool short_gi={};uint8_t bandwidth={};uint8_t mcs_index={};bool vht_mode={};uint8_t vht_nss={};",stbc,ldpc,short_gi,bandwidth,mcs_index,vht_mode,vht_nss);
  
  
  usb_frame = new uint8_t[usb_frame_length]();

  rtap_hdr = (struct ieee80211_radiotap_header*)packet;


  ptxdesc = (struct tx_desc *)usb_frame;

  ptxdesc->txdw0 |= cpu_to_le32((unsigned int)(real_packet_length) & 0x0000ffff);
	ptxdesc->txdw0 |= cpu_to_le32(((TXDESC_SIZE + OFFSET_SZ) << OFFSET_SHT) & 0x00ff0000); // default = 32 bytes for TX Desc 
	ptxdesc->txdw0 |= cpu_to_le32(OWN | FSG | LSG);

	ptxdesc->txdw0 |= cpu_to_le32(BIT(24));

	// offset 4	 
	ptxdesc->txdw1 |= cpu_to_le32(0x00);// MAC_ID 

	ptxdesc->txdw1 |= cpu_to_le32((0x12 << QSEL_SHT) & 0x00001f00);

	ptxdesc->txdw1 |= cpu_to_le32((0x01 << 16) & 0x000f0000); // b mode 
  
  // todo: MCS怎样转换为rate_id,使用的转换方式：PHY_GetRateIndexOfTxPowerByRate，HT模式应该是12

  rate_id=0x07;

  SET_TX_DESC_BMC_8812(usb_frame, 1);

	SET_TX_DESC_RATE_ID_8812(usb_frame, static_cast<uint8_t>(rate_id)); // 原来设置的是7，得考虑下怎么转换
  SET_TX_DESC_HWSEQ_EN_8812(usb_frame, static_cast<uint8_t>(0)); /* Hw do not set sequence number */
	SET_TX_DESC_SEQ_8812(usb_frame, 0); /* Copy inject sequence number to TxDesc */

	SET_TX_DESC_RETRY_LIMIT_ENABLE_8812(usb_frame, static_cast<uint8_t>(1));

	SET_TX_DESC_DATA_RETRY_LIMIT_8812(usb_frame, static_cast<uint8_t>(0));
  if(short_gi){
    SET_TX_DESC_DATA_SHORT_8812(usb_frame, static_cast<uint8_t>(1));
  }
else{
  SET_TX_DESC_DATA_SHORT_8812(usb_frame, static_cast<uint8_t>(0));
}


	SET_TX_DESC_DISABLE_FB_8812(usb_frame, static_cast<uint8_t>(1)); // svpcom: ?
	SET_TX_DESC_USE_RATE_8812(usb_frame, static_cast<uint8_t>(1));
  
  //MRateToHwRate(rate_id);
	SET_TX_DESC_TX_RATE_8812(usb_frame, static_cast<uint8_t>(mcs_index)); // 原来设置的是6,也需要考虑下怎么转换
  
  if (ldpc)
		SET_TX_DESC_DATA_LDPC_8812(usb_frame, 1);
	if (stbc)	
		SET_TX_DESC_DATA_STBC_8812(usb_frame, 1);

  uint8_t BWSettingOfDesc;
	if(_channel.ChannelWidth== CHANNEL_WIDTH_80)
	{
		if(bandwidth == 80)
			BWSettingOfDesc= 2;
		else if(bandwidth == 40)
			BWSettingOfDesc = 1;
		else
			BWSettingOfDesc = 0;
	}
	else if(_channel.ChannelWidth== CHANNEL_WIDTH_40)
	{
		if((bandwidth == 40) || (bandwidth == 80))
			BWSettingOfDesc = 1;
		else
			BWSettingOfDesc = 0;
	}
	else
		BWSettingOfDesc = 0;

	
  SET_TX_DESC_DATA_BW_8812(usb_frame, BWSettingOfDesc); 

  rtl8812a_cal_txdesc_chksum(usb_frame);
  _logger->info("tx desc formed");
  for (size_t i = 0; i < usb_frame_length; ++i) {
        // Print each byte as a two-digit hexadecimal number
        std::cout << "0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(usb_frame[i]);
        
        // Print a space between bytes, but not after the last byte
        if (i < length - 1) {
            std::cout << " ";
        }
    }
    std::cout << std::dec << std::endl;  // Reset to decimal formatting
	// ----- end of fill tx desc ----- 
  uint8_t * addr=usb_frame+TXDESC_SIZE;
  memcpy(addr,packet + radiotap_length,real_packet_length);
  _logger->info("packet formed");
  for (size_t i = 0; i < usb_frame_length; ++i) {
        // Print each byte as a two-digit hexadecimal number
        std::cout << "0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(usb_frame[i]);
        
        // Print a space between bytes, but not after the last byte
        if (i < length - 1) {
            std::cout << " ";
        }
    }
    std::cout << std::dec << std::endl;  // Reset to decimal formatting

  return _device.send_packet(usb_frame,usb_frame_length);
}

void Rtl8812aDevice::Init(Action_ParsedRadioPacket packetProcessor,
                          SelectedChannel channel) {
  _packetProcessor = packetProcessor;

  StartWithMonitorMode(channel);
  SetMonitorChannel(channel);

  _logger->info("Listening air...");
  for (;;)
    _device.infinite_read();

#if 0
  _device.UsbDevice.SetBulkDataHandler(BulkDataHandler);
  _readTask = Task.Run(() = > _device.UsbDevice.InfinityRead());
#endif
}

void Rtl8812aDevice::SetMonitorChannel(SelectedChannel channel) {
  _channel = channel;
  _radioManagement->set_channel_bwmode(channel.Channel, channel.ChannelOffset,
                                      channel.ChannelWidth);
}

void Rtl8812aDevice::StartWithMonitorMode(SelectedChannel selectedChannel) {
  if (NetDevOpen(selectedChannel) == false) {
    throw std::ios_base::failure("StartWithMonitorMode failed NetDevOpen");
  }

  _radioManagement->SetMonitorMode();
}

bool Rtl8812aDevice::NetDevOpen(SelectedChannel selectedChannel) {
  auto status = _halModule.rtw_hal_init(selectedChannel);
  if (status == false) {
    return false;
  }

  return true;
}

SelectedChannel Rtl8812aDevice::GetSelectedChannel(){
  return _channel;
}
