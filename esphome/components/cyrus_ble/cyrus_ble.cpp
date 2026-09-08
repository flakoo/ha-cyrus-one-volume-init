#include "cyrus_ble.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <cstring>
#include <cstdio>

namespace cyrus_ble {

CyrusBleComponent *CyrusBleComponent::instance_ = nullptr;

CyrusBleComponent::CyrusBleComponent() {
  scanner_.set_listener(this);
  connection_.set_listener(this);
}

void CyrusBleComponent::set_status_sensor(esphome::binary_sensor::BinarySensor *status_sensor) {
  status_sensor_ = status_sensor;
}

void CyrusBleComponent::set_led(esphome::light::LightState *led) {
  led_indicator_.set_light(led);
}

void CyrusBleComponent::on_ha_connected() {
  led_indicator_.on_ha_connected();
}

void CyrusBleComponent::setup() {
  ESP_LOGI("cyrus", "Initializing native NimBLE");

  const int rc = nimble_port_init();
  if (rc != 0) {
    ESP_LOGE("cyrus", "nimble_port_init failed: %d", rc);
    return;
  }

  instance_ = this;
  ble_hs_cfg.sync_cb = sync_cb;
  ble_hs_cfg.reset_cb = reset_cb;

  nimble_port_freertos_init(nimble_host_task);
}

void CyrusBleComponent::request_volume(int volume) {
  if (scanner_.is_scanning()) {
    ESP_LOGW("cyrus", "Cancelling previous scan");
    scanner_.stop();
  }

  target_volume_ = std::max(0, std::min(90, volume));
  state_ = State::SCANNING;
  scan_start_time_ = esphome::millis();

  led_indicator_.on_volume_request();

  ESP_LOGI("cyrus", "Volume request: %d", target_volume_);

  start_scan_if_synced();
}

void CyrusBleComponent::loop() {
  if (state_ == State::IDLE) {
    led_indicator_.start_boot();
  }
  led_indicator_.loop();

  if (state_ == State::PULSE) {
    const uint32_t now = esphome::millis();
    if (now - pulse_start_ >= 500) {
      if (status_sensor_ != nullptr) {
        status_sensor_->publish_state(false);
      }
    }
  }

  if (state_ != State::SCANNING)
    return;

  const uint32_t now = esphome::millis();
  if (now - scan_start_time_ >= SCAN_TIMEOUT_MS) {
    on_scan_timeout();
  } else if (scanner_.boot_stage() == 2 && scanner_.mac_a_time() != 0 &&
             (now - scanner_.mac_a_time()) >= MAC_A_TO_B_TIMEOUT_MS) {
    on_mac_b_timeout();
  }
}

CyrusBleComponent *CyrusBleComponent::instance() {
  return instance_;
}

void CyrusBleComponent::nimble_host_task(void *param) {
  (void)param;
  ESP_LOGI("cyrus", "NimBLE host task started");
  nimble_port_run();
  nimble_port_freertos_deinit();
}

void CyrusBleComponent::sync_cb() {
  ESP_LOGI("cyrus", "NimBLE host synced");
  if (instance() != nullptr && instance()->state_ == State::SCANNING) {
    instance()->start_scan_if_synced();
  }
}

void CyrusBleComponent::reset_cb(int reason) {
  ESP_LOGE("cyrus", "NimBLE host reset: %d", reason);
}

void CyrusBleComponent::start_scan_if_synced() {
  if (scanner_.is_scanning()) {
    return;
  }
  if (!ble_hs_synced()) {
    ESP_LOGI("cyrus", "NimBLE not synced yet, scan will start after sync");
    return;
  }
  scanner_.start();
}

void CyrusBleComponent::on_first_mac(const ble_addr_t &addr) {
  first_addr_ = addr;
  led_indicator_.on_first_mac();
}

void CyrusBleComponent::on_second_mac(const ble_addr_t &addr) {
  second_addr_ = addr;
  state_ = State::READY;
  led_indicator_.on_second_mac();
  scanner_.stop();
  connection_.set_pending_volume(target_volume_);
  connection_.connect(addr);
}

void CyrusBleComponent::on_connected() {
  state_ = State::CONNECTED;
  led_indicator_.on_bt_connected();
}

void CyrusBleComponent::on_connection_failed(int reason) {
  ESP_LOGE("cyrus", "Connection failed: %d", reason);
  state_ = State::IDLE;
  publish_status(false);
}

void CyrusBleComponent::on_disconnected(int reason) {
  if (state_ != State::PULSE) {
    state_ = State::IDLE;
  }
}

void CyrusBleComponent::on_volume_written(bool ok) {
  publish_status(ok);
}

void CyrusBleComponent::on_scan_timeout() {
  ESP_LOGW("cyrus", "Scan timeout (%lu ms), giving up", SCAN_TIMEOUT_MS);
  scanner_.stop();
  state_ = State::IDLE;
  publish_status(false);
}

void CyrusBleComponent::on_mac_b_timeout() {
  ESP_LOGW("cyrus", "MAC B timeout (%lu ms after MAC A), falling back to MAC A",
           MAC_A_TO_B_TIMEOUT_MS);
  scanner_.stop();
  scanner_.clear_mac_a_time();
  connect_to_mac_a_fallback();
}

void CyrusBleComponent::connect_to_mac_a_fallback() {
  char addr_str[18];
  snprintf(addr_str, sizeof(addr_str), "%02x:%02x:%02x:%02x:%02x:%02x",
           first_addr_.val[5], first_addr_.val[4], first_addr_.val[3],
           first_addr_.val[2], first_addr_.val[1], first_addr_.val[0]);
  ESP_LOGW("cyrus", "Fallback: connecting to MAC A %s", addr_str);
  connection_.set_pending_volume(target_volume_);
  connection_.connect(first_addr_);
}

void CyrusBleComponent::publish_status(bool ok) {
  pulse_start_ = esphome::millis();
  state_ = State::PULSE;
  led_indicator_.on_finished(ok);
  if (status_sensor_ != nullptr) {
    status_sensor_->publish_state(ok);
  }
}

}  // namespace cyrus_ble
