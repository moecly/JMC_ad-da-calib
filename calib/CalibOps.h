#pragma once

#include <cstdint>

constexpr uint8_t CALIB_FRAME_HEAD = 0xA5;
constexpr int CALIB_MAX_PAYLOAD = 26;

// REF 应答协议（保持原值）
constexpr uint8_t OP_PING = 0x7F, OP_PONG = 0xFF, OP_SET_DA = 0x01, OP_ACK_SET_DA = 0x81,
                  OP_READ_AD = 0x02, OP_ACK_READ_AD = 0x82, OP_NACK = 0x00;

// 上位机→DUT 控制协议（RTT 下行）
constexpr uint8_t OP_HOST_CONFIG = 0x10, OP_HOST_START = 0x11, OP_HOST_PAUSE = 0x12, OP_HOST_RESUME = 0x13,
                  OP_HOST_RESTART = 0x14, OP_HOST_STOP = 0x15, OP_HOST_SET_DA = 0x16, OP_HOST_READ_AD = 0x17,
                  OP_HOST_STATUS = 0x18;
