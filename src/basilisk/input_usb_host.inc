// Included after the input dispatch functions; exercised by native USB tests.
class MultiDeviceUsbHost {
public:
    usb_host_client_handle_t clientHandle;
    UsbDeviceSlot devices[MAX_USB_DEVICES];
    uint32_t eventFlags;
    uint32_t generation = 0;
    uint32_t last_service_us = 0;
    struct ControlContext { uint8_t slot, interface_number; uint32_t generation; };

    MultiDeviceUsbHost() {
        clientHandle = NULL;
        eventFlags = 0;
        for (int i = 0; i < MAX_USB_DEVICES; i++) {
            memset(&devices[i], 0, sizeof(UsbDeviceSlot));
        }
    }

    void begin() {
        const usb_host_config_t host_config = {
            .skip_phy_setup = false,
#if defined(BOARD_AMAC_P4)
            .root_port_unpowered = true,
#endif
            .intr_flags = ESP_INTR_FLAG_LEVEL1,
        };
        esp_err_t err = usb_host_install(&host_config);
        if (err != ESP_OK) {
            Serial.printf("[USB] usb_host_install() err=0x%x\n", err);
            return;
        }
#if defined(BOARD_AMAC_P4)
        // IDF 5.5.4 has no transaction translator for a FS/LS HID behind an
        // HS hub. Keep the wired HS controller/PHY, but force the root hub
        // to enumerate at FS. HW(0) is HS on this P4; HW(1) is USB2/FS.
        usb_dwc_ll_hcfg_set_fsls_supp_only(USB_DWC_LL_GET_HW(0));
        err = usb_host_lib_set_root_port_power(true);
        if (err != ESP_OK) {
            Serial.printf("[USB] Root-port power failed: 0x%x\n", err);
            return;
        }
        Serial.println("[USB] Hub compatibility: full/low-speed root enabled");
#endif

        const usb_host_client_config_t client_config = {
            .is_synchronous = false,
            .max_num_event_msg = 10,
            .async = {
                .client_event_callback = clientEventCallback,
                .callback_arg = this,
            }
        };
        err = usb_host_client_register(&client_config, &clientHandle);
        if (err != ESP_OK) {
            Serial.printf("[USB] usb_host_client_register() err=0x%x\n", err);
        }
    }

    void task() {
        if (!clientHandle) return;
        uint32_t stamp=micros();
        if (last_service_us) noteUsbServiceGap(stamp-last_service_us);
        last_service_us=stamp;
        esp_err_t err = usb_host_lib_handle_events(0, &eventFlags);
        (void)err;

        err = usb_host_client_handle_events(clientHandle, 0);
        (void)err;

        unsigned long now = millis();
        for (int d = 0; d < MAX_USB_DEVICES; d++) {
            UsbDeviceSlot *dev = &devices[d];
            if (dev->disconnecting) { finishDisconnect(d); continue; }
            if (!dev->active || !dev->ready) continue;

            for (int t = 0; t < dev->transfer_count; t++) {
                if (!dev->transfers[t] || dev->pending[t]) continue;
                if (dev->retry_at[t] && (int32_t)(now-dev->retry_at[t])<0) continue;
                if (dev->recover_pipe[t]) {
                    uint8_t ep=dev->transfers[t]->bEndpointAddress;
                    usb_host_endpoint_halt(dev->handle,ep);
                    usb_host_endpoint_flush(dev->handle,ep);
                    if (usb_host_endpoint_clear(dev->handle,ep)!=ESP_OK) {
                        noteUsbTransferError();dev->retry_at[t]=millis()+10;continue;
                    }
                    dev->recover_pipe[t]=false;
                }
                arm(d,t);
            }
        }
    }

    bool hasAnyMouse() {
        for (auto &dev:devices) if (dev.active && !dev.disconnecting && dev.has_mouse) return true;
        return false;
    }

    int findFreeSlot() {
        for (int i = 0; i < MAX_USB_DEVICES; i++) {
            if (!devices[i].active) return i;
        }
        return -1;
    }

    int findDeviceByHandle(usb_device_handle_t handle) {
        for (int i = 0; i < MAX_USB_DEVICES; i++) {
            if (devices[i].active && devices[i].handle == handle) return i;
        }
        return -1;
    }

