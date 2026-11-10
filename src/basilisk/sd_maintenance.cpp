#include "sd_maintenance.h"
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef ESP_PLATFORM
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_task_wdt.h"
#endif

uint32_t SdMaintenanceCrc(uint32_t crc, const void *data, size_t size)
{
    crc = ~crc;
    const auto *bytes = static_cast<const uint8_t *>(data);
    while (size--) {
        crc ^= *bytes++;
        for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xedb88320U & (0U - (crc & 1U)));
    }
    return ~crc;
}
static int nibble(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    c = (char)tolower((unsigned char)c);
    return c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
}
static bool unhex(const char *text, uint8_t *out, size_t capacity, size_t *size)
{
    if (!text || strlen(text) % 2 || strlen(text) / 2 > capacity) return false;
    *size = strlen(text) / 2;
    for (size_t i = 0; i < *size; ++i) {
        int hi = nibble(text[2*i]), lo = nibble(text[2*i+1]);
        if (hi < 0 || lo < 0) return false;
        out[i] = (uint8_t)(hi * 16 + lo);
    }
    return true;
}
static void hex(const uint8_t *data, size_t size, char *out)
{
    static const char digits[] = "0123456789abcdef";
    for (size_t i = 0; i < size; ++i) { out[i*2] = digits[data[i] >> 4]; out[i*2+1] = digits[data[i] & 15]; }
    out[size*2] = 0;
}
static bool number(const char *text, uint32_t *value, int base = 10)
{
    if (!text || !*text || *text == '-' || *text == '+') return false;
    errno = 0;
    char *end;
    unsigned long long n = strtoull(text, &end, base);
    if (errno || *end || n > 0x7fffffffULL) return false;
    *value = (uint32_t)n;
    return true;
}
static bool checksum(const char *text, uint32_t *value)
{
    if (!text || strlen(text) != 8) return false;
    uint8_t bytes[4]; size_t size;
    if (!unhex(text, bytes, 4, &size)) return false;
    *value = (uint32_t)bytes[0]<<24 | (uint32_t)bytes[1]<<16 | (uint32_t)bytes[2]<<8 | bytes[3];
    return true;
}
static bool filename(const char *name)
{
    char lower[81];
    if (strlen(name) > 80) return false;
    for (size_t i=0; i<=strlen(name); ++i) lower[i]=(char)tolower((unsigned char)name[i]);
    if (!*name || strlen(name) > 80 || name[0] == '.' || strstr(name, "..") ||
        strstr(lower, ".upload")) return false;
    for (const unsigned char *p = (const unsigned char *)name; *p; ++p)
        if (*p < 32 || *p > 126 || strchr("/\\:*?\"<>|", *p)) return false;
    size_t n = strlen(name);
    return name[n-1] != ' ' && name[n-1] != '.';
}
static bool path(const char *root, const char *encoded, char *out, size_t capacity)
{
    uint8_t name[81] = {}; size_t size;
    if (!unhex(encoded, name, 80, &size) || memchr(name, 0, size) || !filename((char *)name)) return false;
    return snprintf(out, capacity, "%s/%s", root, name) < (int)capacity;
}
static bool exists(const char *path) { struct stat s; return stat(path, &s) == 0; }
static bool syncFile(FILE *f) { return fflush(f) == 0 && fsync(fileno(f)) == 0; }

