#include "ble_connection.h"
#include "cyrus_protocol.h"
#include "esphome/core/log.h"

namespace cyrus_ble {

void BleConnection::set_pending_volume(int volume) {
  pending_volume_ = volume;
}

void BleConnection::connect(const ble_addr_t &addr) {
  // Reset discovery handles in case of reuse.
  attr_handle_ = 0;
  svc_start_ = 0;
  svc_end_ = 0;
  connecting_ = true;

  struct ble_gap_conn_params conn_params = {0};
  conn_params.scan_itvl = 0x10;
  conn_params.scan_window = 0x10;
  conn_params.itvl_min = BLE_GAP_INITIAL_CONN_ITVL_MIN;
  conn_params.itvl_max = BLE_GAP_INITIAL_CONN_ITVL_MAX;
  conn_params.latency = 0;
  conn_params.supervision_timeout = 500;
  conn_params.min_ce_len = BLE_GAP_INITIAL_CONN_MIN_CE_LEN;
  conn_params.max_ce_len = BLE_GAP_INITIAL_CONN_MAX_CE_LEN;

  const int rc = ble_gap_connect(BLE_OWN_ADDR_PUBLIC, &addr, 10 * 1000,
                                 &conn_params, gap_event_cb, this);
  if (rc != 0) {
    ESP_LOGE("cyrus", "Connect failed to start: %d", rc);
    connecting_ = false;
    notify_connection_failed(rc);
  } else {
    ESP_LOGI("cyrus", "Connecting to target");
  }
}

void BleConnection::disconnect() {
  if (conn_handle_ != BLE_HS_CONN_HANDLE_NONE) {
    ESP_LOGI("cyrus", "Disconnecting");
    ble_gap_terminate(conn_handle_, BLE_ERR_REM_USER_CONN_TERM);
  }
}

void BleConnection::write_volume(int volume) {
  const auto msg = protocol::build_volume_packet(volume);

  const int rc = ble_gattc_write_flat(conn_handle_, attr_handle_, msg.data(),
                                      msg.size(), write_cb, this);
  if (rc == 0) {
    ESP_LOGI("cyrus", "Writing volume=%d", volume);
  } else {
    ESP_LOGE("cyrus", "Write failed to start: %d", rc);
    disconnect();
    notify_volume_written(false);
  }
}

void BleConnection::on_gap_event(const struct ble_gap_event *event) {
  switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
      handle_connect(event);
      break;
    case BLE_GAP_EVENT_DISCONNECT:
      handle_disconnect(event);
      break;
    default:
      break;
  }
}

int BleConnection::gap_event_cb(struct ble_gap_event *event, void *arg) {
  auto *self = static_cast<BleConnection *>(arg);
  self->on_gap_event(event);
  return 0;
}

void BleConnection::handle_connect(const struct ble_gap_event *event) {
  connecting_ = false;
  if (event->connect.status == 0) {
    conn_handle_ = event->connect.conn_handle;
    ESP_LOGI("cyrus", "Connected, conn_handle=%d", conn_handle_);
    notify_connected();
    discover_services();
  } else {
    ESP_LOGE("cyrus", "Connect failed: %d", event->connect.status);
    notify_disconnected(event->connect.status);
  }
}

void BleConnection::handle_disconnect(const struct ble_gap_event *event) {
  ESP_LOGI("cyrus", "Disconnected, reason=%d", event->disconnect.reason);
  conn_handle_ = BLE_HS_CONN_HANDLE_NONE;
  attr_handle_ = 0;
  svc_start_ = 0;
  svc_end_ = 0;
  notify_disconnected(event->disconnect.reason);
}

void BleConnection::discover_services() {
  const int rc = ble_gattc_disc_all_svcs(conn_handle_, svc_disced_cb, this);
  if (rc != 0) {
    ESP_LOGE("cyrus", "Service discovery failed to start: %d", rc);
    disconnect();
    notify_volume_written(false);
  }
}

