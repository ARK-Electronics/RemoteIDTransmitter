
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdint.h>

#include "print_bt_features.h"

struct bitfield_data {
    uint64_t bit;
    const char *str;
};

static const struct bitfield_data features_le[] = {
        {  0, "LE Encryption"					},
        {  1, "Connection Parameter Request Procedure"		},
        {  2, "Extended Reject Indication"			},
        {  3, "Slave-initiated Features Exchange"		},
        {  4, "LE Ping"						},
        {  5, "LE Data Packet Length Extension"			},
        {  6, "LL Privacy"					},
        {  7, "Extended Scanner Filter Policies"		},
        {  8, "LE 2M PHY"					},
        {  9, "Stable Modulation Index - Transmitter"		},
        { 10, "Stable Modulation Index - Receiver"		},
        { 11, "LE Coded PHY"					},
        { 12, "LE Extended Advertising"				},
        { 13, "LE Periodic Advertising"				},
        { 14, "Channel Selection Algorithm #2"			},
        { 15, "LE Power Class 1"				},
        { 16, "Minimum Number of Used Channels Procedure"	},
        { 17, "Connection CTE Request"				},
        { 18, "Connection CTE Response"				},
        { 19, "Connectionless CTE Transmitter"			},
        { 20, "Connectionless CTE Receiver"			},
        { 21, "Antenna Switching During CTE Transmission (AoD)"	},
        { 22, "Antenna Switching During CTE Reception (AoA)"	},
        { 23, "Receiving Constant Tone Extensions"		},
        { 24, "Periodic Advertising Sync Transfer - Sender"	},
        { 25, "Periodic Advertising Sync Transfer - Recipient"	},
        { 26, "Sleep Clock Accuracy Updates"			},
        { 27, "Remote Public Key Validation"			},
        { 28, "Connected Isochronous Stream - Master"		},
        { 29, "Connected Isochronous Stream - Slave"		},
        { 30, "Isochronous Broadcaster"				},
        { 31, "Synchronized Receiver"				},
        { 32, "Isochronous Channels (Host Support)"		},
        { }
};

static const struct bitfield_data features_hci[] = {
    {  0, "3 slot packets"                           },
    {  1, "5 slot packets"                           },
    {  2, "Encryption"                               },
    {  3, "Slot offset"                              },
    {  4, "Timing accuracy"                          },
    {  5, "Role switch"                              },
    {  6, "Hold mode"                                },
    {  7, "Sniff mode"                               },
    {  8, "Previously used"                          },
    {  9, "Power control requests"                   },
    { 10, "Channel quality driven data rate (CQDDR)" },
    { 11, "SCO link"                                 },
    { 12, "HV2 packets"                              },
    { 13, "HV3 packets"                              },
    { 14, "μ-law log synchronous data"               },
    { 15, "A-law log synchronous data"               },
    { 16, "CVSD synchronous data"                    },
    { 17, "Paging parameter negotiation"             },
    { 18, "Power control"                            },
    { 19, "Transparent synchronous data"             },
    { 20, "Flow control lag (least significant bit)" },
    { 21, "Flow control lag (middle bit)"            },
    { 22, "Flow control lag (most significant bit)"  },
    { 23, "Broadcast Encryption"                     },
    { 24, "Reserved for future use"                  },
    { 25, "Enhanced Data Rate ACL 2 Mb/s mode"       },
    { 26, "Enhanced Data Rate ACL 3 Mb/s mode"       },
    { 27, "Enhanced inquiry scan"                    },
    { 28, "Interlaced inquiry scan"                  },
    { 29, "Interlaced page scan"                     },
    { 30, "RSSI with inquiry results"                },
    { 31, "Extended SCO link (EV3 packets)"          },
    { 32, "EV4 packets"                              },
    { 33, "EV5 packets"                              },
    { 34, "Reserved for future use"                  },
    { 35, "AFH capable slave"                        },
    { 36, "AFH classification slave"                 },
    { 37, "BR/EDR Not Supported"                     },
    { 38, "LE Supported (Controller)"                },
    { 39, "3-slot Enhanced Data Rate ACL packets"    },
    { 40, "5-slot Enhanced Data Rate ACL packets"    },
    { 41, "Sniff subrating"                          },
    { 42, "Pause encryption"                         },
    { 43, "AFH capable master"                       },
    { 44, "AFH classification master"                },
    { 45, "Enhanced Data Rate eSCO 2 Mb/s mode"      },
    { 46, "Enhanced Data Rate eSCO 3 Mb/s mode"      },
    { 47, "3-slot Enhanced Data Rate eSCO packets"   },
    { 48, "Extended Inquiry Response"                },
    { 49, "Simultaneous LE and BR/EDR to Same Device Capable (Controller)" },
    { 50, "Reserved for future use"                  },
    { 51, "Secure Simple Pairing (Controller Support)" },
    { 52, "Encapsulated PDU"                         },
    { 53, "Erroneous Data Reporting"                 },
    { 54, "Non-flushable Packet Boundary Flag"       },
    { 55, "Reserved for future use"                  },
    { 56, "HCI_Link_Supervision_Timeout_Changed event" },
    { 57, "Variable Inquiry TX Power Level"          },
    { 58, "Enhanced Power Control"                   },
    { 63, "Extended features"                   },
    { 64, "Secure Simple Pairing (Host Support)"                   },
    { 65, "LE Supported (Host)"                   },
    { 66, "Simultaneous LE and BR/EDR to Same Device Capable (Host)"                   },
    { 67, "Secure Connections (Host Support)"                   },
    { }
};


