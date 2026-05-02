/* SPDX-License-Identifier: Apache-2.0 */
/* ZephyrHome CoAP server — exposes component resources over CoAP. */
#ifndef ZH_COAP_SERVER_H_
#define ZH_COAP_SERVER_H_

/* Start the CoAP server thread. Call after Thread network is up. */
int zh_coap_server_start(void);

/* Send a CoAP multicast announcement (device discovery beacon). */
int zh_coap_announce(void);

#endif /* ZH_COAP_SERVER_H_ */
