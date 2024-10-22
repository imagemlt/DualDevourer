#include <cassert>
#include <iostream>
#include <memory>
#include <string>

#include <libusb-1.0/libusb.h>

#include <spdlog/sinks/android_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include "RtlUsbAdapter.h"
#include "WiFiDriver.h"
#include "FrameParser.h"

#include <iomanip>

//#define USB_VENDOR_ID 0x0bda
//#define USB_PRODUCT_ID 0x8812

void printHexArray(const uint8_t* array, size_t length) {
    for (size_t i = 0; i < length; ++i) {
        // Print each byte as a two-digit hexadecimal number
        std::cout << "0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(array[i]);
        
        // Print a space between bytes, but not after the last byte
        if (i < length - 1) {
            std::cout << " ";
        }
    }
    std::cout << std::dec << std::endl;  // Reset to decimal formatting
}

static void packetProcessor(const ParsedRadioPacket &packet) {
}


int main_test(int argc,char **argv) {

  
  libusb_context *context;
  libusb_device_handle *handle;
  libusb_device *device;
  struct libusb_device_descriptor desc;
  uint8_t usb_frame[10000];
  unsigned char buffer[256];
  struct tx_desc* ptxdesc;
  int fd;


  
  auto logger = spdlog::stderr_color_mt("stdout");
  
  fd = atoi(argv[1]);
  logger->info("got fd {}",fd);

  //libusb_set_option(context, LIBUSB_OPTION_LOG_LEVEL, LIBUSB_LOG_LEVEL_DEBUG);
  libusb_set_option(NULL, LIBUSB_OPTION_NO_DEVICE_DISCOVERY);
  libusb_set_option(NULL, LIBUSB_OPTION_WEAK_AUTHORITY);
  
  int rc=libusb_init(&context);
  
  rc=libusb_wrap_sys_device(context, (intptr_t) fd, &handle);

  device = libusb_get_device(handle);

  rc=libusb_get_device_descriptor(device, &desc);
  
  logger->info("Vendor/Product ID:{:04x}:{:04x}",desc.idVendor,desc.idProduct);
  if (libusb_kernel_driver_active(handle, 0)) {
        rc = libusb_detach_kernel_driver(handle, 0); // detach driver
        logger->error("libusb_detach_kernel_driver: {}", rc);
    }
    rc = libusb_claim_interface(handle, 0);

  WiFiDriver wifi_driver{logger};
  auto rtlDevice = wifi_driver.CreateRtlDevice(handle);
  pid_t fpid;
  fpid=fork();
  if( fpid == 0){

  
  rtlDevice->Init(packetProcessor,SelectedChannel{
                                       .Channel = static_cast<uint8_t>(161),
                                       .ChannelOffset = 0,
                                       .ChannelWidth = CHANNEL_WIDTH_80,

				       });

  return 1;
  }
  //循环发送beacon包
  //
  
  sleep(5);
 /*
  uint8_t beacon_frame[] = {
        0x80, 0x00, // Frame Control (Management, Beacon)
        0x00, 0x00, // Duration/ID
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // Destination Address (Broadcast)
        0x00, 0x11, 0x22, 0x33, 0x44, 0x55, // Source Address (Replace with your MAC)
        0x00, 0x11, 0x22, 0x33, 0x44, 0x55, // BSSID (Replace with your MAC)
        0x00, 0x00, // Sequence Control
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Timestamp
        0x64, 0x00, // Beacon Interval (100 ms)
        0x01, 0x04, // Capability Information
        0x00, 0x07, 'O', 'P', 'E', 'N', 'I','P','C', // SSID Parameter Set (SSID1)
        0x01, 0x08, 0x82, 0x84, 0x8b, 0x96, 0x24, 0x30, 0x48, 0x6c, // Supported Rates
        0x03, 0x01, 0x24, // DS Parameter Set (Channel 1)
        0x05, 0x04, 0x01, 0x02, 0x00, 0x00, // Traffic Indication Map
        0x07, 0x06, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, // IBSS Parameter Set
        0x2a, 0x01, 0x00, // ERP Information
        0x32, 0x04, 0x30, 0x48, 0x60, 0x6c, // Extended Supported Rates
        0x2d, 0x1a, 0xef, 0x01, 0x17, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // HT Capabilities
        0x3d, 0x16, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // HT Information
        0xbf, 0x0c, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // VHT Capabilities
        0xc0, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, // VHT Operation
        0xdd, 0x09, 0x00, 0x10, 0x18, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00 // Vendor Specific
    };
*/

  /*uint8_t beacon_frame[] = {
    // 802.11 Frame Header
    0x40, 0x00, // Frame Control: Type=Management, Subtype=Probe Request
    0x00, 0x00, // Duration: 0 (not used in Probe Request)
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // Destination Address: Broadcast
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, // Source Address: Replace with your MAC
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, // BSSID: Typically same as Source Address
    0x00, 0x00, // Sequence Control: 0 (can be set to any value)

    // Information Elements
    0x00, 0x07,'O','P','E','N','I','P','C', // SSID Parameter Set: SSID Length=0 (wildcard)
    
    0x01, 0x08, // Supported Rates: Length=8
    0x82, 0x84, 0x8b, 0x96, 0x24, 0x30, 0x48, 0x6c, // Rates: 1, 2, 5.5, 11, 18, 24, 36, 54 Mbps
};*/
/*
  uint8_t beacon_frame[] = {
    // Radiotap Header (假设长度为12字节)
   /* 0x00, 0x00, 0x0E, 0x00, //版本和长度
    0x00, 0x00, 0x00, 0x00, //标志
    0x00, 0x00, 0x00, 0x00, //信号强度等信息
    0x00, 0x00, 0x00, 0x00, //频段等信息

    // 802.11 MAC Header
    0x40, 0x00, // 帧控制: 管理帧, Probe Request
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 目的地址 (广播)
    0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, // 源地址 (假设的设备MAC)
    0x00, 0x00, 0x00, 0x00, // BSSID (通常为全零)
    0x00, 0x00, // 序列控制

    // SSID信息元素
    0x00, 0x00, // 信息元素ID和长度
    // SSID内容 (假设为空)
    
    // Supported Rates (假设支持的速率)
    0x01, 0x08, // 信息元素ID和长度
    0x82, 0x84, 0x8B, 0x96, 0x0C, 0x12, 0x18, 0x24, // 支持的速率

    // Extended Supported Rates
    0xFF, 0x01, // 信息元素ID和长度
    0x0C, // 额外速率

    // HT Capabilities
    0x2D, 0x1A, // 信息元素ID和长度
    0x00, 0x00, 0x00, 0x00, // HT Capabilities (假设)
    0x00, // A-MPDU参数
    // 其他可能的字段
};
*/
  /*uint8_t beacon_frame[] = {
    // 802.11 Frame Header
    0x40, 0x00, // Frame Control: Type=Management, Subtype=Probe Request
    0x00, 0x00, // Duration: 0 (not used in Probe Request)
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // Destination Address: Broadcast
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, // Source Address: Replace with your MAC
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, // BSSID: Typically same as Source Address
    0x00, 0x00, // Sequence Control: 0 (can be set to any value)

    // Information Elements
    0x00, 0x07,'O','P','E','N','I','P','C', // SSID Parameter Set: SSID Length=0 (wildcard)
    
    0x01, 0x08, // Supported Rates: Length=8
    0x82, 0x84, 0x8b, 0x96, 0x24, 0x30, 0x48, 0x6c, // Rates: 1, 2, 5.5, 11, 18, 24, 36, 54 Mbps
};*/
  uint8_t beacon_frame[]= {
    0x08, 0x01, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0x57, 0x42, 0x75, 0x05, 0xd6, 0x00,
    0x57, 0x42, 0x75, 0x05, 0xd6, 0x00, 0x00, 0x00,
    0x02, 0xa6, 0x6c, 0x6a, 0x2d, 0xcb, 0x3d, 0xe4,
    0x64, 0xa0, 0x8d, 0x9e, 0x47, 0xe4, 0x3b, 0x35,
    0xe9, 0xe2, 0x63, 0xe5, 0x13, 0xab, 0x87, 0xf7,
    0x69, 0x35, 0x6a, 0xf1, 0x7d, 0x83, 0xe7, 0xb2,
    0xb7, 0x07, 0x38, 0xcc, 0xd4, 0x97, 0x84, 0xb2,
    0xe9, 0xf6, 0x18, 0xc1, 0xc6, 0x56, 0x0b, 0x28,
    0x46, 0x26, 0xda, 0x62, 0x34, 0xfe, 0x7f, 0x20,
    0x1e, 0x16, 0xbf, 0x26, 0x2b, 0x3e, 0x25, 0x7d,
    0x02, 0xce, 0x9c, 0x69, 0x3c, 0xf0, 0xd0, 0xfb,
    0xf8, 0xaa, 0xec, 0x83, 0xbb, 0x52, 0xa0, 0x52,
    0xa3, 0xde, 0xe1, 0x75, 0x19, 0x51, 0x96, 0xe0,
    0x4d, 0x96, 0x4c, 0xe4
};
  memset(usb_frame,0,sizeof(usb_frame));
  ptxdesc = (struct tx_desc *)usb_frame;
  ptxdesc->txdw0 |= cpu_to_le32((unsigned int)(sizeof(beacon_frame)) & 0x0000ffff);
	ptxdesc->txdw0 |= cpu_to_le32(((TXDESC_SIZE + OFFSET_SZ) << OFFSET_SHT) & 0x00ff0000); // default = 32 bytes for TX Desc 
	ptxdesc->txdw0 |= cpu_to_le32(OWN | FSG | LSG);

		ptxdesc->txdw0 |= cpu_to_le32(BIT(24));

	// offset 4	 
	ptxdesc->txdw1 |= cpu_to_le32(0x00);// MAC_ID 

	ptxdesc->txdw1 |= cpu_to_le32((0x12 << QSEL_SHT) & 0x00001f00);

	ptxdesc->txdw1 |= cpu_to_le32((0x01 << 16) & 0x000f0000); // b mode 
        

	SET_TX_DESC_RATE_ID_8812(usb_frame, static_cast<uint8_t>(0x07));
  		SET_TX_DESC_HWSEQ_EN_8812(usb_frame, static_cast<uint8_t>(0)); /* Hw do not set sequence number */
		SET_TX_DESC_SEQ_8812(usb_frame, 0); /* Copy inject sequence number to TxDesc */

		SET_TX_DESC_RETRY_LIMIT_ENABLE_8812(usb_frame, static_cast<uint8_t>(1));

		//if (pattrib->retry_ctrl == _TRUE) {
			//SET_TX_DESC_DATA_RETRY_LIMIT_8812(ptxdesc, 6);
		//} else {
			SET_TX_DESC_DATA_RETRY_LIMIT_8812(usb_frame, static_cast<uint8_t>(0));
		//}
		/*if(pattrib->sgi == _TRUE) {
			SET_TX_DESC_DATA_SHORT_8812(usb_frame, static_cast<uint8_t>(1));
		} else {*/
			SET_TX_DESC_DATA_SHORT_8812(usb_frame, static_cast<uint8_t>(0));
		//}

		SET_TX_DESC_DISABLE_FB_8812(usb_frame, static_cast<uint8_t>(1)); // svpcom: ?
		SET_TX_DESC_USE_RATE_8812(usb_frame, static_cast<uint8_t>(1));
		SET_TX_DESC_TX_RATE_8812(usb_frame, static_cast<uint8_t>(0x06));

		//if (pattrib->ldpc)
		//	SET_TX_DESC_DATA_LDPC_8812(ptxdesc, 1);
		//SET_TX_DESC_DATA_STBC_8812(ptxdesc, pattrib->stbc & 3); //不知道，先忽略
		//SET_TX_DESC_GF_8812(ptxdesc, 1); // no MCS rates if sets, GreenField?
		//SET_TX_DESC_LSIG_TXOP_EN_8812(ptxdesc, 1);
		//SET_TX_DESC_HTC_8812(ptxdesc, 1);
		//SET_TX_DESC_NO_ACM_8812(ptxdesc, 1);
		SET_TX_DESC_DATA_BW_8812(usb_frame, 0); 

	// offset 8			 

	// offset 12		 
	/*ptxdesc->txdw3 |= cpu_to_le32((le16_to_cpu((u16)0) << 16) & 0xffff0000);

	// offset 16		 
	ptxdesc->txdw4 |= cpu_to_le32(BIT(8));// driver uses rate 

	// offset 20 


	// HW append seq 
	ptxdesc->txdw4 |= cpu_to_le32(BIT(7)); // Hw set sequence number 
	ptxdesc->txdw3 |= cpu_to_le32((8 << 28)); // set bit3 to 1. Suugested by TimChen. 2009.12.29. 

*/
	//rtl8188eu_cal_txdesc_chksum(ptxdesc);
	rtl8812a_cal_txdesc_chksum(usb_frame); //没有找到定义，猜测是校验和。
	// ----- end of fill tx desc ----- 
  
  printHexArray(beacon_frame,sizeof(beacon_frame));
  uint8_t * addr=usb_frame+TXDESC_SIZE;
  memcpy(addr,beacon_frame,sizeof(beacon_frame));
 
  printHexArray(usb_frame,sizeof(beacon_frame)+TXDESC_SIZE);
  
  int actual_length = 0;
  while(true){
  	rc = libusb_bulk_transfer(handle, 0x04, usb_frame, TXDESC_SIZE+sizeof(beacon_frame), &actual_length, USB_TIMEOUT);
  	if (rc < 0) {
		logger->error("Error sending beacon frame via libusb: {}", libusb_error_name(rc));
	}
	else{
	
		//logger->info("Beacon frame sent successfully, length: {}", actual_length);
	}
	//std::this_thread::sleep_for(std::chrono::milliseconds(1000));

  }
  rc = libusb_release_interface(handle, 0);
  assert(rc == 0);

  libusb_close(handle);

  libusb_exit(context);
  return 0;
}