    int findKeyboardDevice() {
        for (int i = 0; i < MAX_USB_DEVICES; i++) {
            if (devices[i].active && !devices[i].disconnecting && devices[i].has_keyboard) return i;
        }
        return -1;
    }

    bool hasAnyKeyboard() {
        return findKeyboardDevice() >= 0;
    }

    void setKeyboardLEDs(uint8_t leds) {
        int dev_idx = findKeyboardDevice();
        if (dev_idx < 0) return;
        UsbDeviceSlot *dev = &devices[dev_idx];
        if (!dev->ready || dev->handle == NULL) return;

        usb_transfer_t *transfer;
        esp_err_t err = usb_host_transfer_alloc(8 + 1, 0, &transfer);
        if (err != ESP_OK) return;

        transfer->num_bytes = 8 + 1;
        transfer->data_buffer[0] = 0x21;  // bmRequestType (Host->Device, Class, Interface)
        transfer->data_buffer[1] = 0x09;  // bRequest (SET_REPORT)
        transfer->data_buffer[2] = 0x00;  // wValue low (Report ID 0)
        transfer->data_buffer[3] = 0x02;  // wValue high (Output Report)
        uint8_t keyboard_interface=0;
        for (auto &ep:dev->endpoint_info) if (ep.bInterfaceProtocol==HID_ITF_PROTOCOL_KEYBOARD)
            keyboard_interface=ep.interface_number;
        transfer->data_buffer[4] = keyboard_interface;
        transfer->data_buffer[5] = 0x00;  // wIndex high
        transfer->data_buffer[6] = 0x01;  // wLength low
        transfer->data_buffer[7] = 0x00;  // wLength high
        transfer->data_buffer[8] = leds;

        transfer->device_handle = dev->handle;
        transfer->bEndpointAddress = 0x00;
        auto *context=new ControlContext{(uint8_t)dev_idx,keyboard_interface,dev->generation};
        transfer->callback = controlDone;
        transfer->context = context;
        ++dev->control_pending;

        err = usb_host_transfer_submit_control(clientHandle, transfer);
        if (err != ESP_OK) {
            Serial.printf("[USB] LED control transfer failed: 0x%x\n", err);
            --dev->control_pending;delete context;usb_host_transfer_free(transfer);
        }
    }

    void releaseAllKeyboardClaims() {
        hid_keyboard_report_t empty = {};
        for (int i = 0; i < MAX_USB_DEVICES; ++i) {
            UsbDeviceSlot *dev = &devices[i];
            if (!dev->active || !dev->has_keyboard) continue;
            processKeyboardReport(&empty, &dev->last_kb_report, true);
            dev->last_kb_report = {};
        }
    }

private:
    void arm(int slot,int index) {
        auto &dev=devices[slot];
        if (!dev.active || dev.disconnecting || !dev.ready || dev.pending[index]) return;
        esp_err_t error=usb_host_transfer_submit(dev.transfers[index]);
        if (error==ESP_OK) { dev.pending[index]=true; dev.retry_at[index]=0; }
        else { noteUsbTransferError(); dev.retry_at[index]=millis()+10; }
    }

    static void controlDone(usb_transfer_t *transfer) {
        auto *ctx=(ControlContext*)transfer->context;
        if (usbHost && ctx && ctx->slot<MAX_USB_DEVICES) {
            auto &dev=usbHost->devices[ctx->slot];
            if (dev.generation==ctx->generation && dev.control_pending) --dev.control_pending;
        }
        delete ctx;
        usb_host_transfer_free(transfer);
    }