SdMaintenance::SdMaintenance(const char *root) : root_(root) {}
SdMaintenance::~SdMaintenance() { abort(); }
void SdMaintenance::abort()
{
    if (file_) { fclose(file_); file_ = nullptr; }
    if (*temp_) remove(temp_);
    target_[0] = temp_[0] = backup_[0] = 0;
}
bool SdMaintenance::recover()
{
    char journal[280]; snprintf(journal, sizeof(journal), "%s/.amac-update", root_);
    FILE *f = fopen(journal, "r");
    if (!f) return errno == ENOENT;
    char name[84] = {};
    bool ok = fgets(name, sizeof(name), f) && filename(name);
    fclose(f);
    if (!ok) return false;
    char target[256], backup[280];
    snprintf(target, sizeof(target), "%s/%s", root_, name);
    snprintf(backup, sizeof(backup), "%s.previous", target);
    if (!exists(target) && exists(backup) && rename(backup, target)) return false;
    return remove(journal) == 0;
}
void SdMaintenance::execute(char *command, char *reply, size_t capacity)
{
    const char *error = "invalid_arguments";
    char *save = nullptr;
    char *verb = strtok_r(command, " ", &save);
    char *a = strtok_r(nullptr, " ", &save), *b = strtok_r(nullptr, " ", &save);
    char *c = strtok_r(nullptr, " ", &save);
    if (strtok_r(nullptr, " ", &save)) goto failed;
    if (!verb) goto failed;
    if (!strcmp(verb, "STATUS") && !a) {
        snprintf(reply, capacity, "OK SD STATUS %u %lu %lu", busy(), (unsigned long)offset_, (unsigned long)size_); return;
    }
    if (!strcmp(verb, "ABORT") && !a) { abort(); snprintf(reply, capacity, "OK SD ABORT"); return; }
    if (!strcmp(verb, "LIST") && a && !b) {
        uint32_t index; if (!number(a, &index)) goto failed;
        DIR *dir = opendir(root_); if (!dir) { error="open_failed"; goto failed; }
        struct dirent *entry; uint32_t count=0; char name[81]={};
        while ((entry = readdir(dir))) {
            if (!filename(entry->d_name)) continue;
            char full[256]; snprintf(full,sizeof(full),"%s/%s",root_,entry->d_name);
            struct stat s; if (stat(full,&s) || !S_ISREG(s.st_mode)) continue;
            if (count++ == index) snprintf(name,sizeof(name),"%s",entry->d_name);
        }
        closedir(dir); char encoded[163]; hex((uint8_t*)name,strlen(name),encoded);
        snprintf(reply,capacity,"OK SD ITEM %lu %s",(unsigned long)count,*name?encoded:"-"); return;
    }
    if (!strcmp(verb,"STAT") && a && !b) {
        char full[256]; if (!path(root_,a,full,sizeof(full))) goto failed;
        struct stat s;
        if (stat(full,&s) || !S_ISREG(s.st_mode)) { error="not_found"; goto failed; }
        snprintf(reply,capacity,"OK SD STAT %lu",(unsigned long)s.st_size); return;
    }
    if (!strcmp(verb,"READ") && a && b && c) {
        char full[256]; uint32_t offset,count;
        if (!path(root_,a,full,sizeof(full)) || !number(b,&offset) || !number(c,&count) || !count || count>256) goto failed;
        FILE *f=fopen(full,"rb"); if (!f) { error="open_failed"; goto failed; }
        uint8_t data[256]; size_t got=0;
        bool ok=fseek(f,(long)offset,SEEK_SET)==0;
        if (ok) got=fread(data,1,count,f);
        ok=ok && !ferror(f); fclose(f);
        if (!ok) { error="read_failed"; goto failed; }
        char encoded[513]; hex(data,got,encoded);
        snprintf(reply,capacity,"OK SD DATA %lu %s %08lX",(unsigned long)offset,got?encoded:"-",(unsigned long)SdMaintenanceCrc(0,data,got)); return;
    }
    if (!strcmp(verb,"BEGIN") && a && b && c) {
        if (busy()) { error="transfer_active"; goto failed; }
        if (!path(root_,a,target_,sizeof(target_)) || !number(b,&size_) || !checksum(c,&expected_)) goto failed;
        char lower[81]; const char *name=target_+strlen(root_)+1;
        for (size_t i=0;i<=strlen(name);++i) lower[i]=(char)tolower((unsigned char)name[i]);
        if (strstr(lower,".previous")) goto failed;
        snprintf(temp_,sizeof(temp_),"%s.upload",target_);
        snprintf(backup_,sizeof(backup_),"%s.previous",target_);
        file_=fopen(temp_,"wb"); if (!file_) { error="write_open_failed"; goto failed; }
        verifying_=verified_=false; verify_offset_=verify_crc_=0;
        crc_=offset_=0; snprintf(reply,capacity,"OK SD BEGIN"); return;
    }
    if (!strcmp(verb,"WRITE") && a && b && c) {
        uint32_t offset,crc; uint8_t data[256]; size_t size;
        if (!busy()) { error="no_transfer"; goto failed; }
        if (verifying_) { error="already_verifying"; goto failed; }
        if (!number(a,&offset) || !unhex(b,data,sizeof(data),&size) || !size || !checksum(c,&crc)) goto failed;
        if (offset!=offset_ || size>size_-offset_) { error="wrong_offset"; goto failed; }
        if (SdMaintenanceCrc(0,data,size)!=crc) { error="crc_mismatch"; goto failed; }
        if (fwrite(data,1,size,file_)!=size) { error="write_failed"; goto failed; }
        crc_=SdMaintenanceCrc(crc_,data,size); offset_+=(uint32_t)size;
        snprintf(reply,capacity,"OK SD WRITE %lu",(unsigned long)offset_); return;
    }
    if (!strcmp(verb,"VERIFY") && !a) {
        if (!busy()) { error="no_transfer"; goto failed; }
        if (offset_!=size_ || crc_!=expected_) { error="incomplete_or_bad_crc"; goto failed; }
        if (!verifying_) {
            bool ok=syncFile(file_); fclose(file_); file_=nullptr;
            if (!ok) { error="sync_failed"; goto failed; }
            file_=fopen(temp_,"rb"); if (!file_) { error="verify_open_failed"; goto failed; }
            verifying_=true;
            struct stat s;
            if (fstat(fileno(file_),&s) || (uint32_t)s.st_size!=size_) { error="readback_failed"; goto failed; }
        }
        uint32_t budget=65536;
#ifdef ESP_PLATFORM
        const bool watched = esp_task_wdt_status(nullptr) == ESP_OK;
#endif
        while (budget && verify_offset_<size_) {
            size_t wanted=size_-verify_offset_;
            if (wanted>sizeof(verify_buffer_)) wanted=sizeof(verify_buffer_);
            size_t got=fread(verify_buffer_,1,wanted,file_);
            if (got!=wanted) { error="readback_failed"; goto failed; }
            verify_crc_=SdMaintenanceCrc(verify_crc_,verify_buffer_,got);
            verify_offset_+=(uint32_t)got; budget-=(uint32_t)got;
#ifdef ESP_PLATFORM
            if (watched) esp_task_wdt_reset();
            vTaskDelay(1);
#endif
        }
        if (verify_offset_==size_) {
            if (verify_crc_!=expected_) { error="readback_failed"; goto failed; }
            verified_=true;
        }
        snprintf(reply,capacity,"OK SD VERIFY %lu %08lX",(unsigned long)verify_offset_,(unsigned long)verify_crc_); return;
    }
    if (!strcmp(verb,"COMMIT") && !a) {
        if (!busy()) { error="no_transfer"; goto failed; }
        if (offset_!=size_ || crc_!=expected_) { error="incomplete_or_bad_crc"; goto failed; }
        if (!verified_) { error="verify_required"; goto failed; }
        fclose(file_); file_=nullptr;
        char journal[280]; snprintf(journal,sizeof(journal),"%s/.amac-update",root_);
        FILE *f=fopen(journal,"wb");
        if (!f) { error="journal_failed"; goto failed; }
        const char *name=target_+strlen(root_)+1;
        bool ok=fwrite(name,1,strlen(name),f)==strlen(name) && syncFile(f); fclose(f);
        if (!ok) { error="journal_failed"; goto failed; }
        if (exists(target_)) {
            if (exists(backup_) && remove(backup_)) { error="backup_failed"; goto failed; }
            if (rename(target_,backup_)) { error="backup_failed"; goto failed; }
        }
        if (rename(temp_,target_)) { recover(); error="replace_failed"; goto failed; }
        remove(journal); temp_[0]=0;
        snprintf(reply,capacity,"OK SD COMMIT %lu %08lX",(unsigned long)size_,(unsigned long)verify_crc_); return;
    }
failed:
    snprintf(reply,capacity,"ERR SD %s errno=%d",error,errno);
}
