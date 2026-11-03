# ESP-Hosted 2.12.3 transport

Unmodified `host/drivers/transport/transport_drv.c`, the 11 `common/` private
headers omitted from the Arduino SDK, and Apache 2.0 LICENSE from
Espressif commit `be55ec7141c0efa4796191f273512796542b573d` (version 2.12.3):
https://github.com/espressif/esp-hosted-mcu/tree/be55ec7141c0efa4796191f273512796542b573d

SHA256 of `transport_drv.c`:
`6880fc3fe2a8e9c8103f79eece9cda9b586a33a75b72029dc65b3021a5214d5c`.

The pinned Arduino SDK supplies ESP-Hosted 2.12.3 compiled for ESP32-C6.
`verify_host_config_for_slave()` asserts if the peer is a C5. Setting GPIOs
alone cannot fix this. `src/board/amac/esp_hosted_transport_amac.c` includes
this matching source with the C5 target selected, leaving the chipset check
intact. Its object supplies the transport symbols before the SDK archive is
searched, so the C6 transport object is not linked. A compile-time version
guard and post-link map check reject SDK drift or the wrong provider.

An audit of the 2.12.3 host source found the remaining SDIO target-specific
constants group C5 and C6 together in `sdio_reg.h`. Other SDK components remain
precompiled. This provides the common Wi-Fi API; C5 5 GHz/BLE features are not
qualified by this port. Slave 2.12.9 supplied by OSPTEK is newer than host
2.12.3; firmware compatibility still requires a hardware connection test.
