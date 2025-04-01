/*
 *  TOPPERS/ASP Kernel
 *      Toyohashi Open Platform for Embedded Real-Time Systems/
 *      Advanced Standard Profile Kernel
 * 
 *  Copyright (C) 2000-2003 by Embedded and Real-Time Systems Laboratory
 *                              Toyohashi Univ. of Technology, JAPAN
 *  Copyright (C) 2005-2015 by Embedded and Real-Time Systems Laboratory
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
 * ターゲット依存モジュール（xG23B 868-915 MHz 14 dBm Radio Board用）
 */
#include "kernel_impl.h"
#include <sil.h>
#include "target_syssvc.h"
#include "target_serial.h"
#include "sl_system_init.h"
#include "em_cmu.h"
#include "em_gpio.h"
#include "em_eusart.h"
#include "sl_board_control_config.h"


// デバッガの動作のため .stack 領域を 0x20000000 より確保する必要がある
STK_T _kernel_istack[COUNT_STK_T(DEFAULT_ISTKSZ)] __attribute__ ((section (".stack")));

/*
 *  バーナ出力用のUARTの初期化
 */
static void usart_early_init(void);

/*
 *  起動時のハードウェア初期化処理
 */
#if 0
uint32_t g_istk;
void
hardware_init_hook(void) {
	/*
	 *  -fdata-sectionsを使用するとistkが削除され，
	 *  cfgのパス3のチェックがエラーとなるため，
	 *  削除されないようにする 
	 */
  g_istk = (uint32_t)istk;
}
#endif

/*
 * ターゲット依存部 初期化処理
 */
void
target_initialize(void)
{
  /* Secure app takes care of moving between the security states.
   * SL_TRUSTZONE_SECURE MACRO is for secure access.
   * SL_TRUSTZONE_NONSECURE MACRO is for non-secure access.
   * When both the MACROS are not defined, during start-up below code makes sure
   * that all the peripherals are accessed from non-secure address except SMU,
   * as SMU is used to configure the trustzone state of the system. */
#if !defined(SL_TRUSTZONE_SECURE) && !defined(SL_TRUSTZONE_NONSECURE) \
  && defined(__TZ_PRESENT)
  CMU->CLKEN1_SET = CMU_CLKEN1_SMU;

  // config SMU to Secure and other peripherals to Non-Secure.
  SMU->PPUSATD0_CLR = _SMU_PPUSATD0_MASK;
#if defined (SEMAILBOX_PRESENT)
  SMU->PPUSATD1_CLR = (_SMU_PPUSATD1_MASK & (~SMU_PPUSATD1_SMU & ~SMU_PPUSATD1_SEMAILBOX));
#else
  SMU->PPUSATD1_CLR = (_SMU_PPUSATD1_MASK & ~SMU_PPUSATD1_SMU);
#endif

  // SAU treats all accesses as non-secure
#if defined(__ARM_FEATURE_CMSE) && (__ARM_FEATURE_CMSE == 3U)
  SAU->CTRL = SAU_CTRL_ALLNS_Msk;
  __DSB();
  __ISB();
#else
  #error "The startup code requires access to the CMSE toolchain extension to set proper SAU settings."
#endif // __ARM_FEATURE_CMSE

  CMU_ClockEnable(cmuClock_SMU, true);
#endif //SL_TRUSTZONE_SECURE

	/*
	 *  HALによる初期化
	 *  HAL_Init() : stm32f4xx_hal.c の内容から必要な初期化のみ呼び出す．
	 */
  sl_system_init();

	/*
	 *  コア依存部の初期化
	 */
	core_initialize();

	/*
	 *  使用するペリフェラルにクロックを供給
	 */
  CMU_ClockEnable(cmuClock_GPIO, true);
  CMU_ClockEnable(cmuClock_EUSART1, true);

  /*
   *  使用する GPIO の設定
   */
  /*
   * Configure the BCC_ENABLE pin as output and set high.  This enables
   * the virtual COM port (VCOM) connection to the board controller and
   * permits serial port traffic over the debug connection to the host
   * PC.
   *
   * To disable the VCOM connection and use the pins on the kit
   * expansion (EXP) header, comment out the following line.
   */
  GPIO_PinModeSet(
      SL_BOARD_ENABLE_VCOM_PORT,
      SL_BOARD_ENABLE_VCOM_PIN,
      gpioModePushPull,
      1
  );

	/*
	 *  バーナー出力用のシリアル初期化
	 */
	usart_early_init();
}

/*
 * ターゲット依存部 終了処理
 */
void
target_exit(void)
{
	/* チップ依存部の終了処理 */
	core_terminate();
	while(1);
}

void
usart_early_init()
{
  sio_uart_init(LOGTASK_PORTID, BPS_SETTING);
}

/*
 * システムログの低レベル出力のための文字出力
 */
void
target_fput_log(char c)
{
  sio_pol_snd_chr(c, LOGTASK_PORTID);
}
