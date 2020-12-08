#include "os.h"
#include "cx.h"
#include "utils.h"
#include "menu.h"

#include <stdlib.h>

static cx_ecfp_public_key_t publicKey;
void get_public_key(uint32_t account_number, uint8_t* publicKeyArray) {
    cx_ecfp_private_key_t privateKey;

    cx_ecfp_init_public_key(CX_CURVE_Ed25519, NULL, 0, &publicKey);
    BEGIN_TRY {
        TRY {
            get_private_key(account_number, &privateKey);
            cx_ecfp_generate_pair(CX_CURVE_Ed25519, &publicKey, &privateKey, 1);
            io_seproxyhal_io_heartbeat();
        } FINALLY {
            os_memset(&privateKey, 0, sizeof(privateKey));
        }
    }
    END_TRY;

    for (int i = 0; i < 32; i++) {
        publicKeyArray[i] = publicKey.W[64 - i];
    }
    if ((publicKey.W[32] & 1) != 0) {
        publicKeyArray[31] |= 0x80;
    }
}

uint32_t readUint32BE(uint8_t *buffer) {
  return (buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | (buffer[3]);
}

static const uint32_t HARDENED_OFFSET = 0x80000000;

void get_private_key(uint32_t account_number, cx_ecfp_private_key_t *privateKey) {
    const uint32_t derivePath[BIP32_PATH] = {
            44 | HARDENED_OFFSET,
            396 | HARDENED_OFFSET,
            account_number | HARDENED_OFFSET,
            0 | HARDENED_OFFSET,
            0 | HARDENED_OFFSET
    };

    uint8_t privateKeyData[32];
    BEGIN_TRY {
        TRY {
            io_seproxyhal_io_heartbeat();
            os_perso_derive_node_bip32_seed_key(HDW_ED25519_SLIP10, CX_CURVE_Ed25519, derivePath, BIP32_PATH, privateKeyData, NULL, NULL, 0);
            cx_ecfp_init_private_key(CX_CURVE_Ed25519, privateKeyData, 32, privateKey);
            io_seproxyhal_io_heartbeat();
        } FINALLY {
            os_memset(privateKeyData, 0, sizeof(privateKeyData));
        }
    }
    END_TRY;
}

void send_response(uint8_t tx, bool approve) {
    G_io_apdu_buffer[tx++] = approve? 0x90 : 0x69;
    G_io_apdu_buffer[tx++] = approve? 0x00 : 0x85;
    reset_app_context();
    // Send back the response, do not restart the event loop
    io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, tx);
    // Display back the original UX
    ui_idle();
}

unsigned int ui_prepro(const bagl_element_t *element) {
    unsigned int display = 1;
    if (element->component.userid > 0) {
        display = (ux_step == element->component.userid - 1);
        if (display) {
            if (element->component.userid == 1) {
                UX_CALLBACK_SET_INTERVAL(2000);
            }
            else {
                UX_CALLBACK_SET_INTERVAL(MAX(3000, 1000 + bagl_label_roundtrip_duration_ms(element, 7)));
            }
        }
    }
    return display;
}

uint8_t leading_zeros(uint16_t value) {
    uint8_t lz = 0;
    uint16_t msb = 0x8000;
    for (uint8_t i = 0; i < 16; ++i) {
        if ((value << i) & msb) {
            break;
        }
        ++lz;
    }

    return lz;
}
