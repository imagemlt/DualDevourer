#include "FrameParser.h"

#define CONFIG_USB_RX_AGGREGATION 1

#define RXDESC_SIZE 24

#include "basic_types.h"
#include "rtl8812a_recv.h"


FrameParser::FrameParser(Logger_t logger) : _logger{logger} {}

static rx_pkt_attrib rtl8812_query_rx_desc_status(uint8_t *pdesc) {
  auto pattrib = rx_pkt_attrib{};

  /* Offset 0 */
  pattrib.pkt_len = GET_RX_STATUS_DESC_PKT_LEN_8812(pdesc);
  pattrib.crc_err = GET_RX_STATUS_DESC_CRC32_8812(pdesc);
  pattrib.icv_err = GET_RX_STATUS_DESC_ICV_8812(pdesc);
  pattrib.drvinfo_sz =
      (uint8_t)(GET_RX_STATUS_DESC_DRVINFO_SIZE_8812(pdesc) * 8);
  pattrib.encrypt = GET_RX_STATUS_DESC_SECURITY_8812(pdesc);
  pattrib.qos = GET_RX_STATUS_DESC_QOS_8812(pdesc);
  /* Qos data, wireless
                                                          lan header length is
                                                          26 */
  pattrib.shift_sz = GET_RX_STATUS_DESC_SHIFT_8812(
      pdesc); /* ((le32_to_cpu(pdesc.rxdw0) >> 24) & 0x3); */
  pattrib.physt = GET_RX_STATUS_DESC_PHY_STATUS_8812(
      pdesc); /* ((le32_to_cpu(pdesc.rxdw0) >> 26) & 0x1); */
  pattrib.bdecrypted = !GET_RX_STATUS_DESC_SWDEC_8812(
      pdesc); /* (le32_to_cpu(pdesc.rxdw0) & BIT(27))? 0:1; */

  /* Offset 4 */
  pattrib.priority = GET_RX_STATUS_DESC_TID_8812(pdesc);
  pattrib.mdata = GET_RX_STATUS_DESC_MORE_DATA_8812(pdesc);
  pattrib.mfrag = GET_RX_STATUS_DESC_MORE_FRAG_8812(pdesc);
  /* more fragment bit */

  /* Offset 8 */
  pattrib.seq_num = GET_RX_STATUS_DESC_SEQ_8812(pdesc);
  pattrib.frag_num = GET_RX_STATUS_DESC_FRAG_8812(pdesc);
  /* fragmentation number */

  if (GET_RX_STATUS_DESC_RPT_SEL_8812(pdesc)) {
    pattrib.pkt_rpt_type = RX_PACKET_TYPE::C2H_PACKET;
  } else {
    pattrib.pkt_rpt_type = RX_PACKET_TYPE::NORMAL_RX;
  }

  /* Offset 12 */
  pattrib.data_rate = GET_RX_STATUS_DESC_RX_RATE_8812(pdesc);
  /* Offset 16 */
  pattrib.sgi = GET_RX_STATUS_DESC_SPLCP_8812(pdesc);
  pattrib.ldpc = GET_RX_STATUS_DESC_LDPC_8812(pdesc);
  pattrib.stbc = GET_RX_STATUS_DESC_STBC_8812(pdesc);
  pattrib.bw = GET_RX_STATUS_DESC_BW_8812(pdesc);

  /* Offset 20 */
  /* pattrib.tsfl=(byte)GET_RX_STATUS_DESC_TSFL_8812(pdesc); */

  return pattrib;
}

__inline static u32 _RND8(u32 sz) {
  u32 val;

  val = ((sz >> 3) + ((sz & 7) ? 1 : 0)) << 3;

  return val;
}




