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
#include "cyhal.h"
#include "cybsp.h"
#include "cy_retarget_io.h"
#include "GUI.h"
#include "LCDConf.h"
#include "FreeRTOS.h"
#include "task.h"
#include "mtb_e2271cs021.h"
#include "images.h"

cyhal_spi_t spi; 
const mtb_e2271cs021_pins_t pins =
{
    .spi_mosi  = CYBSP_D11,	// CY8CKIT_028_EPD_PIN_DISPLAY_SPI_MOSI
    .spi_miso  = CYBSP_D12, // CY8CKIT_028_EPD_PIN_DISPLAY_SPI_MISO
    .spi_sclk  = CYBSP_D13, // CY8CKIT_028_EPD_PIN_DISPLAY_SPI_SCLK
    .spi_cs    = CYBSP_D10, // CY8CKIT_028_EPD_PIN_DISPLAY_CS
    .reset     = CYBSP_D2,  // CY8CKIT_028_EPD_PIN_DISPLAY_RST
    .busy      = CYBSP_D3,  // CY8CKIT_028_EPD_PIN_DISPLAY_BUSY
    .discharge = CYBSP_D5,  // CY8CKIT_028_EPD_PIN_DISPLAY_DISCHARGE
    .enable    = CYBSP_D4,  // CY8CKIT_028_EPD_PIN_DISPLAY_EN
    .border    = CYBSP_D6,  // CY8CKIT_028_EPD_PIN_DISPLAY_BORDER
    .io_enable = CYBSP_D7   // CY8CKIT_028_EPD_PIN_DISPLAY_IOEN
};

/* Buffer to the previous frame written on the display */
uint8_t previous_frame[PV_EINK_IMAGE_SIZE] = {0};

/* Pointer to the new frame that need to be written */
uint8_t *current_frame;


/*******************************************************************************
* Macros
*******************************************************************************/
#define AMBIENT_TEMPERATURE_C               (20)
#define SPI_BAUD_RATE_HZ                    (20000000)

/*******************************************************************************
* Forward declaration
*******************************************************************************/
void show_main_screen(void);


void show_main_screen(void)
{
    /* Set font size, background color and text mode */
    GUI_SetFont(GUI_FONT_16B_1);
    GUI_SetBkColor(GUI_WHITE);
    GUI_SetColor(GUI_BLACK);
    GUI_SetTextMode(GUI_TM_NORMAL);
    /* Clear the display */
    GUI_Clear();

    /* Horizontal lines */
    GUI_SetPenSize(2);
    GUI_DrawLine(0, 65, 262, 64);

    GUI_SetFont(GUI_FONT_24B_1);
    GUI_DispStringAt("MACHINE LEARNING", 32, 0);
    GUI_DispStringAt("WITH", 102, 20);
    GUI_DispStringAt("PSOC6 BLUETOOTH", 32, 40);

    GUI_SetFont(GUI_FONT_20_1);
    GUI_SetTextStyle(GUI_TS_NORMAL);
    GUI_DispCharAt(187, 8, 86);
    GUI_DispStringAt("BLE name: Edge Impulse App", 20, 98);

    GUI_DrawBitmap(&bmei_logo, 0, 138);
    GUI_DrawBitmap(&bmifx_logo, 192, 138);
}

void eink_screen_onetime_set(void)
{
    cy_rslt_t result;
    
    /* Initialize SPI and EINK display */
    result = cyhal_spi_init(&spi, CYBSP_D11, CYBSP_D12, CYBSP_D13,
                            NC, NULL, 8, CYHAL_SPI_MODE_00_MSB, false);
    if (CY_RSLT_SUCCESS != result)
    {
        printf("Spi initialization failed!\r\n");
        CY_ASSERT(0);
    }

	result = cyhal_spi_set_frequency(&spi, SPI_BAUD_RATE_HZ);
	if (CY_RSLT_SUCCESS != result)
	{
        printf("Spi set frequency failed!\r\n");
        CY_ASSERT(0);
	}

	result = mtb_e2271cs021_init(&pins, &spi);
	if (CY_RSLT_SUCCESS != result)
    {
        printf("eink display init failed!\r\n");
        CY_ASSERT(0);
    }

	/* Set ambient temperature, in degree C, in order to perform temperature
	 * compensation of E-INK parameters */
	mtb_e2271cs021_set_temp_factor(AMBIENT_TEMPERATURE_C);

	current_frame = (uint8_t*)LCD_GetDisplayBuffer();

	/* Initialize EmWin driver*/
	GUI_Init();

	/* Show the instructions screen */
	show_main_screen();

	/* Update the display */
	mtb_e2271cs021_show_frame(previous_frame, current_frame,
							  MTB_E2271CS021_FULL_4STAGE, true);
}


