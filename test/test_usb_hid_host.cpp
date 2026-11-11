// Exercise the actual USB driver against an asynchronous SDK model. The model
// rejects double submission and freeing an in-flight transfer, as ESP-IDF does.
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <set>
#include <vector>
#include <algorithm>
#include "hid_descriptor.h"

using esp_err_t=int;
constexpr int ESP_OK=0,ESP_ERR_NOT_FINISHED=1,USB_CLASS_HID=3;
constexpr int USB_TRANSFER_STATUS_COMPLETED=0,USB_TRANSFER_STATUS_ERROR=1,USB_TRANSFER_STATUS_CANCELED=2;
constexpr int USB_HOST_CLIENT_EVENT_NEW_DEV=1,USB_HOST_CLIENT_EVENT_DEV_GONE=2;
constexpr int HID_ITF_PROTOCOL_KEYBOARD=1,HID_SUBCLASS_BOOT=1;
constexpr int USB_BM_ATTRIBUTES_XFERTYPE_MASK=3,USB_BM_ATTRIBUTES_XFER_INT=3,USB_B_ENDPOINT_ADDRESS_EP_DIR_MASK=128;
constexpr int USB_INTERFACE_DESC=4,USB_ENDPOINT_DESC=5,USB_HID_DESC_TYPE=0x21,USB_HID_REPORT_DESC_TYPE=0x22;
constexpr int HID_REPORT_DESC_MAX_BYTES=512,ESP_INTR_FLAG_LEVEL1=1;
#define USB_EP_DESC_GET_EP_NUM(ep) ((ep)->bEndpointAddress&15)
using usb_device_handle_t=void*;
using usb_host_client_handle_t=void*;
struct usb_transfer_t {
 uint8_t *data_buffer;int num_bytes=0,actual_num_bytes=0,status=0;
 usb_device_handle_t device_handle=nullptr;uint8_t bEndpointAddress=0;
 void (*callback)(usb_transfer_t*)=nullptr;void *context=nullptr;
};
struct hid_keyboard_report_t {uint8_t modifier,reserved,keycode[6];};
#pragma pack(push,1)
struct usb_config_desc_t {union {struct {uint8_t bLength,bDescriptorType;uint16_t wTotalLength;uint8_t rest[5];};uint8_t val[9];};};
struct usb_intf_desc_t {uint8_t bLength,bDescriptorType,bInterfaceNumber,bAlternateSetting,bNumEndpoints,bInterfaceClass,bInterfaceSubClass,bInterfaceProtocol,iInterface;};
struct usb_ep_desc_t {uint8_t bLength,bDescriptorType,bEndpointAddress,bmAttributes;uint16_t wMaxPacketSize;uint8_t bInterval;};
#pragma pack(pop)
struct usb_device_info_t {int speed=1;};
struct usb_device_desc_t {uint16_t idVendor=0x1234,idProduct=0x5678;};
struct usb_host_client_event_msg_t {int event;struct {uint8_t address;} new_dev;struct {void *dev_hdl;} dev_gone;};
struct usb_host_config_t {bool skip_phy_setup;int intr_flags;};
struct usb_host_client_config_t {bool is_synchronous;int max_num_event_msg;struct {void (*client_event_callback)(const usb_host_client_event_msg_t*,void*);void *callback_arg;} async;};
static struct {template<class... A>void printf(const char*,A...){} void println(const char* = ""){} } Serial;
static uint32_t now_ms=1,errors=0,submits=0,keyboard_events=0,disconnect_releases=0;
static bool keyboard_enabled=true,usb_keyboard_connected=false,mouse_connected=false,right_click_ctrl_injected=false;
static uint8_t usb_mouse_buttons=0;
static std::set<usb_transfer_t*> allocated,pending,controls;
static std::deque<usb_transfer_t*> completions;
static usb_host_client_config_t registered;
static std::vector<uint8_t> configuration;
static int device_token;
static uint32_t millis(){return now_ms;}
static uint32_t micros(){return now_ms*1000;}
static void noteUsbTransferError(){++errors;}
static void noteUsbServiceGap(uint32_t){}
static void releaseAdbKey(int){}
static void releaseDisconnectedMouseButtons(){++disconnect_releases;usb_mouse_buttons=0;}
static esp_err_t usb_host_install(const usb_host_config_t*){return ESP_OK;}
static esp_err_t usb_host_client_register(const usb_host_client_config_t *c,void **out){assert(!c->is_synchronous);registered=*c;*out=(void*)1;return ESP_OK;}
static esp_err_t usb_host_transfer_alloc(size_t n,int,usb_transfer_t **out){*out=new usb_transfer_t;(*out)->data_buffer=new uint8_t[n]{};allocated.insert(*out);return ESP_OK;}
static esp_err_t usb_host_transfer_free(usb_transfer_t *t){assert(!pending.count(t)&&!controls.count(t));assert(allocated.erase(t)==1);delete[]t->data_buffer;delete t;return ESP_OK;}
static esp_err_t usb_host_transfer_submit(usb_transfer_t *t){assert(allocated.count(t));if(pending.count(t)){++errors;return ESP_ERR_NOT_FINISHED;}pending.insert(t);++submits;return ESP_OK;}
static esp_err_t usb_host_transfer_submit_control(void*,usb_transfer_t *t){assert(allocated.count(t));controls.insert(t);return ESP_OK;}
static esp_err_t usb_host_lib_handle_events(int timeout,uint32_t*){assert(timeout==0);return ESP_OK;}
static esp_err_t usb_host_client_handle_events(void*,int timeout){assert(timeout==0);while(!completions.empty()){auto*t=completions.front();completions.pop_front();pending.erase(t);controls.erase(t);t->callback(t);}return ESP_OK;}
static esp_err_t usb_host_device_open(void*,uint8_t,void **out){*out=&device_token;return ESP_OK;}
static esp_err_t usb_host_device_info(void*,usb_device_info_t*){return ESP_OK;}
static esp_err_t usb_host_get_device_descriptor(void*,const usb_device_desc_t **out){static usb_device_desc_t id;*out=&id;return ESP_OK;}
static esp_err_t usb_host_get_active_config_descriptor(void*,const usb_config_desc_t **out){*out=(const usb_config_desc_t*)configuration.data();return ESP_OK;}
static esp_err_t usb_host_device_close(void*,void*){return ESP_OK;}
static esp_err_t usb_host_interface_claim(void*,void*,int,int){return ESP_OK;}
static esp_err_t usb_host_interface_release(void*,void*,int){return ESP_OK;}
static esp_err_t usb_host_endpoint_halt(void*,int){return ESP_OK;}
static esp_err_t usb_host_endpoint_clear(void*,int){return ESP_OK;}
static esp_err_t usb_host_endpoint_flush(void *device,int ep){for(auto*t:pending)if(t->device_handle==device&&t->bEndpointAddress==ep){t->status=USB_TRANSFER_STATUS_CANCELED;t->actual_num_bytes=0;completions.push_back(t);}return ESP_OK;}