std::vector<Packet> FrameParser::recvbuf2recvframe(std::span<uint8_t> ptr) {
  auto pbuf = ptr;
  auto pkt_cnt = GET_RX_STATUS_DESC_USB_AGG_PKTNUM_8812(pbuf.data());
  //_logger->info("pkt_cnt == {}", pkt_cnt);

  auto ret = std::vector<Packet>{};

  do {
    auto pattrib = rtl8812_query_rx_desc_status(pbuf.data());

    if ((pattrib.crc_err) || (pattrib.icv_err)) {
      _logger->info("RX Warning! crc_err={} "
                    "icv_err={}, skip!",
                    pattrib.crc_err, pattrib.icv_err);
      break;
    }

    auto pkt_offset = RXDESC_SIZE + pattrib.drvinfo_sz + pattrib.shift_sz +
                      pattrib.pkt_len; // this is offset for next package

    if ((pattrib.pkt_len <= 0) || (pkt_offset > pbuf.size())) {
      _logger->warn(
          "RX Warning!,pkt_len <= 0 or pkt_offset > transfer_len; pkt_len: "
          "{}, pkt_offset: {}, transfer_len: {}",
          pattrib.pkt_len, pkt_offset, pbuf.size());
      break;
    }

    if (pattrib.mfrag) {
      // !!! We skips this packages because ohd not use fragmentation
      _logger->warn("mfrag scipping");

      // if (rtw_os_alloc_recvframe(precvframe, pbuf.Slice(pattrib.shift_sz +
      // pattrib.drvinfo_sz + RXDESC_SIZE), pskb) == false)
      //{
      //     return false;
      // }
    }

    // recvframe_put(precvframe, pattrib.pkt_len);
    /* recvframe_pull(precvframe, drvinfo_sz + RXDESC_SIZE); */

    if (pattrib.pkt_rpt_type ==
        RX_PACKET_TYPE::NORMAL_RX) /* Normal rx packet */
    {
      ret.push_back({pattrib, pbuf.subspan(pattrib.shift_sz +
                                               pattrib.drvinfo_sz + RXDESC_SIZE,
                                           pattrib.pkt_len)});
      // pre_recv_entry(precvframe, pattrib.physt ? pbuf.Slice(RXDESC_OFFSET) :
      // null);
    } else {
      /* pkt_rpt_type == TX_REPORT1-CCX, TX_REPORT2-TX RTP,HIS_REPORT-USB HISR
       * RTP */
      if (pattrib.pkt_rpt_type == RX_PACKET_TYPE::C2H_PACKET) {
        _logger->info("RX USB C2H_PACKET");
        // rtw_hal_c2h_pkt_pre_hdl(padapter, precvframe.u.hdr.rx_data,
        // pattrib.pkt_len);
      } else if (pattrib.pkt_rpt_type == RX_PACKET_TYPE::HIS_REPORT) {
        _logger->info("RX USB HIS_REPORT");
      }
    }

    /* jaguar 8-byte alignment */
    pkt_offset = (uint16_t)_RND8(pkt_offset);
    // pkt_cnt--;

    if (pkt_offset >= pbuf.size()) {
      break;
    }
    pbuf = pbuf.subspan(pkt_offset, pbuf.size() - pkt_offset);
  } while (pbuf.size() > 0);

  if (pkt_cnt != 0) {
    _logger->info("Unprocessed packets: {}", pkt_cnt);
  }
  _logger->info("{} received in frame", ret.size());

  return ret;
}

void rtl8812a_cal_txdesc_chksum(uint8_t *ptxdesc)
{
	u16	*usPtr;
	u32 count;
	u32 index;
	u16 checksum = 0;


	usPtr = (u16 *)ptxdesc;

	/* checksume is always calculated by first 32 bytes, */
	/* and it doesn't depend on TX DESC length. */
	/* Thomas,Lucas@SD4,20130515 */
	count = 16;

	/* Clear first */
	SET_TX_DESC_TX_DESC_CHECKSUM_8812(ptxdesc, 0);

	for (index = 0 ; index < count ; index++)
		checksum = checksum ^ le16_to_cpu(*(usPtr + index));

	SET_TX_DESC_TX_DESC_CHECKSUM_8812(ptxdesc, checksum);

}

int rtw_action_frame_parse(const u8 *frame, u32 frame_len, u8 *category, u8 *action)
{
	/*const u8 *frame_body = frame + sizeof(struct rtw_ieee80211_hdr_3addr);
	u16 fc;
	u8 c;
	u8 a = ACT_PUBLIC_MAX;

	fc = le16_to_cpu(((struct rtw_ieee80211_hdr_3addr *)frame)->frame_ctl);

	if ((fc & (RTW_IEEE80211_FCTL_FTYPE | RTW_IEEE80211_FCTL_STYPE))
	    != (RTW_IEEE80211_FTYPE_MGMT | RTW_IEEE80211_STYPE_ACTION)
	   )
		return _FALSE;

	c = frame_body[0];

	switch (c) {
	case RTW_WLAN_CATEGORY_P2P: // vendor-specific 
		break;
	default:
		a = frame_body[1];
	}

	if (category)
		*category = c;
	if (action)
		*action = a;
*/
	return _TRUE;
	
}

