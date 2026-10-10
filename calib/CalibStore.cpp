#include "calib/CalibStore.h"

#include <cstring>

#include "at32f425_flash.h"

namespace {
constexpr uint32_t CALIB_MAGIC = 0x314C4143;
constexpr uint16_t CALIB_VERSION = 2;
constexpr uint16_t CALIB_CHANNEL_COUNT = 4;
constexpr uint32_t CALIB_PAYLOAD_LEN = 128;
constexpr int CALIB_CRC_OFFSET = 148;
constexpr int CALIB_RECORD_LEN = 152;
constexpr int CALIB_PAYLOAD_OFFSET = 20;

#if defined(CONFIG_DEVICE_AD_4CH)
constexpr uint16_t CALIB_BOARD_ID = 1;
#elif defined(CONFIG_DEVICE_DA_4CH)
constexpr uint16_t CALIB_BOARD_ID = 2;
#else
constexpr uint16_t CALIB_BOARD_ID = 0;
#endif
}  // namespace

bool CalibStoreLoad(CalibRecord* out) {
    const uint8_t* base = reinterpret_cast<const uint8_t*>(CALIB_BASE);

    uint32_t magic;
    std::memcpy(&magic, base, 4);
    if (magic != CALIB_MAGIC) return false;

    uint16_t version;
    std::memcpy(&version, base + 4, 2);
    if (version != CALIB_VERSION) return false;

    uint16_t board;
    std::memcpy(&board, base + 6, 2);
    if (board != CALIB_BOARD_ID) return false;

    uint16_t degree;
    std::memcpy(&degree, base + 8, 2);
    if (degree != POLY_DEGREE) return false;

    uint16_t channelCount;
    std::memcpy(&channelCount, base + 10, 2);
    if (channelCount != CALIB_CHANNEL_COUNT) return false;

    uint32_t payloadLen;
    std::memcpy(&payloadLen, base + 12, 4);
    if (payloadLen != CALIB_PAYLOAD_LEN) return false;

    uint16_t validMask;
    std::memcpy(&validMask, base + 16, 2);
    if ((validMask & ~0xF) != 0) return false;

    uint32_t crcCalc = CalibCrc().Calculate(reinterpret_cast<const char*>(base), CALIB_CRC_OFFSET);
    uint32_t crcStored;
    std::memcpy(&crcStored, base + CALIB_CRC_OFFSET, 4);
    if (crcCalc != crcStored) return false;

    CalibRecord tmp;
    tmp.board = board;
    tmp.degree = degree;
    tmp.channelCount = channelCount;
    tmp.validMask = validMask;
    std::memcpy(tmp.coeff, base + CALIB_PAYLOAD_OFFSET, CALIB_PAYLOAD_LEN);
    *out = tmp;
    return true;
}

bool CalibStoreSave(const CalibRecord* in) {
    uint8_t buf[CALIB_RECORD_LEN];
    std::memset(buf, 0, sizeof(buf));

    uint32_t magic = CALIB_MAGIC;
    std::memcpy(buf + 0, &magic, 4);
    uint16_t version = CALIB_VERSION;
    std::memcpy(buf + 4, &version, 2);
    uint16_t board = CALIB_BOARD_ID;
    std::memcpy(buf + 6, &board, 2);
    uint16_t degree = POLY_DEGREE;
    std::memcpy(buf + 8, &degree, 2);
    uint16_t channelCount = CALIB_CHANNEL_COUNT;
    std::memcpy(buf + 10, &channelCount, 2);
    uint32_t payloadLen = CALIB_PAYLOAD_LEN;
    std::memcpy(buf + 12, &payloadLen, 4);
    uint16_t validMask = in->validMask;
    std::memcpy(buf + 16, &validMask, 2);
    std::memcpy(buf + CALIB_PAYLOAD_OFFSET, in->coeff, CALIB_PAYLOAD_LEN);

    uint32_t crcVal = CalibCrc().Calculate(reinterpret_cast<const char*>(buf), CALIB_CRC_OFFSET);
    std::memcpy(buf + CALIB_CRC_OFFSET, &crcVal, 4);

    flash_unlock();
    flash_sector_erase(CALIB_BASE);
    flash_operation_wait_for(0xFFFFFF);
    for (int i = 0; i < CALIB_RECORD_LEN / 4; i++) {
        uint32_t word;
        std::memcpy(&word, buf + 4 * i, 4);
        flash_word_program(CALIB_BASE + 4 * i, word);
        flash_operation_wait_for(0xFFFFFF);
    }
    flash_lock();

    CalibRecord rb;
    if (!CalibStoreLoad(&rb)) return false;
    if (rb.board != in->board || rb.degree != in->degree || rb.channelCount != in->channelCount ||
        rb.validMask != in->validMask)
        return false;
    return std::memcmp(rb.coeff, in->coeff, sizeof(rb.coeff)) == 0;
}
