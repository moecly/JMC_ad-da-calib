#include "SerialOutStream.h"

#include <cstring>

#include "at32f425_board.h"

#define USE_ISR

SerialOutStream::SerialOutStream() : m_head(0), m_tail(0) {
#if defined(USE_ISR)
    for (int i = 0; i < MAX_BUF_NUM; i++) {
        m_buf[i] = new char[MAX_BUF_SIZE];
    }
#endif
}

SerialOutStream::~SerialOutStream() {
#if defined(USE_ISR)
    for (int i = 0; i < MAX_BUF_NUM; i++) {
        delete m_buf[i];
    }
#endif
}

void SerialOutStream::Write(const char* message) {
#if defined(USE_ISR)
    if (message == nullptr) return;
    int tmp = (m_tail + 1) >= MAX_BUF_NUM ? 0 : m_tail + 1;
    if (tmp == m_head) return;
    int strLen = strlen(message);
    int copyLen = strLen >= (MAX_BUF_SIZE - 1) ? (MAX_BUF_SIZE - 1) : strLen;
    memcpy(m_buf[tmp], message, copyLen);
    m_buf[tmp][copyLen] = '\0';
    m_tail = tmp;
#else
    if (message == nullptr) return;
    char* p = const_cast<char*>(message);
    while (*p != '\0') {
        while (usart_flag_get(USART4, USART_TDBE_FLAG) == RESET);
        usart_data_transmit(USART4, *p);
        while (usart_flag_get(USART4, USART_TDC_FLAG) == RESET);
        p++;
    }
#endif
}

void SerialOutStream::Loop() {
#if defined(USE_ISR)
    if (m_head == m_tail) return;
    int tmp = (m_head + 1) >= MAX_BUF_NUM ? 0 : m_head + 1;
    char* p = reinterpret_cast<char*>(m_buf[tmp]);
    while (*p != '\0') {
        while (usart_flag_get(USART4, USART_TDBE_FLAG) == RESET);
        usart_data_transmit(USART4, *p);
        while (usart_flag_get(USART4, USART_TDC_FLAG) == RESET);
        p++;
    }
    // printf("%s", p);
    m_head = tmp;
#endif
}
