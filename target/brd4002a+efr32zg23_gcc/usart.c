/*
 *  TOPPERS/ASP Kernel
 *      Toyohashi Open Platform for Embedded Real-Time Systems/
 *      Advanced Standard Profile Kernel
 * 
 *  Copyright (C) 2007,2011,2013,2015 by Embedded and Real-Time Systems Laboratory
 *              Graduate School of Information Science, Nagoya Univ., JAPAN
 * 
 *  上記著作権者は，以下の(1)～(4)の条件を満たす場合に限り，本ソフトウェ
 *  ア（本ソフトウェアを改変したものを含む．以下同じ）を使用・複製・改
 *  変・再配布（以下，利用と呼ぶ）することを無償で許諾する．
 *  (1) 本ソフトウェアをソースコードの形で利用する場合には，上記の著作
 *      権表示，この利用条件および下記の無保証規定が，そのままの形でソー
 *      スコード中に含まれていること．
 *  (2) 本ソフトウェアを，ライブラリ形式など，他のソフトウェア開発に使
 *      用できる形で再配布する場合には，再配布に伴うドキュメント（利用
 *      者マニュアルなど）に，上記の著作権表示，この利用条件および下記
 *      の無保証規定を掲載すること．
 *  (3) 本ソフトウェアを，機器に組み込むなど，他のソフトウェア開発に使
 *      用できない形で再配布する場合には，次のいずれかの条件を満たすこ
 *      と．
 *    (a) 再配布に伴うドキュメント（利用者マニュアルなど）に，上記の著
 *        作権表示，この利用条件および下記の無保証規定を掲載すること．
 *    (b) 再配布の形態を，別に定める方法によって，TOPPERSプロジェクトに
 *        報告すること．
 *  (4) 本ソフトウェアの利用により直接的または間接的に生じるいかなる損
 *      害からも，上記著作権者およびTOPPERSプロジェクトを免責すること．
 *      また，本ソフトウェアのユーザまたはエンドユーザからのいかなる理
 *      由に基づく請求からも，上記著作権者およびTOPPERSプロジェクトを
 *      免責すること．
 * 
 *  本ソフトウェアは，無保証で提供されているものである．上記著作権者お
 *  よびTOPPERSプロジェクトは，本ソフトウェアに関して，特定の使用目的
 *  に対する適合性も含めて，いかなる保証も行わない．また，本ソフトウェ
 *  アの利用により直接的または間接的に生じたいかなる損害に関しても，そ
 *  の責任を負わない．
 * 
 */

/*
 * シリアルドライバ（EFR32xG23用）
 */

#include <kernel.h>
#include <sil.h>
#include "usart.h"
#include "target_syssvc.h"
#include "target_serial.h"
#include "em_eusart.h"
#include "em_gpio.h"


/*
 *  シリアルI/Oポート初期化ブロックの定義
 */
typedef struct sio_port_initialization_block {
  EUSART_TypeDef*   p_eusart;
  int               index;
  GPIO_Port_TypeDef tx_port;
  unsigned int      tx_pin;
  GPIO_Port_TypeDef rx_port;
  unsigned int      rx_pin;
} SIOPINIB;

/*
 *  シリアルポートの管理ブロック
 */
struct sio_port_control_block {
  const SIOPINIB* p_siopinib;
  intptr_t        exinf;
  bool_t          is_initialized; /* デバイス初期化済みフラグ */
};

/*
 * シリアルI/Oポート管理ブロックエリア
 */
SIOPCB siopcb_table[TNUM_PORT];

static const SIOPINIB siopinib_table[TNUM_PORT] = {
    {
        EUSART0,
        0,
        gpioPortA,
        8,
        gpioPortA,
        9
    },
    {
        EUSART1,
        1,
        gpioPortA,
        8,
        gpioPortA,
        9
    }
};

/*
 *  シリアルI/OポートIDから管理ブロックを取り出すためのマクロ
 */
#define INDEX_SIOP(siopid)   ((uint_t)((siopid) - 1))
#define get_siopcb(siopid)   (&(siopcb_table[INDEX_SIOP(siopid)]))
#define get_siopinib(siopid) (&(siopinib_table[INDEX_SIOP(siopid)]))


Inline bool_t
sio_putready(SIOPCB* p_siopcb)
{
  uint32_t sts;
  sts = EUSART_StatusGet(p_siopcb->p_siopinib->p_eusart);
	return (EUSART_STATUS_TXFL & sts);
}

Inline bool_t
sio_getready(SIOPCB* p_siopcb)
{
  uint32_t sts;
  sts = EUSART_StatusGet(p_siopcb->p_siopinib->p_eusart);
  return (EUSART_STATUS_RXFL & sts);
}

/*
 *  SIO初期化
 */
void
sio_initialize(intptr_t exinf)
{
	int i;

	for (i = 0; i < TNUM_PORT; i++) {
		siopcb_table[i].p_siopinib = &(siopinib_table[i]);
		siopcb_table[i].exinf = 0;
    //siopcb_table[i].is_initialized = false;
	}
}

/*
 *  カーネル起動時のバナー出力用の初期化
 */
