#pragma once

#include <array>
#include <cstdint>
#include <cstring>

namespace cyrus_ble {

namespace protocol {

inline constexpr char SERVICE_UUID[] = "bc2f4cc6-aaef-4351-9034-d66268e328f0";
inline constexpr char CHARACTERISTIC_UUID[] = "06d1e5e7-79ad-4a71-8faa-373789f7d93c";
inline constexpr uint8_t VOLUME_PREFIX[] = {0x40, 0x2B, 0x56, 0x32};
inline constexpr uint8_t VOLUME_SUFFIX = 0x25;
inline constexpr uint8_t VOLUME_PACKET_SIZE = 7;

inline std::array<uint8_t, VOLUME_PACKET_SIZE> build_volume_packet(int volume) {
  std::array<uint8_t, VOLUME_PACKET_SIZE> msg{};
  std::memcpy(msg.data(), VOLUME_PREFIX, sizeof(VOLUME_PREFIX));
  const int clamped = volume < 0 ? 0 : (volume > 90 ? 90 : volume);
  msg[4] = static_cast<uint8_t>('0' + (clamped / 10));
  msg[5] = static_cast<uint8_t>('0' + (clamped % 10));
  msg[6] = VOLUME_SUFFIX;
  return msg;
}

}  // namespace protocol

}  // namespace cyrus_ble