    void finishDisconnect(int slot) {
        auto &dev=devices[slot];
        if (dev.control_pending) return;
        for (int t=0;t<dev.transfer_count;++t) if (dev.pending[t]) return;
        for (int t=0;t<dev.transfer_count;++t) if (dev.transfers[t]) usb_host_transfer_free(dev.transfers[t]);
        for (int i=0;i<dev.interface_count;++i) usb_host_interface_release(clientHandle,dev.handle,dev.interfaces[i]);
        usb_host_device_close(clientHandle,dev.handle);
        memset(&dev,0,sizeof(dev));
    }
    static void clientEventCallback(const usb_host_client_event_msg_t *eventMsg, void *arg) {
        MultiDeviceUsbHost *host = (MultiDeviceUsbHost *)arg;

        switch (eventMsg->event) {
        case USB_HOST_CLIENT_EVENT_NEW_DEV:
            host->handleNewDevice(eventMsg->new_dev.address);
            break;
        case USB_HOST_CLIENT_EVENT_DEV_GONE:
            host->handleDeviceGone(eventMsg->dev_gone.dev_hdl);
            break;
        default:
            break;
        }
    }

    void handleNewDevice(uint8_t dev_addr) {
        int slot = findFreeSlot();
        if (slot < 0) {
            Serial.println("[USB] No free device slots");
            return;
        }

        UsbDeviceSlot *dev = &devices[slot];
        memset(dev, 0, sizeof(UsbDeviceSlot));

        esp_err_t err = usb_host_device_open(clientHandle, dev_addr, &dev->handle);
        if (err != ESP_OK) {
            Serial.printf("[USB] device_open(%d) err=0x%x\n", dev_addr, err);
            return;
        }

        dev->active = true;
        dev->generation=++generation;
        const usb_device_desc_t *identity=nullptr;
        if (usb_host_get_device_descriptor(dev->handle,&identity)==ESP_OK)
            Serial.printf("[USB] VID=%04X PID=%04X\n",identity->idVendor,identity->idProduct);

        usb_device_info_t dev_info;
        if (usb_host_device_info(dev->handle, &dev_info) == ESP_OK) {
            Serial.printf("[USB] Device %d connected (addr=%d, speed=%d)\n",
                          slot, dev_addr, dev_info.speed);
        }

        const usb_config_desc_t *config_desc;
        err = usb_host_get_active_config_descriptor(dev->handle, &config_desc);
        if (err != ESP_OK) {
            Serial.printf("[USB] get_config_desc err=0x%x\n", err);
            return;
        }

        enumerateDevice(slot, config_desc);

        if (dev->has_keyboard) {
            Serial.printf("[USB] Device %d: keyboard detected\n", slot);
            usb_keyboard_connected = true;
        }
        if (dev->has_mouse) {
            Serial.printf("[USB] Device %d: mouse detected\n", slot);
            mouse_connected = true;
        }
    }

    void handleDeviceGone(usb_device_handle_t dev_hdl) {
        int slot = findDeviceByHandle(dev_hdl);
        if (slot < 0) {
            usb_host_device_close(clientHandle, dev_hdl);
            return;
        }

        UsbDeviceSlot *dev = &devices[slot];
        bool was_keyboard = dev->has_keyboard;
        bool was_mouse = dev->has_mouse;

        Serial.printf("[USB] Device %d disconnected\n", slot);

        if (was_keyboard) {
            hid_keyboard_report_t empty = {};
            processKeyboardReport(&empty, &dev->last_kb_report, true);
        }

        dev->disconnecting=true;
        dev->ready=false;
        for (auto &ep:dev->endpoint_info) ep.mouse.buttons=0;
        for (int i = 0; i < dev->transfer_count; i++) {
            if (dev->transfers[i] != NULL) {
                usb_host_endpoint_halt(dev->handle, dev->transfers[i]->bEndpointAddress);
                usb_host_endpoint_flush(dev->handle, dev->transfers[i]->bEndpointAddress);
            }
        }

        // Only reset connection flags if no other device provides the capability
        if (was_keyboard) {
            usb_keyboard_connected = false;
            for (int i = 0; i < MAX_USB_DEVICES; i++) {
                if (devices[i].active && !devices[i].disconnecting && devices[i].has_keyboard) {
                    usb_keyboard_connected = true;
                    break;
                }
            }
        }
        if (was_mouse) {
            mouse_connected = false;
            for (int i = 0; i < MAX_USB_DEVICES; i++) {
                if (devices[i].active && !devices[i].disconnecting && devices[i].has_mouse) {
                    mouse_connected = true;
                    break;
                }
            }
            releaseDisconnectedMouseButtons();
        }
    }

