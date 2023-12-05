/*
 * Copyright (C) 2021, Soren Friis
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Open Drone ID Linux transmitter example.
 *
 * Maintainer: Soren Friis
 * friissoren2@gmail.com
 */

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/param.h>

#include <lib/bluetooth.h>
#include <lib/hci.h>
#include <lib/hci_lib.h>

#include "bluetooth.h"
#include "print_bt_features.h"

int device_descriptor = 0;

static int open_hci_device() {
    struct hci_filter flt; // Host Controller Interface filter

    int dev_id = hci_devid("hci0");
    if (dev_id < 0)
        dev_id = hci_get_route(NULL);

    int dd = hci_open_dev(dev_id);
    if (dd < 0) {
        perror("Device open failed");
        exit(EXIT_FAILURE);
    }

    hci_filter_clear(&flt);
    hci_filter_set_ptype(HCI_EVENT_PKT, &flt);
    hci_filter_all_events(&flt);

    if (setsockopt(dd, SOL_HCI, HCI_FILTER, &flt, sizeof(flt)) < 0) {
        perror("HCI filter setup failed");
        exit(EXIT_FAILURE);
    }
    return dd;
}

static void send_cmd(int dd, uint8_t ogf, uint16_t ocf, uint8_t *cmd_data, int length) {
    if (hci_send_cmd(dd, ogf, ocf, length, cmd_data) < 0)
        exit(EXIT_FAILURE);

    unsigned char buf[HCI_MAX_EVENT_SIZE] = { 0 }, *ptr;
    uint16_t opcode = htobs(cmd_opcode_pack(ogf, ocf));
    hci_event_hdr *hdr = (void *) (buf + 1);
    evt_cmd_complete *cc;
    ssize_t len;
    while ((len = read(dd, buf, sizeof(buf))) < 0) {
        if (errno == EAGAIN || errno == EINTR)
            continue;
        printf("While loop for reading event failed\n");
        return;
    }

    hdr = (void *) (buf + 1);
    ptr = buf + (1 + HCI_EVENT_HDR_SIZE);
    len -= (1 + HCI_EVENT_HDR_SIZE);

    switch (hdr->evt) {
        case EVT_CMD_COMPLETE:
            cc = (void *) ptr;

            if (cc->opcode != opcode)
                printf("Received event with invalid opcode 0x%X. Expected 0x%X\n", cc->opcode, opcode);

            ptr += EVT_CMD_COMPLETE_SIZE;
            len -= EVT_CMD_COMPLETE_SIZE;

            uint8_t rparam[10] = { 0 };
            memcpy(rparam, ptr, len);
            if (rparam[0] && ocf != 0x3C)
                printf("Command 0x%X returned error 0x%X\n", ocf, rparam[0]);
            if (ocf == OCF_LE_READ_LOCAL_SUPPORTED_FEATURES) {
                printf("Supported Low Energy Bluetooth features:\n");
                print_bt_le_features(&rparam[1]);
            }
            if (ocf == 0x36)
                printf("The transmit power is set to %d dBm\n", (unsigned char) rparam[1]);
            fflush(stdout);
            return;

        default:
            printf("Received unknown event: 0x%X\n", hdr->evt);
            return;
    }
}

static void generate_random_mac_address(uint8_t *mac) {
    if (!mac)
        return;
    srand(time(0)); // NOLINT(cert-msc51-cpp)
    for (int i = 0; i < 6; i++)
        mac[i] = rand() % 255; // NOLINT(cert-msc50-cpp)
        
    mac[0] |= 0xC0;  // set to Bluetooth Random Static Address, see https://www.novelbits.io/bluetooth-address-privacy-ble/ or section 1.3 of the Bluetooth specification (5.3)
}

/*
 * The functions below for sending the various HCI commands are based on the Bluetooth Core Specification 5.1:
 * https://www.bluetooth.com/specifications/bluetooth-core-specification/
 * Please see the descriptions in Vol 2, Part E, Chapter 7.8.
 * (In version 5.2, they appear to have been moved to Vol 4, Part E, Chapter 7.8).
 */

static void hci_reset(int dd) {
    uint8_t ogf = OGF_HOST_CTL; // Opcode Group Field. LE Controller Commands
    uint16_t ocf = OCF_RESET;
    send_cmd(dd, ogf, ocf, NULL, 0);
}

static void hci_le_read_local_supported_features(int dd) {
    uint8_t ogf = OGF_LE_CTL; // Opcode Group Field. LE Controller Commands
    uint16_t ocf = OCF_LE_READ_LOCAL_SUPPORTED_FEATURES;
    send_cmd(dd, ogf, ocf, NULL, 0);
}