void radiotap_to_txdesc(uint8_t *packet,uint8_t *usb_frame){/*
  int ret = 0;
	int rtap_len;
	int qos_len = 0;
	int dot11_hdr_len = 24;
	int snap_len = 6;
	unsigned char *pdata;
	u16 frame_ctl;
	unsigned char src_mac_addr[6];
	unsigned char dst_mac_addr[6];
	struct radiotap::ieee80211_radiotap_header *dot11_hdr;
  struct radiotap::ieee80211_radiotap_iterator iterator;

  u8 fixed_rate = MGN_1M, sgi = 0, bwidth = 0, ldpc = 0, stbc = 0;
  bool vht = false;
	u16 txflags = 0;

  u8 *buf=packet;
  u32 len=sizeof(packet);

  rtap_len = radiotap::ieee80211_get_radiotap_len(buf);
  u8 category,action;

  rtap_hdr = (struct radiotap::ieee80211_radiotap_header *)buf;

  ret = radiotap::rtw_ieee80211_radiotap_iterator_init(&iterator, rtap_hdr, len, NULL);
	while (!ret) {
    ret = radiotap::rtw_ieee80211_radiotap_iterator_next(&iterator);
    if (ret)
			continue;
    switch (iterator.this_arg_index) {

		case IEEE80211_RADIOTAP_RATE:		//
			fixed_rate = *iterator.this_arg;
			break;

		case IEEE80211_RADIOTAP_TX_FLAGS:
			txflags = get_unaligned_le16(iterator.this_arg);
			break;

		case IEEE80211_RADIOTAP_MCS: {		// u8,u8,u8 
			u8 mcs_have = iterator.this_arg[0];
			if (mcs_have & IEEE80211_RADIOTAP_MCS_HAVE_MCS) {
				fixed_rate = iterator.this_arg[2] & 0x7f;
				if(fixed_rate > 31)
					fixed_rate = 0;
				fixed_rate += MGN_MCS0;
			}
			if ((mcs_have & 4) && 
			    (iterator.this_arg[1] & 4))
				sgi = 1;
			if ((mcs_have & 1) && 
			    (iterator.this_arg[1] & 1))
				bwidth = 1;
			if ((mcs_have & 0x10) && 
			    (iterator.this_arg[1] & 0x10))
				ldpc = 1;
			if ((mcs_have & 0x20))
				stbc = (iterator.this_arg[1] >> 5) & 3;	
		}
		break;

		case IEEE80211_RADIOTAP_VHT: {
      vht = true;
		// u16 known, u8 flags, u8 bandwidth, u8 mcs_nss[4], u8 coding, u8 group_id, u16 partial_aid 
			u8 known = iterator.this_arg[0];
			u8 flags = iterator.this_arg[2];
			unsigned int mcs, nss;
			if((known & 4) && (flags & 4))
				sgi = 1;
			if((known & 1) && (flags & 1))
				stbc = 1;
			if(known & 0x40) {
				bwidth = iterator.this_arg[3] & 0x1f;
				if(bwidth>=1 && bwidth<=3)
					bwidth = 1; // 40 MHz
				else if(bwidth>=4 && bwidth<=10)
					bwidth = 2;	// 80 MHz
				else
					bwidth = 0; // 20 MHz
			}
			if(iterator.this_arg[8] & 1)
				ldpc = 1;
			mcs = (iterator.this_arg[4]>>4) & 0x0f;
			nss = iterator.this_arg[4] & 0x0f;
			if(nss > 0) {
				if(nss > 4) nss = 4;
				if(mcs > 9) mcs = 9;
				fixed_rate = MGN_VHT1SS_MCS0 + ((nss-1)*10 + mcs);
			}
		}
		break;

		default:
			break;
		}
  }

  buf = buf + rtap_len;
  //pattrib更新

  ptxdesc = (struct tx_desc *)usb_frame;

  ptxdesc->txdw0 |= cpu_to_le32((unsigned int)(len - rtap_len) & 0x0000ffff);
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
  SET_TX_DESC_HWSEQ_EN_8812(usb_frame, static_cast<uint8_t>(0)); // Hw do not set sequence number 
	SET_TX_DESC_SEQ_8812(usb_frame, 0); // Copy inject sequence number to TxDesc 

	SET_TX_DESC_RETRY_LIMIT_ENABLE_8812(usb_frame, static_cast<uint8_t>(1));

	SET_TX_DESC_DATA_RETRY_LIMIT_8812(usb_frame, static_cast<uint8_t>(0));

  SET_TX_DESC_DATA_SHORT_8812(usb_frame, static_cast<uint8_t>(sgi));


	SET_TX_DESC_DISABLE_FB_8812(usb_frame, static_cast<uint8_t>(1)); // svpcom: ?
	SET_TX_DESC_USE_RATE_8812(usb_frame, static_cast<uint8_t>(1));
  
  //MRateToHwRate(rate_id);
	SET_TX_DESC_TX_RATE_8812(usb_frame, static_cast<uint8_t>(MRateToHwRate(pattrib->rate))); // 原来设置的是6,也需要考虑下怎么转换
  
  if (ldpc)
		SET_TX_DESC_DATA_LDPC_8812(usb_frame, 1);
	if (stbc)	
		SET_TX_DESC_DATA_STBC_8812(usb_frame, 1);

  SET_TX_DESC_DATA_BW_8812(usb_frame, bwidth); 

  rtl8812a_cal_txdesc_chksum(usb_frame);
  

  */
  //未完待续

}