    void enumerateDevice(int slot, const usb_config_desc_t *config_desc) {
        UsbDeviceSlot *dev = &devices[slot];
        const uint8_t *p = &config_desc->val[0];
        uint8_t bLength;

        uint8_t cur_intf_class = 0;
        uint8_t cur_intf_subclass = 0;
        uint8_t cur_intf_protocol = 0;
        uint8_t cur_intf_number = 0;
        bool cur_intf_claimed = false;
        bool cur_intf_is_mouse = false;

        /* If we find a HID class descriptor for the mouse interface,
         * remember its number and the report-descriptor length so we
         * can fetch it after enumeration completes. */
        uint8_t report_interfaces[MAX_INTERFACES_PER_DEVICE]={};
        uint16_t report_lengths[MAX_INTERFACES_PER_DEVICE]={};
        unsigned report_count=0;

        for (int i = 0; i < config_desc->wTotalLength; i += bLength, p += bLength) {
            bLength = *p;
            if (bLength < 2 || (i + bLength) > config_desc->wTotalLength) break;

            uint8_t bDescriptorType = *(p + 1);

            if (bDescriptorType == USB_INTERFACE_DESC) {
                if (bLength<9) break;
                const usb_intf_desc_t *intf = (const usb_intf_desc_t *)p;
                cur_intf_class = intf->bInterfaceClass;
                cur_intf_subclass = intf->bInterfaceSubClass;
                cur_intf_protocol = intf->bInterfaceProtocol;
                cur_intf_number = intf->bInterfaceNumber;
                cur_intf_claimed = false;
                cur_intf_is_mouse = false;

                if (cur_intf_class == USB_CLASS_HID) {
                    esp_err_t err = usb_host_interface_claim(
                        clientHandle, dev->handle,
                        intf->bInterfaceNumber, intf->bAlternateSetting);
                    if (err == ESP_OK) {
                        cur_intf_claimed = true;
                        if (dev->interface_count < MAX_INTERFACES_PER_DEVICE) {
                            dev->interfaces[dev->interface_count++] = intf->bInterfaceNumber;
                        }
                        if (cur_intf_protocol == HID_ITF_PROTOCOL_KEYBOARD) {
                            dev->has_keyboard = true;
                        } else {
                            cur_intf_is_mouse = true;
                        }
                    }
                }

            } else if (bDescriptorType == USB_HID_DESC_TYPE && cur_intf_claimed) {
                /* HID class descriptor: 9 bytes minimum.
                 *   bLength, bDescriptorType, bcdHID(2), bCountryCode,
                 *   bNumDescriptors, bDescriptorType_subord(=0x22 Report),
                 *   wDescriptorLength(2)
                 * We only consume the first subordinate descriptor entry,
                 * which is the standard layout for ~all mice. */
                if (bLength >= 9 && cur_intf_is_mouse && report_count<MAX_INTERFACES_PER_DEVICE) {
                    uint8_t  sub_type = p[6];
                    uint16_t sub_len  = (uint16_t)p[7] | ((uint16_t)p[8] << 8);
                    if (sub_type == USB_HID_REPORT_DESC_TYPE && sub_len > 0 &&
                        sub_len <= HID_REPORT_DESC_MAX_BYTES) {
                        report_interfaces[report_count]=cur_intf_number;
                        report_lengths[report_count++]=sub_len;
                    }
                }

            } else if (bDescriptorType == USB_ENDPOINT_DESC && cur_intf_claimed) {
                if (bLength<7) break;
                const usb_ep_desc_t *ep_desc = (const usb_ep_desc_t *)p;

                if ((ep_desc->bmAttributes & USB_BM_ATTRIBUTES_XFERTYPE_MASK)
                    != USB_BM_ATTRIBUTES_XFER_INT)
                    continue;
                if (!(ep_desc->bEndpointAddress & USB_B_ENDPOINT_ADDRESS_EP_DIR_MASK))
                    continue;

                uint8_t ep_num = USB_EP_DESC_GET_EP_NUM(ep_desc);
                if (!ep_num) continue;
                if (ep_num < MAX_ENDPOINTS) {
                    dev->endpoint_info[ep_num].bInterfaceClass = cur_intf_class;
                    dev->endpoint_info[ep_num].bInterfaceSubClass = cur_intf_subclass;
                    dev->endpoint_info[ep_num].bInterfaceProtocol = cur_intf_protocol;
                    dev->endpoint_info[ep_num].interface_number = cur_intf_number;
                }

                if (dev->transfer_count < MAX_TRANSFERS_PER_DEVICE) {
                    usb_transfer_t *xfer = NULL;
                    uint16_t packet_size=ep_desc->wMaxPacketSize&0x7ff;
                    if (!packet_size || packet_size>1024) continue;
                    esp_err_t err = usb_host_transfer_alloc(packet_size, 0, &xfer);
                    if (err == ESP_OK && xfer != NULL) {
                        xfer->device_handle = dev->handle;
                        xfer->bEndpointAddress = ep_desc->bEndpointAddress;
                        xfer->callback = transferCallback;
                        xfer->context = (void *)(uintptr_t)slot;
                        xfer->num_bytes = packet_size;
                        dev->transfers[dev->transfer_count++] = xfer;
                        dev->ready = true;
                    }
                }
            }
        }

        // Each HID interface owns its own descriptor and endpoint bindings.
        for (unsigned r=0;r<report_count;++r) kickHidReportRead(slot,report_interfaces[r],report_lengths[r]);
    }

