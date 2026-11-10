#include "board_config.h"
#include "board_sd.h"
#include "board_sd_probe.h"
#include <Arduino.h>
#include <SD_MMC.h>
#include "driver/sdmmc_host.h"

static bool s_mounted;
extern "C" bool BoardSD_Init(void)
{
    if (s_mounted) return true;
    pinMode(AMAC_SD_DETECT_GPIO, INPUT_PULLUP);
    // Card power is fixed VCC_3V3 in the netlist, not an adjustable SD LDO.
    if (!SD_MMC.setPowerChannel(-1)) return false;
    if (!SD_MMC.setPins(AMAC_SD_CLK_GPIO, AMAC_SD_CMD_GPIO, AMAC_SD_D0_GPIO,
                        AMAC_SD_D1_GPIO, AMAC_SD_D2_GPIO, AMAC_SD_D3_GPIO)) return false;
    // Reads at 20 MHz succeeded on the assembled PCB, but writes timed out.
    // Start conservatively until the board's signal integrity is qualified.
    Serial.printf("[SD] detect=%d, slot=0, width=1, clock=4000 kHz\n",
                  digitalRead(AMAC_SD_DETECT_GPIO));
    if (!SD_MMC.begin(BOARD_SD_MOUNT_POINT, true, false, 4000, 8)) {
        SD_MMC.end();
        if (!SD_MMC.begin(BOARD_SD_MOUNT_POINT, true, false, SDMMC_FREQ_PROBING, 8)) {
            Serial.println("[SD] Mount failed; check FAT32 card and connector. No formatting performed.");
            return false;
        }
    }
    s_mounted = true;
    return true;
}
extern "C" uint64_t BoardSD_CardSizeMB(void)
{
    return s_mounted ? SD_MMC.cardSize() / (1024ULL * 1024ULL) : 0;
}
extern "C" BoardSdFsType BoardSD_ProbeFilesystem(void)
{
    // A raw probe must not reset the SDMMC host used by the C5 on slot 1.
    return s_mounted ? BOARD_SD_FS_FAT : BOARD_SD_FS_UNKNOWN;
}
