// The test runner inserts the production init and scan functions below.
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
enum {WIFI_STA=1, WL_IDLE_STATUS=0, WL_CONNECTED=3, WL_DISCONNECTED=6,
      WIFI_SCAN_RUNNING=-1,WIFI_SCAN_FAILED=-2};
using wl_status_t=int;
static uint32_t now=100;
static uint32_t millis(){return now;}
static void delay(int n){now+=n;}
static unsigned prepare_calls=0;
static void BoardWifi_Prepare(){++prepare_calls;}
static struct {
 void println(const char*){}
 template<typename...T>void printf(const char*,T...){}
} Serial;
static struct {
 bool available=false;
 int modes=0,reconnects=0,disconnects=0,scans=0,deletes=0;
 int scan_result=WIFI_SCAN_RUNNING,link=WL_DISCONNECTED;
 bool mode(int){++modes;return available;}
 void setAutoReconnect(bool enabled){assert(!enabled);++reconnects;}
 void disconnect(bool=false){++disconnects;link=WL_DISCONNECTED;}
 int status(){return link;}
 void scanDelete(){++deletes;}
 int scanNetworks(bool async){assert(async&&available);++scans;return scan_result;}
} WiFi;
static bool wifi_initialized=false;
// INSERT_INIT
int main(){
 bool scanning=true,connecting=true;
 int selected=4,connect_target=4,first=2;
 uint32_t scan_started=0;
 std::vector<int> rows={1,2},wifi_networks={1,2};
 char wifi_notice[96]="old failure";
 auto paint=[](const char*){};
 // INSERT_SCAN
 startScan();
 assert(!wifi_initialized&&!scanning&&!connecting);
 assert(selected==-1&&connect_target==-1&&first==0&&rows.empty()&&wifi_networks.empty());
 assert(WiFi.modes==1&&prepare_calls==1);
 assert(WiFi.reconnects==0&&WiFi.disconnects==0&&WiFi.scans==0&&WiFi.deletes==0);
 assert(std::strstr(wifi_notice,"could not start"));
 // A retry can initialize successfully; the failure is not latched as ready.
 WiFi.available=true;startScan();
 assert(wifi_initialized&&scanning&&!wifi_notice[0]&&scan_started==now);
 assert(WiFi.modes==2&&prepare_calls==2&&WiFi.reconnects==1&&WiFi.disconnects==1&&WiFi.scans==1);
 // Cancelling association before scanning does not reinstall the driver.
 WiFi.link=WL_CONNECTED;startScan();
 assert(scanning&&WiFi.modes==2&&WiFi.disconnects==2&&WiFi.scans==2);
 // An immediate scan failure stays retryable, with a distinct visible reason.
 WiFi.scan_result=WIFI_SCAN_FAILED;startScan();
 assert(!scanning&&std::strstr(wifi_notice,"Scan failed"));
 WiFi.scan_result=WIFI_SCAN_RUNNING;startScan();
 assert(scanning&&!wifi_notice[0]&&WiFi.modes==2);
 puts("PASS: failed startup blocks scan, retry recovers, failed scan remains retryable");
}
