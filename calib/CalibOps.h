#pragma once

#include <cstdint>

constexpr uint8_t CALIB_FRAME_HEAD = 0xA5;
constexpr int CALIB_MAX_PAYLOAD = 26;

// REF 应答协议（保持原值）
constexpr uint8_t OP_PING = 0x7F, OP_PONG = 0xFF, OP_SET_DA = 0x01, OP_ACK_SET_DA = 0x81,
                  OP_READ_AD = 0x02, OP_ACK_READ_AD = 0x82, OP_NACK = 0x00;

// 手动读写的取值语义：RAW = 直接码，CAL = 经校准（DA 写按 ideal 查系数得 raw；AD 读回 ideal）
constexpr uint8_t OP_MODE_RAW = 0x00, OP_MODE_CAL = 0x01;

// 上位机→DUT 控制协议（RTT 下行）
constexpr uint8_t OP_HOST_CONFIG = 0x10, OP_HOST_START = 0x11, OP_HOST_PAUSE = 0x12, OP_HOST_RESUME = 0x13,
                  OP_HOST_RESTART = 0x14, OP_HOST_STOP = 0x15, OP_HOST_SET_DA = 0x16, OP_HOST_READ_AD = 0x17,
                  OP_HOST_STATUS = 0x18, OP_HOST_VERIFY = 0x19, OP_HOST_PARAMS = 0x1A;

// DUT ↔ REF 信息查询（RS485）
constexpr uint8_t OP_GET_INFO = 0x03, OP_ACK_INFO = 0x83;

// 上位机→DUT 手动校准协议（RTT 下行，单板）
constexpr uint8_t OP_HOST_MANUAL_BEGIN = 0x20, OP_HOST_MANUAL_ABORT = 0x21, OP_HOST_MANUAL_SET_RAW = 0x22,
                  OP_HOST_MANUAL_ADD = 0x23, OP_HOST_MANUAL_FIT = 0x24, OP_HOST_MANUAL_SAVE = 0x25;
