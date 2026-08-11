#include "badge_id.h"
#include <esp_mac.h>
#include <mbedtls/sha256.h>

namespace badge_id {

// SHA-256(station MAC + salt), first hexLen hex chars. Uses esp_read_mac() (normal
// octet order), not ESP.getEfuseMac()'s raw uint64 packing, to avoid an endianness trap.
static String hashedMacHex(const char* salt, int hexLen) {
    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    uint8_t input[6 + 16];
    memcpy(input, mac, 6);
    size_t saltLen = strlen(salt);
    if (saltLen > sizeof(input) - 6) saltLen = sizeof(input) - 6;
    memcpy(input + 6, salt, saltLen);

    uint8_t digest[32];
    mbedtls_sha256(input, 6 + saltLen, digest, 0);

    char buf[65];
    int n = hexLen / 2 + (hexLen % 2);
    if (n > (int)sizeof(digest)) n = sizeof(digest);
    for (int i = 0; i < n; i++) snprintf(buf + i * 2, 3, "%02X", digest[i]);
    return String(buf).substring(0, hexLen);
}

String getBadgeId() {
    return hashedMacHex("VANGUARD-BADGEID", 6);
}

String getBadgePassword() {
    return hashedMacHex("VANGUARD-BADGEPW", 8); // different salt than getBadgeId()
}

String getLinkId() {
    uint8_t mac[6] = {0}; // esp_read_mac() gives normal octet order, no endianness trap
    esp_read_mac(mac, ESP_MAC_BT);
    char buf[7];
    snprintf(buf, sizeof(buf), "%02X%02X%02X", mac[3], mac[4], mac[5]);
    return String(buf);
}

} // namespace badge_id