int main(int argc,char **argv) {

  
  libusb_context *context;
  libusb_device_handle *handle;
  libusb_device *device;
  struct libusb_device_descriptor desc;
  uint8_t usb_frame[10000];
  unsigned char buffer[256];
  struct tx_desc* ptxdesc;
  int fd;


  
  auto logger = spdlog::stderr_color_mt("stdout");
  
  fd = atoi(argv[1]);
  logger->info("got fd {}",fd);

  //libusb_set_option(context, LIBUSB_OPTION_LOG_LEVEL, LIBUSB_LOG_LEVEL_DEBUG);
  libusb_set_option(NULL, LIBUSB_OPTION_NO_DEVICE_DISCOVERY);
  libusb_set_option(NULL, LIBUSB_OPTION_WEAK_AUTHORITY);
  
  int rc=libusb_init(&context);
  
  rc=libusb_wrap_sys_device(context, (intptr_t) fd, &handle);

  device = libusb_get_device(handle);

  rc=libusb_get_device_descriptor(device, &desc);
  
  logger->info("Vendor/Product ID:{:04x}:{:04x}",desc.idVendor,desc.idProduct);
  if (libusb_kernel_driver_active(handle, 0)) {
        rc = libusb_detach_kernel_driver(handle, 0); // detach driver
        logger->error("libusb_detach_kernel_driver: {}", rc);
    }
    rc = libusb_claim_interface(handle, 0);

  WiFiDriver wifi_driver{logger};
  auto rtlDevice = wifi_driver.CreateRtlDevice(handle);
  pid_t fpid;
  fpid=fork();
  if( fpid == 0){

  
  rtlDevice->Init(packetProcessor,SelectedChannel{
                                       .Channel = static_cast<uint8_t>(161),
                                       .ChannelOffset = 0,
                                       .ChannelWidth = CHANNEL_WIDTH_40,

				       });

  return 1;
  }
  //循环发送beacon包
  //
  
  sleep(5);
 
  uint8_t beacon_frame[]= {
	  0x00 ,0x00 ,0x0d, 0x00 ,0x00, 0x80, 0x08, 0x00, 0x08, 0x00, 0x37, 0x00, 0x01,
    0x08, 0x01, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0x57, 0x42, 0x75, 0x05, 0xd6, 0x00,
    0x57, 0x42, 0x75, 0x05, 0xd6, 0x00, 0x00, 0x00,
    0x02, 0xa6, 0x6c, 0x6a, 0x2d, 0xcb, 0x3d, 0xe4,
    0x64, 0xa0, 0x8d, 0x9e, 0x47, 0xe4, 0x3b, 0x35,
    0xe9, 0xe2, 0x63, 0xe5, 0x13, 0xab, 0x87, 0xf7,
    0x69, 0x35, 0x6a, 0xf1, 0x7d, 0x83, 0xe7, 0xb2,
    0xb7, 0x07, 0x38, 0xcc, 0xd4, 0x97, 0x84, 0xb2,
    0xe9, 0xf6, 0x18, 0xc1, 0xc6, 0x56, 0x0b, 0x28,
    0x46, 0x26, 0xda, 0x62, 0x34, 0xfe, 0x7f, 0x20,
    0x1e, 0x16, 0xbf, 0x26, 0x2b, 0x3e, 0x25, 0x7d,
    0x02, 0xce, 0x9c, 0x69, 0x3c, 0xf0, 0xd0, 0xfb,
    0xf8, 0xaa, 0xec, 0x83, 0xbb, 0x52, 0xa0, 0x52,
    0xa3, 0xde, 0xe1, 0x75, 0x19, 0x51, 0x96, 0xe0,
    0x4d, 0x96, 0x4c, 0xe4
  };
  

  uint8_t stbc;
  bool ldpc;
  bool short_gi;
  uint8_t bandwidth;
  uint8_t mcs_index;
  bool vht_mode;
  uint8_t vht_nss;

  if(beacon_frame[2] == 0x0d){
    vht_mode = false;
    
    uint8_t flags = beacon_frame[MCS_FLAGS_OFF];
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
    
    mcs_index = beacon_frame[MCS_IDX_OFF];

  }else{
    vht_mode = true;
    uint8_t flags = beacon_frame[VHT_FLAGS_OFF];
    if (flags & IEEE80211_RADIOTAP_VHT_FLAG_SGI) {
        short_gi = true;
    }
    if (flags & IEEE80211_RADIOTAP_VHT_FLAG_STBC) {
        stbc = true;
    }

    // 解析带宽
    switch (beacon_frame[VHT_BW_OFF]) {
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
    if (beacon_frame[VHT_CODING_OFF] & IEEE80211_RADIOTAP_VHT_CODING_LDPC_USER0) {
        ldpc = true;
    }

    // 解析MCS索引和NSS
    mcs_index = (beacon_frame[VHT_MCSNSS0_OFF] & IEEE80211_RADIOTAP_VHT_MCS_MASK) >> IEEE80211_RADIOTAP_VHT_MCS_SHIFT;
    vht_nss = (beacon_frame[VHT_MCSNSS0_OFF] & IEEE80211_RADIOTAP_VHT_NSS_MASK) >> IEEE80211_RADIOTAP_VHT_NSS_SHIFT;
  }
  logger->info("uint8_t stbc = {};bool ldpc = {};bool short_gi={};uint8_t bandwidth={};uint8_t mcs_index={};bool vht_mode={};uint8_t vht_nss={};",stbc,ldpc,short_gi,bandwidth,mcs_index,vht_mode,vht_nss);
  //std::cout << "Packet Report Type: " << attrib.pkt_rpt_type << std::endl;
  //logger->debug("parsed radiotap header to rx_pkt_attrib,{}",pattrib);
  
  int actual_length = 0;
  
  while(true){
    
  	rc = rtlDevice->send_packet(beacon_frame,sizeof(beacon_frame));
    
	  //std::this_thread::sleep_for(std::chrono::milliseconds(1000));

  }
  rc = libusb_release_interface(handle, 0);
  assert(rc == 0);

  libusb_close(handle);

  libusb_exit(context);
  return 0;
}
