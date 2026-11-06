/*
 * FatFs low-level disk I/O glue for the SPI SD card (drive 0, read-only).
 */
#include "ff.h"
#include "diskio.h"
#include "sd_spi.h"

DSTATUS disk_status(BYTE pdrv)
{
    if (pdrv != 0) {
        return STA_NOINIT;
    }
    return sd_spi_is_ready() ? STA_PROTECT : (STA_NOINIT | STA_PROTECT);
}

DSTATUS disk_initialize(BYTE pdrv)
{
    if (pdrv != 0) {
        return STA_NOINIT;
    }
    return sd_spi_init() ? STA_PROTECT : (STA_NOINIT | STA_PROTECT);
}

DRESULT disk_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count)
{
    if (pdrv != 0 || count == 0) {
        return RES_PARERR;
    }
    if (!sd_spi_is_ready()) {
        return RES_NOTRDY;
    }
    return sd_spi_read(buff, (uint32_t)sector, count) ? RES_OK : RES_ERROR;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
    if (pdrv != 0) {
        return RES_PARERR;
    }
    if (!sd_spi_is_ready()) {
        return RES_NOTRDY;
    }
    switch (cmd) {
    case CTRL_SYNC:
        return RES_OK;
    case GET_SECTOR_SIZE:
        *(WORD *)buff = 512;
        return RES_OK;
    case GET_SECTOR_COUNT:
        *(LBA_t *)buff = sd_spi_sector_count();
        return RES_OK;
    case GET_BLOCK_SIZE:
        *(DWORD *)buff = 1;
        return RES_OK;
    default:
        return RES_PARERR;
    }
}