#define COLOR_OFF	"\x1B[0m"
#define COLOR_WHITE_BG	"\x1B[0;47;30m"
#define COLOR_UNKNOWN_FEATURE_BIT	COLOR_WHITE_BG

#define print_text(color, fmt, args...) \
		print_indent(8, COLOR_OFF, "", "", color, fmt, ## args)

#define print_field(fmt, args...) \
		print_indent(8, COLOR_OFF, "", "", COLOR_OFF, fmt, ## args)

static pid_t pager_pid = 0;

bool use_color(void)
{
    static int cached_use_color = -1;

    if (__builtin_expect(!!(cached_use_color < 0), 0))
        cached_use_color = isatty(STDOUT_FILENO) > 0 || pager_pid > 0;

    return cached_use_color;
}

#define print_indent(indent, color1, prefix, title, color2, fmt, args...) \
do { \
	printf("%*c%s%s%s%s" fmt "%s\n", (indent), ' ', \
		use_color() ? (color1) : "", prefix, title, \
		use_color() ? (color2) : "", ## args, \
		use_color() ? COLOR_OFF : ""); \
} while (0)

static inline uint64_t print_bitfield(int indent, uint64_t val, const struct bitfield_data *table)
{
    uint64_t mask = val;
    for (int i = 0; table[i].str; i++) {
        if (val & (((uint64_t) 1) << table[i].bit)) {
            print_field("%*c%s", indent, ' ', table[i].str);
            mask &= ~(((uint64_t) 1) << table[i].bit);
        }
    }
    return mask;
}

void print_bt_le_features(const uint8_t* data, int size)
{
    uint64_t mask, features = 0;
    char str[41];

    for (int i = 0; i < size; i++) {
        sprintf(str + (i * 5), " 0x%2.2x", data[i]);
        features |= ((uint64_t) data[i]) << (i * 8);
    }
    print_field("Features:%s", str);

    mask = print_bitfield(1, features, features_le);
    if (mask) {
        print_text(COLOR_UNKNOWN_FEATURE_BIT, "  Unknown features (0x%16.16" PRIx64 ")", mask);
    }

    fflush(stdout);
}

void print_bt_hci_features(const uint8_t* data, int size)
{
    uint64_t mask, features = 0;
    char str[41];

    for (int i = 0; i < size; i++) {
        sprintf(str + (i * 5), " 0x%2.2x", data[i]);
        features |= ((uint64_t) data[i]) << (i * 8);
    }
    print_field("Features:%s", str);

    mask = print_bitfield(1, features, features_hci);
    if (mask) {
        print_text(COLOR_UNKNOWN_FEATURE_BIT, "  Unknown features (0x%16.16" PRIx64 ")", mask);
    }

    fflush(stdout);
}