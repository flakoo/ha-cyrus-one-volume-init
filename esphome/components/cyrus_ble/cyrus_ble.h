#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "ble_scanner.h"
#include "ble_connection.h"
#include "cyrus_led.h"

extern "C" {
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
}

namespace cyrus_ble {

class CyrusBleComponent : public esphome::Component,
                          public BleScannerListener,
                          public BleConnectionListener {
 public:
  CyrusBleComponent();

  void set_status_sensor(esphome::binary_sensor::BinarySensor *status_sensor);
  void set_led(esphome::light::LightState *led);

  void on_ha_connected();

  void setup() override;
  void loop() override;

  void request_volume(int volume);

  // BleScannerListener
  void on_first_mac(const ble_addr_t &addr) override;
  void on_second_mac(const ble_addr_t &addr) override;

  // BleConnectionListener
  void on_connected() override;
  void on_connection_failed(int reason) override;
  void on_disconnected(int reason) override;
  void on_volume_written(bool ok) override;

 private:
  enum class State {
    IDLE,
    SCANNING,
    READY,
    CONNECTING,
    CONNECTED,
    DISCOVERING_SVC,
    DISCOVERING_CHR,
    WRITING,
    DISCONNECTING,
    PULSE
  };

  static constexpr uint32_t SCAN_TIMEOUT_MS = 15000;
  static constexpr uint32_t MAC_A_TO_B_TIMEOUT_MS = 4500;

  esphome::binary_sensor::BinarySensor *status_sensor_{nullptr};
  State state_{State::IDLE};
  int target_volume_{46};

  BleScanner scanner_;
  BleConnection connection_;
  CyrusLedIndicator led_indicator_;

  ble_addr_t first_addr_{};
  ble_addr_t second_addr_{};

  static CyrusBleComponent *instance_;
  static CyrusBleComponent *instance();

  uint32_t scan_start_time_{0};
  uint32_t pulse_start_{0};

  static void nimble_host_task(void *param);
  static void sync_cb();
  static void reset_cb(int reason);

  void start_scan_if_synced();
  void on_scan_timeout();
  void on_mac_b_timeout();
  void connect_to_mac_a_fallback();
  void publish_status(bool ok);
};

}  // namespace cyrus_ble
