#pragma once
//
// The board's own https certificate.
//
// Phones only let a page use the microphone when it came over https. There is no certificate
// authority for a board on a home network, so the board makes its own: an ECDSA P-256 key and a
// self-signed certificate for <host>.local and its current IP address. It is made once, on the
// first boot, and kept in flash (NVS), so every board has its own key and none is in the code.
// If the board's IP changes, it makes a new one (the old one would not match).
//
// What phones check, and why each field is here:
//   - subjectAltName: browsers ignore the common name and look only here (name and IP).
//   - extendedKeyUsage serverAuth and at most 825 days: iPhones refuse a TLS certificate without them.
//   - P-256 ECDSA: fast to make on the ESP32 (well under a second) and accepted everywhere.

#include <Arduino.h>
#include <Preferences.h>
#include <esp_random.h>
#include <mbedtls/pk.h>
#include <mbedtls/ecp.h>
#include <mbedtls/oid.h>
#include <mbedtls/x509_crt.h>

namespace cert {

static String pem, key;                  // kept for as long as the https server runs

static int rng(void *, unsigned char *b, size_t n) {
  esp_fill_random(b, n);
  return 0;
}

static bool make(const char *host, IPAddress ip) {
  mbedtls_pk_context pk;
  mbedtls_x509write_cert crt;
  mbedtls_pk_init(&pk);
  mbedtls_x509write_crt_init(&crt);
  const size_t SZ = 2048;
  unsigned char *buf = (unsigned char *)malloc(SZ);
  int r = buf ? 0 : -1;

  if (!r) r = mbedtls_pk_setup(&pk, mbedtls_pk_info_from_type(MBEDTLS_PK_ECKEY));
  if (!r) r = mbedtls_ecp_gen_key(MBEDTLS_ECP_DP_SECP256R1, mbedtls_pk_ec(pk), rng, nullptr);

  char name[80];
  snprintf(name, sizeof name, "CN=%s.local,O=devkit-diary", host);
  char dns[64];
  snprintf(dns, sizeof dns, "%s.local", host);
  uint8_t ipb[4] = {ip[0], ip[1], ip[2], ip[3]};
  uint8_t serial[16];
  esp_fill_random(serial, sizeof serial);
  serial[0] = (serial[0] & 0x7f) | 0x01;   // positive and not zero

  mbedtls_x509_san_list sanIp = {}, sanDns = {};
  sanDns.node.type = MBEDTLS_X509_SAN_DNS_NAME;
  sanDns.node.san.unstructured_name.p = (unsigned char *)dns;
  sanDns.node.san.unstructured_name.len = strlen(dns);
  sanDns.next = &sanIp;
  sanIp.node.type = MBEDTLS_X509_SAN_IP_ADDRESS;
  sanIp.node.san.unstructured_name.p = ipb;
  sanIp.node.san.unstructured_name.len = 4;

  mbedtls_asn1_sequence eku = {};
  eku.buf.tag = MBEDTLS_ASN1_OID;
  eku.buf.p = (unsigned char *)MBEDTLS_OID_SERVER_AUTH;
  eku.buf.len = MBEDTLS_OID_SIZE(MBEDTLS_OID_SERVER_AUTH);

  if (!r) {
    mbedtls_x509write_crt_set_version(&crt, MBEDTLS_X509_CRT_VERSION_3);
    mbedtls_x509write_crt_set_md_alg(&crt, MBEDTLS_MD_SHA256);
    mbedtls_x509write_crt_set_subject_key(&crt, &pk);
    mbedtls_x509write_crt_set_issuer_key(&crt, &pk);
    r = mbedtls_x509write_crt_set_subject_name(&crt, name);
  }
  if (!r) r = mbedtls_x509write_crt_set_issuer_name(&crt, name);
  if (!r) r = mbedtls_x509write_crt_set_serial_raw(&crt, serial, sizeof serial);
  // The board has no clock at first boot, so the dates are fixed: two years, under the 825-day
  // limit iPhones apply.
  if (!r) r = mbedtls_x509write_crt_set_validity(&crt, "20261101000000", "20281030000000");
  if (!r) r = mbedtls_x509write_crt_set_basic_constraints(&crt, 0, -1);
  if (!r) r = mbedtls_x509write_crt_set_key_usage(&crt, MBEDTLS_X509_KU_DIGITAL_SIGNATURE);
  if (!r) r = mbedtls_x509write_crt_set_ext_key_usage(&crt, &eku);
  if (!r) r = mbedtls_x509write_crt_set_subject_alternative_name(&crt, &sanDns);
  if (!r) r = mbedtls_x509write_crt_pem(&crt, buf, SZ, rng, nullptr);
  if (!r) pem = (const char *)buf;
  if (!r) r = mbedtls_pk_write_key_pem(&pk, buf, SZ);
  if (!r) key = (const char *)buf;

  if (r) Serial.printf("cert: could not make one (mbedtls -0x%04x)\n", (unsigned)-r);
  if (buf) { memset(buf, 0, SZ); free(buf); }
  mbedtls_x509write_crt_free(&crt);
  mbedtls_pk_free(&pk);
  return r == 0;
}

// Loads the saved certificate, or makes and saves a new one. True when pem and key are ready.
inline bool ready(const char *host, IPAddress ip) {
  Preferences p;
  p.begin("cert", true);
  pem = p.getString("crt", "");
  key = p.getString("key", "");
  const String forIp = p.getString("ip", ""), forHost = p.getString("host", "");
  p.end();
  // Every chapter shares this store, so the certificate must be for this name as well as this IP.
  if (pem.length() && key.length() && forIp == ip.toString() && forHost == host) {
    Serial.println("cert: using the saved certificate");
    return true;
  }
  Serial.println("cert: making this board's certificate...");
  const uint32_t t = millis();
  if (!make(host, ip)) return false;
  Serial.printf("cert: made in %lu ms\n", (unsigned long)(millis() - t));
  p.begin("cert", false);
  p.putString("crt", pem);
  p.putString("key", key);
  p.putString("ip", ip.toString());
  p.putString("host", host);
  p.end();
  return true;
}

}  // namespace cert
