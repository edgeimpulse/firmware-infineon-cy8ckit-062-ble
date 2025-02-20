/* The Clear BSD License
 *
 * Copyright (c) 2025 EdgeImpulse Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted (subject to the limitations in the disclaimer
 * below) provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 *   * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 *
 *   * Neither the name of the copyright holder nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED BY
 * THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef EI_FLASH_MEMORY_H
#define EI_FLASH_MEMORY_H

#include "firmware-sdk/ei_device_memory.h"

extern "C" {
	#include "cy_pdl.h"
	#include "cyhal.h"
	#include "cybsp.h"
	#include <cycfg_qspi_memslot.h>
	#include <cy_serial_flash_qspi.h>
};

#define QSPI_MEM_SLOT_NUM       (0u)		 /* QSPI slot to use */
#define QSPI_BUS_FREQUENCY_HZ   (50000000lu) /* 50 Mhz */
/*
  Flash Related Parameter Define
*/
#define FLASH_ERASE_TIME    600 // Typical time is 520ms + 15% buffer
#define FLASH_SIZE          0x4000000   // 64 MB
#define FLASH_SECTOR_SIZE   0x40000     // 256K Sector size
#define FLASH_PAGE_SIZE     0x0200      // 512 Byte Page size
#define FLASH_BLOCK_NUM     (FLASH_SIZE / SECTOR_SIZE)

class EiFlashMemory : public EiDeviceMemory {
protected:
    uint32_t read_data(uint8_t *data, uint32_t address, uint32_t num_bytes);
    uint32_t write_data(const uint8_t *data, uint32_t address, uint32_t num_bytes);
    uint32_t erase_data(uint32_t address, uint32_t num_bytes);

public:
    EiFlashMemory(uint32_t config_size);
};

#endif /* EI_FLASH_MEMORY_H */
