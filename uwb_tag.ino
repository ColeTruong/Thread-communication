
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEAdvertising.h>

BLEAdvertising *pAdvertising;

#include "dw3000.h"

#define APP_NAME "SS TWR INIT v1.0"

const uint8_t PIN_RST = 27;
const uint8_t PIN_IRQ = 34;
const uint8_t PIN_SS = 4;

static double tof;
static double distance;

static dwt_config_t config = {
  5,
  DWT_PLEN_128, 
  DWT_PAC8,
  9,
  9,
  1,
  DWT_BR_6M8,
  DWT_PHRMODE_STD,
  DWT_PHRRATE_STD,
  (128 + 1 + 8 - 8),
  DWT_STS_MODE_OFF,
  DWT_STS_LEN_128,
  DWT_PDOA_M0
};

#define RNG_DELAY_MS 100
#define TX_ANT_DLY 16399
#define RX_ANT_DLY 16399

static uint8_t tx_poll_msg[] = {0x41, 0x88, 0, 0xCA, 0xDE, 'W', 'A', 'V', 'E', 0xE0, 0, 0};
static uint8_t rx_resp_msg[] = {0x41, 0x88, 0, 0xCA, 0xDE, 'V', 'E', 'W', 'A', 0xE1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

#define ALL_MSG_COMMON_LEN 10
#define ALL_MSG_SN_IDX 2
#define RESP_MSG_POLL_RX_TS_IDX 10
#define RESP_MSG_RESP_TX_TS_IDX 14
#define RESP_MSG_TS_LEN 4

static uint8_t frame_seq_nb = 0;
#define RX_BUF_LEN 20
static uint8_t rx_buffer[RX_BUF_LEN];

static uint32_t status_reg = 0;
#define POLL_TX_TO_RESP_RX_DLY_UUS 1720
#define RESP_RX_TIMEOUT_UUS 250

extern dwt_txconfig_t txconfig_options;

void setup() {
  Serial.begin(115200);
  UART_init();
  test_run_info((unsigned char *)APP_NAME);

  spiBegin(PIN_IRQ, PIN_RST);
  spiSelect(PIN_SS);
  delay(2);

  while (!dwt_checkidlerc()) {
    Serial.println("IDLE FAILED");
    while (1);
  }

  if (dwt_initialise(DWT_DW_INIT) == DWT_ERROR) {
    Serial.println("INIT FAILED");
    while (1);
  }

  dwt_setleds(DWT_LEDS_ENABLE | DWT_LEDS_INIT_BLINK);

  if (dwt_configure(&config)) {
    Serial.println("CONFIG FAILED");
    while (1);
  }

  dwt_configuretxrf(&txconfig_options);
  dwt_setrxantennadelay(RX_ANT_DLY);
  dwt_settxantennadelay(TX_ANT_DLY);
  dwt_setrxaftertxdelay(POLL_TX_TO_RESP_RX_DLY_UUS);
  dwt_setrxtimeout(RESP_RX_TIMEOUT_UUS);
  dwt_setlnapamode(DWT_LNA_ENABLE | DWT_PA_ENABLE);

    // BLE Init
  BLEDevice::init("UWB_Tag");
  pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->setScanResponse(true);
  // Advertise device name in scan response data:
  BLEAdvertisementData scanResponseData;
  scanResponseData.setName("UWB_Tag");
  pAdvertising->setScanResponseData(scanResponseData);
  
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  pAdvertising->start();

  Serial.println("BLE advertising started.");
}

void loop() {
  tx_poll_msg[ALL_MSG_SN_IDX] = frame_seq_nb;
  dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_TXFRS_BIT_MASK);
  dwt_writetxdata(sizeof(tx_poll_msg), tx_poll_msg, 0);
  dwt_writetxfctrl(sizeof(tx_poll_msg), 0, 1);
  dwt_starttx(DWT_START_TX_IMMEDIATE | DWT_RESPONSE_EXPECTED);

  while (!((status_reg = dwt_read32bitreg(SYS_STATUS_ID)) & 
    (SYS_STATUS_RXFCG_BIT_MASK | SYS_STATUS_ALL_RX_TO | SYS_STATUS_ALL_RX_ERR))) { }

  frame_seq_nb++;

  if (status_reg & SYS_STATUS_RXFCG_BIT_MASK) {
    uint32_t frame_len;
    dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_RXFCG_BIT_MASK);
    frame_len = dwt_read32bitreg(RX_FINFO_ID) & RXFLEN_MASK;

    if (frame_len <= sizeof(rx_buffer)) {
      dwt_readrxdata(rx_buffer, frame_len, 0);
      rx_buffer[ALL_MSG_SN_IDX] = 0;

      if (memcmp(rx_buffer, rx_resp_msg, ALL_MSG_COMMON_LEN) == 0) {
        uint32_t poll_tx_ts, resp_rx_ts, poll_rx_ts, resp_tx_ts;
        int32_t rtd_init, rtd_resp;
        float clockOffsetRatio;

        poll_tx_ts = dwt_readtxtimestamplo32();
        resp_rx_ts = dwt_readrxtimestamplo32();
        clockOffsetRatio = ((float)dwt_readclockoffset()) / (uint32_t)(1 << 26);

        resp_msg_get_ts(&rx_buffer[RESP_MSG_POLL_RX_TS_IDX], &poll_rx_ts);
        resp_msg_get_ts(&rx_buffer[RESP_MSG_RESP_TX_TS_IDX], &resp_tx_ts);

        rtd_init = resp_rx_ts - poll_tx_ts;
        rtd_resp = resp_tx_ts - poll_rx_ts;

        tof = ((rtd_init - rtd_resp * (1 - clockOffsetRatio)) / 2.0) * DWT_TIME_UNITS;
        distance = tof * SPEED_OF_LIGHT;

        Serial.print("Distance: ");
        Serial.print(distance*100, 3);
        Serial.println(" cm");
        
       // BLE Advertise
        uint16_t dist_cm = (uint16_t)(distance * 100);  // convert to cm
        uint8_t mfg_data[6] = {
          0x34, 0x12,  // Manufacturer ID = 0x1234
          (uint8_t)(dist_cm & 0xFF),
          (uint8_t)(dist_cm >> 8),
          0x00, 0x00   // padding or future data
        };
      
        String mfgString = "";
        for (size_t i = 0; i < sizeof(mfg_data); i++) {
          mfgString += (char)mfg_data[i];
        };
        BLEAdvertisementData oAdvertisementData;
        oAdvertisementData.setFlags(0x06); // General discoverable
        oAdvertisementData.setManufacturerData(mfgString);
        
        pAdvertising->setAdvertisementData(oAdvertisementData);
        pAdvertising->start();
      }
    }
  } else {
    dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_ALL_RX_TO | SYS_STATUS_ALL_RX_ERR);
  }

  delay(RNG_DELAY_MS);
}