#include "input_usb_types.h"
class MultiDeviceUsbHost;
static MultiDeviceUsbHost *usbHost=nullptr;
struct Motion {int x,y,buttons;};
static std::vector<Motion> motions;
static unsigned ignored=0;
static void processKeyboardReport(hid_keyboard_report_t*,hid_keyboard_report_t*,bool = false){++keyboard_events;}
static void processMouseReport(const usb_transfer_t*t,EndpointInfo*ep){uint8_t b;int16_t x,y,w;if(ep->mouse.decode(t->data_buffer,t->actual_num_bytes,&b,&x,&y,&w))motions.push_back({x,y,b});else++ignored;}
#include "input_usb_host.inc"

static const std::vector<uint8_t> mouse_descriptor={
 0x05,9,0x19,1,0x29,5,0x15,0,0x25,1,0x75,1,0x95,5,0x81,2,
 0x75,3,0x95,1,0x81,3,
 0x05,1,0x09,0x30,0x09,0x31,0x16,1,0x80,0x26,0xff,0x7f,0x75,16,0x95,2,0x81,6,
 0x09,0x38,0x15,0x81,0x25,0x7f,0x75,8,0x95,1,0x81,6,
 0x75,8,0x95,1,0x81,3};
static const std::vector<uint8_t> vendor_descriptor={0x06,0,0xff,0x09,1,0x75,8,0x95,8,0x81,2};
static void configure(bool keyboard=false){
 configuration={9,2,0,0,(uint8_t)(keyboard?1:2),1,0,0x80,50};
 auto add=[&](uint8_t number,uint8_t ep,uint8_t protocol,size_t length){
  const uint8_t data[]={9,4,number,0,1,3,(uint8_t)(protocol?1:0),protocol,0,
                      9,0x21,0x11,1,0,1,0x22,(uint8_t)length,(uint8_t)(length>>8),
                      7,5,ep,3,8,0,1};
  configuration.insert(configuration.end(),data,data+sizeof(data));
 };
 if(keyboard)add(3,0x83,1,0);else {add(0,0x81,0,mouse_descriptor.size());add(1,0x82,0,vendor_descriptor.size());}
 configuration[2]=(uint8_t)configuration.size();configuration[3]=(uint8_t)(configuration.size()>>8);
}
static void attach(){usb_host_client_event_msg_t e={};e.event=USB_HOST_CLIENT_EVENT_NEW_DEV;e.new_dev.address=2;registered.async.client_event_callback(&e,registered.async.callback_arg);}
static void detach(){usb_host_client_event_msg_t e={};e.event=USB_HOST_CLIENT_EVENT_DEV_GONE;e.dev_gone.dev_hdl=&device_token;registered.async.client_event_callback(&e,registered.async.callback_arg);for(auto*t:controls){t->status=USB_TRANSFER_STATUS_CANCELED;completions.push_back(t);}}
static void descriptor(uint8_t interface_number,const std::vector<uint8_t>&bytes){
 auto it=std::find_if(controls.begin(),controls.end(),[&](auto*t){return t->data_buffer[4]==interface_number;});assert(it!=controls.end());auto*t=*it;controls.erase(it);
 std::memcpy(t->data_buffer+8,bytes.data(),bytes.size());t->status=USB_TRANSFER_STATUS_COMPLETED;t->actual_num_bytes=(int)bytes.size()+8;t->callback(t);
}
static usb_transfer_t *endpoint(uint8_t ep){for(auto*t:pending)if(t->bEndpointAddress==ep)return t;assert(false);return nullptr;}
static void packet(uint8_t ep,std::initializer_list<uint8_t> bytes,int status=USB_TRANSFER_STATUS_COMPLETED){auto*t=endpoint(ep);pending.erase(t);std::copy(bytes.begin(),bytes.end(),t->data_buffer);t->actual_num_bytes=(int)bytes.size();t->status=status;t->callback(t);}