static void hci_le_set_advertising_disable(int dd) {
    uint8_t ogf = OGF_LE_CTL; // Opcode Group Field. LE Controller Commands
    uint16_t ocf = OCF_LE_SET_ADVERTISE_ENABLE;
    uint8_t buf[] = { 0x00 }; // Enable: 0 = Advertising is disabled (default)

    send_cmd(dd, ogf, ocf, buf, sizeof(buf));
}

static void hci_le_set_advertising_set_random_address(int dd, uint8_t set, const uint8_t *mac) {
    if (!mac)
        return;
    uint8_t ogf = OGF_LE_CTL; // Opcode Group Field. LE Controller Commands
    uint16_t ocf = 0x35;      // Opcode Command Field: LE Set Advertising Set Random Address
    uint8_t buf[] = { 0x00,   // Advertising_Handle: Used to identify an advertising set
                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; // Advertising_Random_Address:
    buf[0] = set;
    for (int i = 0; i < 6; i++)
        buf[i + 1] = mac[i];
    send_cmd(dd, ogf, ocf, buf, sizeof(buf));
}

static void hci_le_set_extended_advertising_parameters(int dd, uint8_t set, int interval_ms, bool long_range) {
    uint8_t ogf = OGF_LE_CTL;     // Opcode Group Field. LE Controller Commands
    uint16_t ocf = 0x36;          // Opcode Command Field: LE Set Extended Advertising Parameters
    uint8_t buf[] = { 0x00,       // Advertising_Handle: Used to identify an advertising set
                      0x10, 0x00, // Advertising_Event_Properties: 0x0010 = Use legacy advertising PDUs + Non-connectable and non-scannable undirected
                      0x00, 0x08, 0x00, // Primary_Advertising_Interval_Min: N * 0.625 ms. 0x000800 = 1280 ms
                      0x00, 0x08, 0x00, // Primary_Advertising_Interval_Max: N * 0.625 ms. 0x000800 = 1280 ms
                      0x07,       // Primary_Advertising_Channel_Map: 7 = all three channels enabled
                      0x01,       // Own_Address_Type: 1 = Random Device Address
                      0x00,       // Peer_Address_Type: 0 = Public Device Address or Public Identity Address
                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Peer_Address
                      0x00,       // Advertising_Filter_Policy: 0 = Process scan and connection requests from all devices (i.e., the White List is not in use)
                      0x7F,       // Advertising_Tx_Power: 0x7F = Host has no preference
                      0x01,       // Primary_Advertising_PHY: 1 = Primary advertisement PHY is LE 1M
                      0x00,       // Secondary_Advertising_Max_Skip: 0 = AUX_ADV_IND shall be sent prior to the next advertising event
                      0x01,       // Secondary_Advertising_PHY: 1 = Secondary advertisement PHY is LE 1M
                      0x00,       // Advertising_SID: 0 = Value of the Advertising SID subfield in the ADI field of the PDU
                      0x00 };     // Scan_Request_Notification_Enable: 0 = Scan request notifications disabled
    buf[0] = set;

    interval_ms = MIN(MAX((1000 * interval_ms) / 625, 0x000020), 0xFFFFFF);
    buf[3] = buf[6] = interval_ms & 0xFF;
    buf[4] = buf[7] = (interval_ms >> 8) & 0xFF;
    buf[5] = buf[8] = (interval_ms >> 16) & 0xFF;

    if (long_range) {
        buf[1] = 0x00;  // Advertising_Event_Properties: 0x0000 = Non-connectable and non-scannable undirected
        buf[20] = 0x03; // Primary_Advertising_PHY: 3 = Primary advertisement PHY is LE Coded
        buf[22] = 0x03; // Secondary_Advertising_PHY: 3 = Secondary advertisement PHY is LE Coded
    }

    send_cmd(dd, ogf, ocf, buf, sizeof(buf));
}

// See hci_le_set_advertising_data for further details
static void hci_le_set_extended_advertising_data(int dd, uint8_t set,
                                                 const union ODID_Message_encoded *encoded,
                                                 uint8_t msg_counter) {
    uint8_t ogf = OGF_LE_CTL; // Opcode Group Field. LE Controller Commands
    uint16_t ocf = 0x37;      // Opcode Command Field: LE Set Extended Advertising Data
    uint8_t buf[] = { 0x00,   // Advertising_Handle: Used to identify an advertising set
                      0x03,   // Operation: 3 = Complete extended advertising data
                      0x01,   // Fragment_Preference: 1 = The Controller should not fragment or should minimize fragmentation of Host advertising data
                      0x1F,   // Advertising_Data_Length: The number of octets in the Advertising Data parameter
                      0x1E,   // The length of the following data field
                      0x16,   // 16 = GAP AD Type = "Service Data - 16-bit UUID"
                      0xFA, 0xFF, // 0xFFFA = ASTM International, ASTM Remote ID
                      0x0D,   // 0x0D = AD Application Code within the ASTM address space = Open Drone ID
                      0x00,   // xx = 8-bit message counter starting at 0x00 and wrapping around at 0xFF
                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };  // 25-byte Drone ID message data
    buf[0] = set;
    buf[9] = msg_counter;
    for (int i = 0; i < ODID_MESSAGE_SIZE; i++)
        buf[10 + i] = encoded->rawData[i];

    send_cmd(dd, ogf, ocf, buf, sizeof(buf));
}

static void hci_le_set_extended_advertising_disable(int dd) {
    uint8_t ogf = OGF_LE_CTL; // Opcode Group Field. LE Controller Commands
    uint16_t ocf = 0x39;      // Opcode Command Field: LE Set Extended Advertising Enable
    uint8_t buf[] = { 0x00,   // Enable: 0 = Advertising is disabled
                      0x00 }; // Number_of_Sets: 0 = Disable all advertising sets
    send_cmd(dd, ogf, ocf, buf, sizeof(buf));
}

// Max two advertising sets is supported by this function currently
static void hci_le_set_extended_advertising_enable(int dd, struct config_data *config) {
    uint8_t ogf = OGF_LE_CTL; // Opcode Group Field. LE Controller Commands
    uint16_t ocf = 0x39;      // Opcode Command Field: LE Set Extended Advertising Enable
    uint8_t buf[] = { 0x01,   // Enable: 1 = Advertising is enabled
                      0x01,   // Number_of_Sets: Number of advertising sets to enable or disable
                      0x00, 0x00,   // Advertising_Handle[i]:
                      0x00, 0x00,   // Duration[i]: 0 = No advertising duration. Advertising to continue until the Host disables it
                      0x00, 0x00 }; // Max_Extended_Advertising_Events[i]: 0 = No maximum number of advertising events

    if (config->use_bt4 && config->use_bt5) {
        buf[1] = 2;
        buf[2] = config->handle_bt4;
        buf[3] = config->handle_bt5;
    } else if (config->use_bt4) {
        buf[2] = config->handle_bt4;
    } else if (config->use_bt5) {
        buf[2] = config->handle_bt5;
    }
    send_cmd(dd, ogf, ocf, buf, sizeof(buf));
}

static void hci_le_remove_advertising_set(int dd, uint8_t set) {
    uint8_t ogf = OGF_LE_CTL; // Opcode Group Field. LE Controller Commands
    uint16_t ocf = 0x3C;      // Opcode Command Field: LE Remove Advertising Set
    uint8_t buf[] = { 0x00 }; // Advertising_Handle: Used to identify an advertising set
    buf[0] = set;
    send_cmd(dd, ogf, ocf, buf, sizeof(buf));
}

static void stop_transmit(struct config_data *config) {
    hci_le_set_advertising_disable(device_descriptor);
    hci_le_set_extended_advertising_disable(device_descriptor);
    hci_le_remove_advertising_set(device_descriptor, config->handle_bt4);
    hci_le_remove_advertising_set(device_descriptor, config->handle_bt5);
}

void init_bluetooth(struct config_data *config) {
    uint8_t mac[6] = { 0 };
    generate_random_mac_address(mac);

    device_descriptor = open_hci_device();
    hci_reset(device_descriptor);
    stop_transmit(config);

    hci_le_read_local_supported_features(device_descriptor);

    if (config->use_bt4) {
        hci_reset(device_descriptor);
        hci_le_set_extended_advertising_parameters(device_descriptor, config->handle_bt4, 300, false);
        hci_le_set_advertising_set_random_address(device_descriptor, config->handle_bt4, mac);
    }

    if (config->use_bt5) {
        hci_reset(device_descriptor);
        hci_le_set_extended_advertising_parameters(device_descriptor, config->handle_bt5, 950, true);
        hci_le_set_advertising_set_random_address(device_descriptor, config->handle_bt5, mac);
    }

    if (config->use_bt4)
        hci_le_set_extended_advertising_enable(device_descriptor, config);

    if (config->use_bt5)
        hci_le_set_extended_advertising_enable(device_descriptor, config);
}

void send_bluetooth_message_extended_api(const union ODID_Message_encoded *encoded, uint8_t msg_counter, struct config_data *config) {
    if (config->use_bt4)
        hci_le_set_extended_advertising_data(device_descriptor, config->handle_bt4, encoded, msg_counter);
    if (config->use_bt5)
        hci_le_set_extended_advertising_data(device_descriptor, config->handle_bt5, encoded, msg_counter);
}

// TODO: should we implement this?
void close_bluetooth(struct config_data *config) {
    stop_transmit(config);
    hci_close_dev(device_descriptor);
}
