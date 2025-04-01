/***************************************************************************//**
 * @file
 * @brief Top level application functions
 *******************************************************************************
 * # License
 * <b>Copyright 2020 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement. This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/

#include <stdio.h>
#include "em_device.h"
#include "em_chip.h"
#include "em_cmu.h"
#include "em_emu.h"
#include "em_eusart.h"
#include "em_gpio.h"
#include "sl_board_control.h"

// BSP for board controller pin macros
#include "sl_board_control_config.h"

#define BSP_BCC_TXPORT  gpioPortA
#define BSP_BCC_TXPIN   8
#define BSP_BCC_RXPORT  gpioPortA
#define BSP_BCC_RXPIN   9

// Size of the buffer for received data
#define BUFLEN  80

// Receive data buffer
uint8_t buffer[BUFLEN];

// Current position in buffer
uint32_t inpos = 0;
uint32_t outpos = 0;

// True while receiving data (waiting for CR or BUFLEN characters)
bool receive = true;

/**************************************************************************//**
 * @brief
 *    CMU initialization
 *****************************************************************************/
void initCMU(void)
{
  // Enable clock to GPIO and EUSART1
  CMU_ClockEnable(cmuClock_GPIO, true);
  CMU_ClockEnable(cmuClock_EUSART1, true);
}

/**************************************************************************//**
 * @brief
 *    GPIO initialization
 *****************************************************************************/
void initGPIO(void)
{
  // Configure the EUSART TX pin to the board controller as an output
  GPIO_PinModeSet(BSP_BCC_TXPORT, BSP_BCC_TXPIN, gpioModePushPull, 1);

  // Configure the EUSART RX pin to the board controller as an input
  GPIO_PinModeSet(BSP_BCC_RXPORT, BSP_BCC_RXPIN, gpioModeInput, 0);

  /*
   * Configure the BCC_ENABLE pin as output and set high.  This enables
   * the virtual COM port (VCOM) connection to the board controller and
   * permits serial port traffic over the debug connection to the host
   * PC.
   *
   * To disable the VCOM connection and use the pins on the kit
   * expansion (EXP) header, comment out the following line.
   */
  GPIO_PinModeSet(SL_BOARD_ENABLE_VCOM_PORT, SL_BOARD_ENABLE_VCOM_PIN, gpioModePushPull, 1);
}

/**************************************************************************//**
 * @brief
 *    EUSART1 initialization
 *****************************************************************************/
void initEUSART1(void)
{
  // Default asynchronous initializer (115.2 Kbps, 8N1, no flow control)
  EUSART_UartInit_TypeDef init = EUSART_UART_INIT_DEFAULT_HF;
  init.baudrate = 9600;

  // Route EUSART1 TX and RX to the board controller TX and RX pins
  GPIO->EUSARTROUTE[1].TXROUTE = (BSP_BCC_TXPORT << _GPIO_EUSART_TXROUTE_PORT_SHIFT)
      | (BSP_BCC_TXPIN << _GPIO_EUSART_TXROUTE_PIN_SHIFT);
  GPIO->EUSARTROUTE[1].RXROUTE = (BSP_BCC_RXPORT << _GPIO_EUSART_RXROUTE_PORT_SHIFT)
      | (BSP_BCC_RXPIN << _GPIO_EUSART_RXROUTE_PIN_SHIFT);

  // Enable RX and TX signals now that they have been routed
  GPIO->EUSARTROUTE[1].ROUTEEN = GPIO_EUSART_ROUTEEN_RXPEN | GPIO_EUSART_ROUTEEN_TXPEN;

  // Configure and enable EUSART1 for high-frequency (EM0/1) operation
  EUSART_UartInitHf(EUSART1, &init);

  // Enable NVIC USART sources
  NVIC_ClearPendingIRQ(EUSART1_RX_IRQn);
  NVIC_EnableIRQ(EUSART1_RX_IRQn);
  NVIC_ClearPendingIRQ(EUSART1_TX_IRQn);
  NVIC_EnableIRQ(EUSART1_TX_IRQn);
}

/**************************************************************************//**
 * @brief
 *    The EUSART1 receive interrupt saves incoming characters.
 *****************************************************************************/
void EUSART1_RX_IRQHandler(void)
{
  // Get the character just received
  buffer[inpos] = EUSART1->RXDATA;

  // Exit loop on new line or buffer full
  if ((buffer[inpos] != '\r') && (inpos < BUFLEN))
    inpos++;
  else
    receive = false;   // Stop receiving on CR

  /*
   * The EUSART differs from the USART in that explicit clearing of
   * RX interrupt flags is required even after emptying the RX FIFO.
   */
  EUSART_IntClear(EUSART1, EUSART_IF_RXFL);
}

/**************************************************************************//**
 * @brief
 *    The EUSART1 transmit interrupt outputs characters.
 *****************************************************************************/
void EUSART1_TX_IRQHandler(void)
{
  // Send a previously received character
  if (outpos < inpos)
  {
    EUSART1->TXDATA = buffer[outpos++];

    /*
     * The EUSART differs from the USART in that the TX FIFO interrupt
     * flag must be explicitly cleared even after a write to the FIFO.
     */
    EUSART_IntClear(EUSART1, EUSART_IF_TXFL);
  }
  else
  /*
   * Need to disable the transmit FIFO level interrupt in this IRQ
   * handler when done or it will immediately trigger again upon exit
   * even though there is no data left to send.
   */
  {
    receive = true;   // Go back into receive when all is sent
    EUSART_IntDisable(EUSART1, EUSART_IEN_TXFL);
  }
}

/***************************************************************************//**
 * Initialize application.
 ******************************************************************************/
void app_init(void)
{
  // Initialize GPIO and EUSART
  initCMU();
  initGPIO();
  initEUSART1();
}

/***************************************************************************//**
 * App ticking function.
 ******************************************************************************/
void app_process_action(void)
{
  // Zero out buffer
  for (volatile uint32_t i = 0; i < BUFLEN; i++)
    buffer[i] = 0;

  // Enable receive data valid interrupt
  EUSART_IntEnable(EUSART1, EUSART_IEN_RXFL);

  // Wait in EM1 while receiving to reduce current draw
  while (receive)
    EMU_EnterEM1();

  // Disable RX FIFO Level Interrupt
  EUSART_IntDisable(EUSART1, EUSART_IEN_RXFL);

  // Enable TX FIFO Level Interrupt
  EUSART_IntEnable(EUSART1, EUSART_IEN_TXFL);

  // Wait in EM1 while transmitting to reduce current draw
  while (!receive)
    EMU_EnterEM1();

  // Reset buffer indices
  inpos = outpos = 0;
}
