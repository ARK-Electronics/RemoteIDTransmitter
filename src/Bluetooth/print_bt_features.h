
#ifndef _PRINT_FEATURES_H_
#define _PRINT_FEATURES_H_

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

void print_bt_le_features(const uint8_t* data, int size);
void print_bt_hci_features(const uint8_t* data, int size);

#ifdef __cplusplus
}
#endif

#endif //_PRINT_FEATURES_H_
