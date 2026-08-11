#include "frame_codec.h"
#include <cstring>

namespace framecodec {

namespace {

// Fictional callsigns (see frame_codec.h header).
constexpr const char* kSatCallsign = "VANGRD";
constexpr uint8_t     kSatSsid     = 7;
constexpr const char* kGndCallsign = "GRNDCT";
constexpr uint8_t     kGndSsid     = 0;

char hexDigit(uint8_t v) { return v < 10 ? ('0' + v) : ('A' + (v - 10)); }

int hexVal(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

void appendByte(String& out, uint8_t b) {
    out += hexDigit(b >> 4);
    out += hexDigit(b & 0x0F);
}

// Fixed hex-string offsets/lengths for the header/trailer -- flag(1B) +
// dest(7B) + src(7B) + ctrl(1B) + pid(1B) = 17 bytes = 34 hex chars header;
// fcs(2B) + flag(1B) = 3 bytes = 6 hex chars trailer.
constexpr int kHeaderHexLen  = 2 + kAddrBytes * 2 * 2 + 2 + 2;
constexpr int kTrailerHexLen = 6;

} // namespace

void buildAddress(const char* callsign, uint8_t ssid, bool last, uint8_t out[kAddrBytes]) {
    size_t n = strlen(callsign);
    for (size_t i = 0; i < 6; i++) {
        char c = (i < n) ? callsign[i] : ' ';
        out[i] = (uint8_t)(c << 1);
    }
    // Bits 6-5 conventionally 1; bit0 is the address-extension bit.
    out[6] = 0x60 | ((ssid & 0x0F) << 1) | (last ? 0x01 : 0x00);
}

void satelliteAddress(bool last, uint8_t out[kAddrBytes]) {
    buildAddress(kSatCallsign, kSatSsid, last, out);
}

void groundAddress(bool last, uint8_t out[kAddrBytes]) {
    buildAddress(kGndCallsign, kGndSsid, last, out);
}

String buildFrame(const uint8_t dest[kAddrBytes], const uint8_t src[kAddrBytes],
                   uint8_t pid, const uint8_t* info, size_t infoLen) {
    String out;
    out.reserve(kHeaderHexLen + infoLen * 2 + kTrailerHexLen);
    appendByte(out, kFlag);
    for (size_t i = 0; i < kAddrBytes; i++) appendByte(out, dest[i]);
    for (size_t i = 0; i < kAddrBytes; i++) appendByte(out, src[i]);
    appendByte(out, kCtrl);
    appendByte(out, pid);
    for (size_t i = 0; i < infoLen; i++) appendByte(out, info[i]);
    uint16_t fcs = computeFcs(dest, src, pid, info, infoLen);
    appendByte(out, (uint8_t)(fcs >> 8));
    appendByte(out, (uint8_t)(fcs & 0xFF));
    appendByte(out, kFlag);
    return out;
}

bool parseFrame(const String& hexPkt, uint8_t destOut[kAddrBytes], uint8_t srcOut[kAddrBytes],
                 uint8_t& pid, uint8_t* infoOut, size_t infoCap, size_t& infoLen) {
    if (hexPkt.length() < (size_t)(kHeaderHexLen + kTrailerHexLen) || (hexPkt.length() % 2) != 0)
        return false;

    auto byteAt = [&](int hexOff) -> int {
        int hi = hexVal(hexPkt[hexOff]);
        int lo = hexVal(hexPkt[hexOff + 1]);
        if (hi < 0 || lo < 0) return -1;
        return (hi << 4) | lo;
    };

    if (byteAt(0) != kFlag) return false; // not one of ours

    int off = 2;
    for (size_t i = 0; i < kAddrBytes; i++) {
        int b = byteAt(off);
        if (b < 0) return false;
        destOut[i] = (uint8_t)b;
        off += 2;
    }
    for (size_t i = 0; i < kAddrBytes; i++) {
        int b = byteAt(off);
        if (b < 0) return false;
        srcOut[i] = (uint8_t)b;
        off += 2;
    }
    int ctrlByte = byteAt(off); off += 2;
    int pidByte  = byteAt(off); off += 2;
    if (ctrlByte < 0 || pidByte < 0) return false;

    int infoHexLen = (int)hexPkt.length() - kHeaderHexLen - kTrailerHexLen;
    if (infoHexLen < 0) return false;
    size_t n = (size_t)infoHexLen / 2;
    if (n > infoCap) n = infoCap; // truncate rather than overflow the caller's buffer

    for (size_t i = 0; i < n; i++) {
        int b = byteAt(off + (int)i * 2);
        if (b < 0) return false;
        infoOut[i] = (uint8_t)b;
    }

    pid = (uint8_t)pidByte;
    infoLen = n;
    return true;
}

// Simple additive checksum, not a cryptographic check. Missions 02-04 never
// validate it; radio_link is the first caller that does, via verifyFcs().
uint16_t computeFcs(const uint8_t dest[kAddrBytes], const uint8_t src[kAddrBytes],
                     uint8_t pid, const uint8_t* info, size_t infoLen) {
    uint16_t sum = kCtrl + pid;
    for (size_t i = 0; i < kAddrBytes; i++) sum += dest[i] + src[i];
    for (size_t i = 0; i < infoLen; i++) sum += info[i];
    return sum;
}

bool verifyFcs(const String& hexPkt) {
    uint8_t dest[kAddrBytes], src[kAddrBytes], pid;
    // Above the ~107-byte practical INFO ceiling so a valid frame never truncates here.
    constexpr size_t kMaxVerifyInfo = 128;
    uint8_t info[kMaxVerifyInfo];
    size_t infoLen = 0;
    if (!parseFrame(hexPkt, dest, src, pid, info, kMaxVerifyInfo, infoLen)) return false;

    // parseFrame() never reads the FCS bytes; pull them from the fixed trailer offset.
    if (hexPkt.length() < (size_t)kTrailerHexLen) return false;
    size_t fcsOff = hexPkt.length() - kTrailerHexLen;
    int hi1 = hexVal(hexPkt[fcsOff]),     lo1 = hexVal(hexPkt[fcsOff + 1]);
    int hi2 = hexVal(hexPkt[fcsOff + 2]), lo2 = hexVal(hexPkt[fcsOff + 3]);
    if (hi1 < 0 || lo1 < 0 || hi2 < 0 || lo2 < 0) return false;
    uint16_t gotFcs = (uint16_t)(((hi1 << 4 | lo1) << 8) | (hi2 << 4 | lo2));
    return gotFcs == computeFcs(dest, src, pid, info, infoLen);
}

void xorInPlace(uint8_t* buf, size_t len, uint8_t key) {
    for (size_t i = 0; i < len; i++) buf[i] ^= key;
}

bool isPrintableAscii(const uint8_t* buf, size_t len) {
    if (len == 0) return false;
    for (size_t i = 0; i < len; i++) {
        if (buf[i] < 0x20 || buf[i] > 0x7E) return false;
    }
    return true;
}

} // namespace framecodec