void
sio_uart_init(ID siopid, uint32_t bitrate)
{
  SIOPCB*         p_siopcb = get_siopcb(siopid);
  const SIOPINIB* p_siopinib = get_siopinib(siopid);
  /*  この時点では、p_siopcb->p_siopinibは初期化されていない  */

  /*  二重初期化の防止  */
  p_siopcb->is_initialized = true;

  // Configure the EUSART TX pin to the board controller as an output
  GPIO_PinModeSet(p_siopinib->tx_port, p_siopinib->tx_pin, gpioModePushPull, 1);

  // Configure the EUSART RX pin to the board controller as an input
  GPIO_PinModeSet(p_siopinib->rx_port, p_siopinib->rx_pin, gpioModeInput, 0);

  // Default asynchronous initializer (115.2 Kbps, 8N1, no flow control)
  EUSART_UartInit_TypeDef init = EUSART_UART_INIT_DEFAULT_HF;
  init.baudrate = bitrate;

  // Route EUSART1 TX and RX to the board controller TX and RX pins
  GPIO->EUSARTROUTE[p_siopinib->index].TXROUTE = (p_siopinib->tx_port << _GPIO_EUSART_TXROUTE_PORT_SHIFT)
      | (p_siopinib->tx_pin << _GPIO_EUSART_TXROUTE_PIN_SHIFT);
  GPIO->EUSARTROUTE[p_siopinib->index].RXROUTE = (p_siopinib->rx_port << _GPIO_EUSART_RXROUTE_PORT_SHIFT)
      | (p_siopinib->rx_pin << _GPIO_EUSART_RXROUTE_PIN_SHIFT);

  // Enable RX and TX signals now that they have been routed
  GPIO->EUSARTROUTE[p_siopinib->index].ROUTEEN = GPIO_EUSART_ROUTEEN_RXPEN | GPIO_EUSART_ROUTEEN_TXPEN;

  // Configure and enable EUSART1 for high-frequency (EM0/1) operation
  EUSART_UartInitHf(p_siopinib->p_eusart, &init);
}

/*
 *  シリアルオープン
 */
SIOPCB*
sio_opn_por(ID siopid, intptr_t exinf)
{
  SIOPCB*         p_siopcb = get_siopcb(siopid);

	if (siopid > TNUM_PORT) {
		return NULL;
	}

	p_siopcb->exinf = exinf;

  /*
   *  ハードウェアの初期化
   *
   *  既に初期化している場合は, 二重に初期化しない.
   */
  if(!(p_siopcb->is_initialized)){
      sio_uart_init(siopid, BPS_SETTING);
  }

  ena_int(INTNO_SIO_TX);
  ena_int(INTNO_SIO_RX);

	return (p_siopcb);
}

/*
 *  シリアルクローズ
 */
void
sio_cls_por(SIOPCB* p_siopcb)
{
	(void)p_siopcb;
}

/*
 *  割込みサービスルーチン
 */
void
sio_tx_isr(intptr_t exinf)
{
  SIOPCB*         p_siopcb = get_siopcb((int)exinf);
	if (sio_putready(p_siopcb)) {
		sio_irdy_snd(p_siopcb->exinf);
	}
	EUSART_IntClear(p_siopcb->p_siopinib->p_eusart, EUSART_IF_TXFL);
}

void
sio_rx_isr(intptr_t exinf)
{
  SIOPCB*         p_siopcb = get_siopcb((int)exinf);
  if (sio_getready(p_siopcb)) {
    sio_irdy_rcv(p_siopcb->exinf);
  }
  EUSART_IntClear(p_siopcb->p_siopinib->p_eusart, EUSART_IF_RXFL);
}

/*
 *  1文字送信
 */
bool_t
sio_snd_chr(SIOPCB* p_siopcb, char c)
{
	if (sio_putready(p_siopcb)) {
		EUSART_Tx(p_siopcb->p_siopinib->p_eusart, (uint8_t)c);
		return true;
	}
	return false;
}

/*
 *  1文字受信
 */
int_t
sio_rcv_chr(SIOPCB *p_siopcb)
{
	int_t c = -1;
	if (sio_getready(p_siopcb)) {
		c = (int_t)EUSART_Rx(p_siopcb->p_siopinib->p_eusart);
	}
	return c;
}

/*
 *  コールバックの許可
 */
void
sio_ena_cbr(SIOPCB *p_siopcb, uint_t cbrtn)
{
  switch (cbrtn) {
  case SIO_RDY_SND:
    EUSART_IntEnable(p_siopcb->p_siopinib->p_eusart, EUSART_IEN_TXFL);
    break;
  case SIO_RDY_RCV:
    EUSART_IntEnable(p_siopcb->p_siopinib->p_eusart, EUSART_IEN_RXFL);
    break;
  default:
    break;
  }
}

/* 
 *  コールバックの禁止
 */
void
sio_dis_cbr(SIOPCB *p_siopcb, uint_t cbrtn)
{
  switch (cbrtn) {
  case SIO_RDY_SND:
    EUSART_IntDisable(p_siopcb->p_siopinib->p_eusart, EUSART_IEN_TXFL);
    break;
  case SIO_RDY_RCV:
    EUSART_IntDisable(p_siopcb->p_siopinib->p_eusart, EUSART_IEN_RXFL);
    break;
  default:
    break;
  }
}

/*
 *  1文字出力（ポーリングでの出力）
 */
void
sio_pol_snd_chr(char c, ID siopid)
{
  const SIOPINIB* p_siopinib = get_siopinib(siopid);
	char cr = '\r';
  if (c == '\n') {
      EUSART_Tx(p_siopinib->p_eusart, (uint8_t)cr);
  }
  EUSART_Tx(p_siopinib->p_eusart, (uint8_t)c);
}