    /*
     * Issue GET_DESCRIPTOR(REPORT) on the device's default control
     * endpoint. The transfer buffer has 8 bytes of setup packet
     * followed by the requested wLength of data; on completion the
     * callback parses bytes [8 .. 8+wLength) as the report descriptor.
     */
    void kickHidReportRead(int slot, uint8_t intf_num, uint16_t length) {
        UsbDeviceSlot *dev = &devices[slot];
        if (dev->handle == NULL) return;

        usb_transfer_t *xfer = NULL;
        esp_err_t err = usb_host_transfer_alloc(8 + length, 0, &xfer);
        if (err != ESP_OK || xfer == NULL) {
            Serial.printf("[USB] HID descriptor alloc failed: 0x%x\n", err);
            return;
        }

        xfer->num_bytes = 8 + length;
        xfer->data_buffer[0] = 0x81;  /* bmRequestType: D->H, Standard, Interface */
        xfer->data_buffer[1] = 0x06;  /* GET_DESCRIPTOR */
        xfer->data_buffer[2] = 0x00;  /* wValue low: descriptor index 0 */
        xfer->data_buffer[3] = USB_HID_REPORT_DESC_TYPE;  /* wValue high: Report */
        xfer->data_buffer[4] = intf_num;
        xfer->data_buffer[5] = 0x00;
        xfer->data_buffer[6] = (uint8_t)(length & 0xFF);
        xfer->data_buffer[7] = (uint8_t)((length >> 8) & 0xFF);

        xfer->device_handle = dev->handle;
        xfer->bEndpointAddress = 0x00;  /* default control pipe */
        xfer->callback = hidReportDescCallback;
        /* Pack slot index into the context so the callback knows which
         * device this came from. */
        xfer->context = new ControlContext{(uint8_t)slot,intf_num,dev->generation};
        ++dev->control_pending;

        err = usb_host_transfer_submit_control(clientHandle, xfer);
        if (err != ESP_OK) {
            Serial.printf("[USB] HID descriptor submit failed: 0x%x\n", err);
            controlDone(xfer);
        }
    }

