#pragma once

#include <cstdint>

extern "C" {
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/ble_hs.h"
}

namespace cyrus_ble {

class BleConnectionListener {
 public:
  virtual ~BleConnectionListener() = default;
  virtual void on_connected() = 0;
  virtual void on_connection_failed(int reason) = 0;
  virtual void on_disconnected(int reason) = 0;
  virtual void on_volume_written(bool ok) = 0;
};

class BleConnection {
 public:
  BleConnection() = default;

  void set_listener(BleConnectionListener *listener) { listener_ = listener; }

  bool is_connected() const { return conn_handle_ != BLE_HS_CONN_HANDLE_NONE; }
  uint16_t conn_handle() const { return conn_handle_; }

  void set_pending_volume(int volume);
  void connect(const ble_addr_t &addr);
  void disconnect();
  void write_volume(int volume);

  // Dispatches NimBLE GAP/GATT events registered during connect().
  void on_gap_event(const struct ble_gap_event *event);

 private:
  static int gap_event_cb(struct ble_gap_event *event, void *arg);
  static int svc_disced_cb(uint16_t conn_handle, const struct ble_gatt_error *error,
                           const struct ble_gatt_svc *service, void *arg);
  static int chr_disced_cb(uint16_t conn_handle, const struct ble_gatt_error *error,
                           const struct ble_gatt_chr *chr, void *arg);
  static int write_cb(uint16_t conn_handle, const struct ble_gatt_error *error,
                      struct ble_gatt_attr *attr, void *arg);

  void handle_connect(const struct ble_gap_event *event);
  void handle_disconnect(const struct ble_gap_event *event);
  void discover_services();
  void discover_characteristics();

  void notify_connected();
  void notify_connection_failed(int reason);
  void notify_disconnected(int reason);
  void notify_volume_written(bool ok);

  BleConnectionListener *listener_{nullptr};
  uint16_t conn_handle_{BLE_HS_CONN_HANDLE_NONE};
  uint16_t svc_start_{0};
  uint16_t svc_end_{0};
  uint16_t attr_handle_{0};
  bool connecting_{false};
  int pending_volume_{0};
};

}  // namespace cyrus_ble