int main(){
 MultiDeviceUsbHost host;usbHost=&host;host.begin();configure();attach();host.task();
 assert(pending.size()==2&&controls.size()==2);
 packet(0x81,{2,4,0,0xfb,0xff,0,0});assert(motions.empty()&&ignored==1);
 descriptor(0,mouse_descriptor);descriptor(1,vendor_descriptor);
 assert(host.devices[0].endpoint_info[1].mouse.layout.valid);
 assert(!host.devices[0].endpoint_info[2].mouse.layout.valid);
 assert(host.hasAnyMouse());
 packet(0x81,{2,4,0,0xfb,0xff,0,0});
 assert(motions.size()==1&&motions.back().x==4&&motions.back().y==-5&&motions.back().buttons==2);
 packet(0x82,{0xff,0xff,0x7f,0xff,0x7f,0xff,0x7f,0});assert(motions.size()==1);
 unsigned calls=submits;
 for(int i=0;i<100;++i){++now_ms;host.task();}
 assert(submits==calls&&errors==0); // Pending URBs are never resubmitted.
 for(int i=0;i<300;++i){packet(0x81,{0,1,0,0xff,0xff,0,0});++now_ms;host.task();}
 assert(motions.size()==301&&pending.size()==2);
 packet(0x81,{},USB_TRANSFER_STATUS_ERROR);assert(errors==1);
 host.task();assert(!host.devices[0].pending[0]);now_ms+=10;host.task();assert(host.devices[0].pending[0]);
 detach();assert(!allocated.empty());host.task();assert(allocated.empty()&&!host.devices[0].active);
 // Detach while descriptors are outstanding, then reuse the same device slot.
 configure();attach();host.task();detach();host.task();assert(allocated.empty());
 configure(true);attach();host.task();host.setKeyboardLEDs(2);
 assert(controls.size()==1&&(*controls.begin())->data_buffer[4]==3);
 auto*t=*controls.begin();controls.erase(t);t->status=USB_TRANSFER_STATUS_COMPLETED;t->callback(t);
 assert(host.devices[0].control_pending==0);
 packet(0x83,{0,0,4,0,0,0,0,0});assert(keyboard_events>0);
 detach();host.task();assert(allocated.empty());
 puts("PASS: continuous USB receive, interface isolation, recovery, deferred teardown and control lifetimes");
}