    static void hidReportDescCallback(usb_transfer_t *transfer) {
        auto *context=(ControlContext*)transfer->context;
        if (!usbHost || !context || context->slot>=MAX_USB_DEVICES) { controlDone(transfer); return; }
        uint8_t slot=context->slot;
        UsbDeviceSlot *dev = &usbHost->devices[slot];

        do {
            if (transfer->status != USB_TRANSFER_STATUS_COMPLETED) {
                Serial.printf("[USB] HID descriptor transfer status=%d\n",
                              transfer->status);
                break;
            }
            if (transfer->actual_num_bytes <= 8) {
                Serial.println("[USB] HID descriptor: no payload");
                break;
            }
            if (!dev->active || dev->disconnecting || context->generation!=dev->generation) {
                break;
            }

            const uint8_t *desc = transfer->data_buffer + 8;
            size_t desc_len = transfer->actual_num_bytes - 8;

            HidMouseLayout layout;
            bool ok = HidParseMouseDescriptor(desc, desc_len, &layout);
            for (auto &ep:dev->endpoint_info) if (ep.bInterfaceClass==USB_CLASS_HID && ep.interface_number==context->interface_number) {
                ep.mouse.layout=layout;
                ep.mouse.described=true;
            }
            if (ok) {
                dev->has_mouse=true;
                mouse_connected=true;
                Serial.printf("[HID] interface=%u descriptor=",context->interface_number);
                for (size_t d=0;d<desc_len;++d) Serial.printf("%02X",desc[d]);
                Serial.println();
                Serial.printf("[HID] parsed report desc: btns=%d x=%d%c y=%d%c "
                              "wheel=%d%c report_id=%u bytes=%u\n",
                              layout.button_count,
                              layout.x_size_bits, layout.x_signed ? 's' : 'u',
                              layout.y_size_bits, layout.y_signed ? 's' : 'u',
                              layout.has_wheel ? layout.wheel_size_bits : 0,
                              (layout.has_wheel && layout.wheel_signed) ? 's' : 'u',
                              layout.report_id,
                              layout.report_bytes);
            } else {
                Serial.printf("[HID] interface=%u has no supported relative mouse report; ignoring it\n",context->interface_number);
            }
        } while (0);

        controlDone(transfer);
    }

    static void transferCallback(usb_transfer_t *transfer) {
        if (!usbHost) return;

        uint8_t dev_idx = (uint8_t)(uintptr_t)transfer->context;
        if (dev_idx >= MAX_USB_DEVICES) return;

        UsbDeviceSlot *dev = &usbHost->devices[dev_idx];
        if (!dev->active) return;
        int index=-1;
        for (int t=0;t<dev->transfer_count;++t) if (dev->transfers[t]==transfer) index=t;
        if (index<0) return;
        dev->pending[index]=false;
        if (dev->disconnecting) return;
        if (transfer->status != USB_TRANSFER_STATUS_COMPLETED) {
            noteUsbTransferError();dev->retry_at[index]=millis()+10;dev->recover_pipe[index]=true;return;
        }
        if (transfer->actual_num_bytes == 0) { usbHost->arm(dev_idx,index);return; }

        uint8_t ep_num = transfer->bEndpointAddress & 0x0F;
        if (ep_num >= MAX_ENDPOINTS) return;

        EndpointInfo *ep_info = &dev->endpoint_info[ep_num];

        // Boot keyboard: HID class, Boot subclass, Keyboard protocol
        if (ep_info->bInterfaceClass == USB_CLASS_HID &&
            ep_info->bInterfaceSubClass == HID_SUBCLASS_BOOT &&
            ep_info->bInterfaceProtocol == HID_ITF_PROTOCOL_KEYBOARD &&
            transfer->actual_num_bytes >= 8) {

            hid_keyboard_report_t report;
            report.modifier = transfer->data_buffer[0];
            report.reserved = transfer->data_buffer[1];
            report.keycode[0] = transfer->data_buffer[2];
            report.keycode[1] = transfer->data_buffer[3];
            report.keycode[2] = transfer->data_buffer[4];
            report.keycode[3] = transfer->data_buffer[5];
            report.keycode[4] = transfer->data_buffer[6];
            report.keycode[5] = transfer->data_buffer[7];

            if (keyboard_enabled &&
                memcmp(&report, &dev->last_kb_report, sizeof(report)) != 0) {
                processKeyboardReport(&report, &dev->last_kb_report);
                memcpy(&dev->last_kb_report, &report, sizeof(report));
            }

        } else if (ep_info->bInterfaceClass == USB_CLASS_HID &&
                   ep_info->bInterfaceProtocol != HID_ITF_PROTOCOL_KEYBOARD) {
            processMouseReport(transfer, ep_info);
        }
        usbHost->arm(dev_idx,index);
    }
};
