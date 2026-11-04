/**
 * @file license_config.h
 * @brief Trial + license configuration for the minimal A12E firmware
 *
 * The PUBLIC key below matches tools/license_private.pem.
 * The private key is NOT in the firmware and must never be shared.
 *
 * If you regenerate the keypair, replace LICENSE_PUBLIC_KEY_PEM and rebuild
 * the firmware. Existing licenses will stop working.
 */

#ifndef LICENSE_CONFIG_H
#define LICENSE_CONFIG_H

// Product / license format version (also written into every key)
#define LICENSE_PRODUCT_CODE "A12E1"

// Trial length. The trial counts POWERED-ON time (the ESP32 has no battery
// backed clock), so 3 h = 3 hours of actual use.
//
// Customer trial build: 3 hours. For production use 24 h:
//   #define LICENSE_TRIAL_SECONDS (24UL * 60UL * 60UL)
#define LICENSE_TRIAL_SECONDS (3UL * 60UL * 60UL)

// NVS namespace for trial counter + activated key
#define LICENSE_NVS_NAMESPACE "a12e_lic"

// NVS write interval while the trial is running (flash wear vs. accuracy).
// A power cut can give back at most this many seconds of trial.
#define LICENSE_SAVE_INTERVAL_SEC 300UL

// Public key used to verify activation keys (ECDSA P-256, SHA-256).
// This is the PUBLIC half only - safe to ship inside the .bin.
#define LICENSE_PUBLIC_KEY_PEM                                              \
  "-----BEGIN PUBLIC KEY-----\n"                                            \
  "MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEQLjD3yQSp/le1mzjvmkcR0T86TDv\n"      \
  "gur1ZWd85tpkJfPFjVxK7AQt8wcGSBSy98YqBilAcOU/axP1ByisvxCv8Q==\n"          \
  "-----END PUBLIC KEY-----\n"

#endif // LICENSE_CONFIG_H
