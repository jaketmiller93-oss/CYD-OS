#pragma once
#ifndef TEST_LINK_DISABLED
#define CONFIG_CYD_LINK_ENABLE 1
#endif
#ifndef TEST_LINK_UNCONFIRMED
#define CONFIG_CYD_LINK_WIRING_CONFIRMED 1
#endif
extern int fake_tx,fake_rx;
#define CONFIG_CYD_LINK_TX_GPIO fake_tx
#define CONFIG_CYD_LINK_RX_GPIO fake_rx
#define CONFIG_CYD_LINK_BAUD 115200