int BleConnection::svc_disced_cb(uint16_t conn_handle,
                                 const struct ble_gatt_error *error,
                                 const struct ble_gatt_svc *service,
                                 void *arg) {
  (void)conn_handle;
  auto *self = static_cast<BleConnection *>(arg);

  if (error->status == BLE_HS_EDONE || service == nullptr) {
    if (self->svc_start_ != 0) {
      ESP_LOGI("cyrus", "Discovering characteristics");
      self->discover_characteristics();
    } else {
      ESP_LOGE("cyrus", "Cyrus service not found");
      self->disconnect();
      self->notify_volume_written(false);
    }
    return 0;
  }

  ble_uuid_any_t target_uuid;
  if (ble_uuid_from_str(&target_uuid, protocol::SERVICE_UUID) != 0)
    return 0;

  if (ble_uuid_cmp(&service->uuid.u, &target_uuid.u) == 0) {
    self->svc_start_ = service->start_handle;
    self->svc_end_ = service->end_handle;
    ESP_LOGI("cyrus", "Cyrus service found: %d-%d", self->svc_start_, self->svc_end_);
  }

  return 0;
}

void BleConnection::discover_characteristics() {
  const int rc = ble_gattc_disc_all_chrs(conn_handle_, svc_start_, svc_end_,
                                         chr_disced_cb, this);
  if (rc != 0) {
    ESP_LOGE("cyrus", "Char discovery failed to start: %d", rc);
    disconnect();
    notify_volume_written(false);
  }
}

int BleConnection::chr_disced_cb(uint16_t conn_handle,
                                 const struct ble_gatt_error *error,
                                 const struct ble_gatt_chr *chr,
                                 void *arg) {
  (void)conn_handle;
  auto *self = static_cast<BleConnection *>(arg);

  if (error->status == BLE_HS_EDONE || chr == nullptr) {
    if (self->attr_handle_ != 0) {
      self->write_volume(self->pending_volume_);
    } else {
      ESP_LOGE("cyrus", "Cyrus characteristic not found");
      self->disconnect();
      self->notify_volume_written(false);
    }
    return 0;
  }

  ble_uuid_any_t target_uuid;
  if (ble_uuid_from_str(&target_uuid, protocol::CHARACTERISTIC_UUID) != 0)
    return 0;

  if (ble_uuid_cmp(&chr->uuid.u, &target_uuid.u) == 0) {
    self->attr_handle_ = chr->val_handle;
    ESP_LOGI("cyrus", "Cyrus char found, handle=%d", self->attr_handle_);
  }

  return 0;
}

int BleConnection::write_cb(uint16_t conn_handle,
                            const struct ble_gatt_error *error,
                            struct ble_gatt_attr *attr,
                            void *arg) {
  (void)conn_handle;
  (void)attr;
  auto *self = static_cast<BleConnection *>(arg);

  // Cyrus returns error 0x10E (GATT Unlikely Error / MTU bug), but command works
  if (error->status == 0 || error->status == 0x10E) {
    ESP_LOGI("cyrus", "Volume write successful");
    self->notify_volume_written(true);
  } else {
    ESP_LOGE("cyrus", "Write error: %d", error->status);
    self->notify_volume_written(false);
  }

  self->disconnect();
  return 0;
}

void BleConnection::notify_connected() {
  if (listener_ != nullptr) {
    listener_->on_connected();
  }
}

void BleConnection::notify_connection_failed(int reason) {
  if (listener_ != nullptr) {
    listener_->on_connection_failed(reason);
  }
}

void BleConnection::notify_disconnected(int reason) {
  if (listener_ != nullptr) {
    listener_->on_disconnected(reason);
  }
}

void BleConnection::notify_volume_written(bool ok) {
  if (listener_ != nullptr) {
    listener_->on_volume_written(ok);
  }
}

}  // namespace cyrus_ble
