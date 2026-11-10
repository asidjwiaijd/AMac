#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

// Called only by the main/preboot task. Root-relative regular files only;
// active guest disks must never be modified through this interface.
class SdMaintenance {
public:
    explicit SdMaintenance(const char *root);
    ~SdMaintenance();
    void execute(char *command, char *reply, size_t capacity);
    bool busy() const { return file_ != nullptr; }
    void abort();
    bool recover();
private:
    const char *root_;
    FILE *file_ = nullptr;
    char target_[256] = {}, temp_[280] = {}, backup_[280] = {};
    uint32_t size_ = 0, offset_ = 0, expected_ = 0, crc_ = 0;
    uint32_t verify_offset_ = 0, verify_crc_ = 0;
    bool verifying_ = false, verified_ = false;
};

uint32_t SdMaintenanceCrc(uint32_t crc, const void *data, size_t size);
