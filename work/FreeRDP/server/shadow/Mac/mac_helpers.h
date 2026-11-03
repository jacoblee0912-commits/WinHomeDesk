/* WinHomeDesk macOS. Apache-2.0. Pure helpers shared by input and tests. */
#ifndef WINHOMEDESK_MAC_HELPERS_H
#define WINHOMEDESK_MAC_HELPERS_H
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
static inline bool whd_hex(const char *text, uint8_t *out, size_t n) {
    if (!text || strlen(text) != n * 2) return false;
    for (size_t i = 0; i < n; i++) {
        unsigned value = 0;
        for (size_t j = 0; j < 2; j++) {
            char c = text[i * 2 + j];
            unsigned digit;
            if (c >= '0' && c <= '9') digit = c - '0';
            else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') digit = c - 'A' + 10;
            else return false;
            value = value * 16 + digit;
        }
        out[i] = (uint8_t)value;
    }
    return true;
}
static inline int whd_wheel(uint16_t flags) {
    int amount = flags & 0x1ff;
    return (flags & 0x100) ? amount - 0x200 : amount;
}
static inline double whd_coordinate(unsigned value, unsigned size, double origin, double extent) {
    if (size < 2 || extent < 1) return origin;
    if (value >= size) value = size - 1;
    return origin + value * (extent - 1) / (size - 1);
}
enum whd_ime_action { WHD_IME_NONE, WHD_IME_CONSUME, WHD_IME_TOGGLE };
/* Korean dedicated keys can have normalized 72 or raw F2 scan codes and
 * may arrive as release-only events. A normal press/release toggles once. */
static inline enum whd_ime_action whd_hangul_event(uint8_t *held, uint8_t scan,
        bool extended, bool release, bool korean_layout) {
    uint8_t bit = (scan == 0x72 || scan == 0xf2) ? 1 :
        (scan == 0x38 && extended && korean_layout) ? 2 : 0;
    if (!bit) return WHD_IME_NONE;
    if (release) {
        if (*held & bit) { *held &= (uint8_t)~bit; return WHD_IME_CONSUME; }
        return WHD_IME_TOGGLE;
    }
    if (*held & bit) return WHD_IME_CONSUME;
    *held |= bit;
    return WHD_IME_TOGGLE;
}
#endif
