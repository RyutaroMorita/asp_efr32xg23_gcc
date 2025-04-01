#
#  @(#) $Id: Makefile.core 2728 2015-12-30 01:46:11Z ertl-honda $
# 

#
#		Makefile のターゲット依存部（EFR32xG23用）
#

# 
#  チップ依存部ディレクトリ名の定義 
# 
CHIPDIR = $(SRCDIR)/arch/$(PRC)_$(TOOL)

#
#  コアタイプ
#
CORE_TYPE = CORTEX_M33

#
#  コンパイルオプション
#
INCLUDES := $(INCLUDES) -I$(SRCDIR)/arch/$(PRC)_$(TOOL)/$(CHIP) \
            -I$(SRCDIR)/arch/$(PRC)_$(TOOL)/$(CHIP)/STM32F4xx_HAL_Driver/Inc \
            -I$(SRCDIR)/arch/$(PRC)_$(TOOL)/$(CHIP)/CMSIS/Device/ST/STM32F4xx/Include \
            -I$(SRCDIR)/arch/$(PRC)_$(TOOL)/$(CHIP)/CMSIS/Include
COPTS := $(COPTS) -mlittle-endian -nostartfiles
LDFLAGS := $(LDFLAGS) -mlittle-endian
LIBS := $(LIBS)

#
#  カーネルに関する定義
#
KERNEL_DIR := $(KERNEL_DIR) $(SRCDIR)/arch/$(PRC)_$(TOOL)/$(CHIP) \
              $(SRCDIR)/arch/$(PRC)_$(TOOL)/$(CHIP)/STM32F4xx_HAL_Driver/Src
KERNEL_ASMOBJS := $(KERNEL_ASMOBJS)
KERNEL_COBJS := $(KERNEL_COBJS) prc_timer.o

#
#  システムサービスに関する定義
#
SYSSVC_DIR := $(SYSSVC_DIR) $(SRCDIR)/arch/$(PRC)_$(TOOL)/$(CHIP)
SYSSVC_COBJS := $(SYSSVC_COBJS)

# 
#  オフセットファイル生成のための定義 
# 
OFFSET_TF := $(CHIPDIR)/prc_offset.tf

#
#		Makefile のプロセッサ依存部（ARM-M用）
#

# 
#  コア依存部ディレクトリ名の定義 
# 
COREDIR = $(SRCDIR)/arch/$(PRC)_$(TOOL)

#
#  ツール依存部ディレクトリ名の定義 
#
TOOLDIR = $(SRCDIR)/arch/$(TOOL)

#
#  コンパイルオプション
#
COPTS := $(COPTS) -mthumb -mcmse -std=c18 --specs=nano.specs
INCLUDES := $(INCLUDES) -I$(COREDIR) -I$(TOOLDIR)
LDFLAGS := $(LDFLAGS) 
CDEFS := $(CDEFS)
LIBS := $(LIBS) -lgcc -lnosys

#
#  カーネルに関する定義
#
KERNEL_DIR := $(KERNEL_DIR) $(COREDIR)
KERNEL_ASMOBJS := $(KERNEL_ASMOBJS)
KERNEL_COBJS := $(KERNEL_COBJS) prc_config.o

#
#  コアのタイプによる違い
#
ifeq ($(CORE_TYPE),CORTEX_M33)
	ARM_ARCH = ARMV8M
	COPTS := $(COPTS) -mcpu=cortex-m33
	CDEFS := $(CDEFS) -DTOPPERS_CORTEX_M33
	FPU_ARCH_MACRO = __TARGET_FPU_FPV4_SP
	FPU_ARCH_OPT   = fpv5-sp-d16
	FPU_ABI        = hard
else ifeq ($(CORE_TYPE),CORTEX_M4)
	ARM_ARCH = ARMV7M
	COPTS := $(COPTS) -mcpu=cortex-m4
	CDEFS := $(CDEFS) -DTOPPERS_CORTEX_M4
	FPU_ARCH_MACRO = __TARGET_FPU_FPV4_SP
	FPU_ARCH_OPT   = fpv4-sp-d16
else ifeq ($(CORE_TYPE),CORTEX_M0PLUS)
	ARM_ARCH = ARMV6M
	COPTS := $(COPTS) -mcpu=cortex-m0plus
	CDEFS := $(CDEFS) -DTOPPERS_CORTEX_M0PLUS
else ifeq ($(CORE_TYPE),CORTEX_M0)
	ARM_ARCH = ARMV6M
	COPTS := $(COPTS) -mcpu=cortex-m0
	CDEFS := $(CDEFS) -DTOPPERS_CORTEX_M0
else ifeq ($(CORE_TYPE),CORTEX_M3)
	ARM_ARCH = ARMV7M
	COPTS := $(COPTS) -mcpu=cortex-m3
	CDEFS := $(CDEFS) -DTOPPERS_CORTEX_M3
endif

#
#  アーキテクチャ毎に異なる設定
#
ifeq ($(ARM_ARCH),ARMV8M)
	KERNEL_ASMOBJS := $(KERNEL_ASMOBJS) prc_support.o
	CDEFS := $(CDEFS) -D__TARGET_ARCH_THUMB=5
else ifeq ($(ARM_ARCH),ARMV7M)
	KERNEL_ASMOBJS := $(KERNEL_ASMOBJS) prc_support.o
	CDEFS := $(CDEFS) -D__TARGET_ARCH_THUMB=4
else ifeq ($(ARM_ARCH),ARMV6M)
	KERNEL_ASMOBJS := $(KERNEL_ASMOBJS) prc_support_v6m.o
	CDEFS := $(CDEFS) -D__TARGET_ARCH_THUMB=3
endif

#
#  FPUの設定
#
ifeq ($(FPU_ABI),)
	FPU_ABI = softfp
endif 

ifeq ($(FPU_USAGE),FPU_NO_PRESERV)
	COPTS := $(COPTS) -mfloat-abi=$(FPU_ABI) -mfpu=$(FPU_ARCH_OPT)
	CDEFS := $(CDEFS) -D$(FPU_ARCH_MACRO) -DTOPPERS_FPU_ENABLE
else ifeq ($(FPU_USAGE),FPU_NO_LAZYSTACKING)
	COPTS := $(COPTS) -mfloat-abi=$(FPU_ABI) -mfpu=$(FPU_ARCH_OPT)
	CDEFS := $(CDEFS) -D$(FPU_ARCH_MACRO) -DTOPPERS_FPU_ENABLE -DTOPPERS_FPU_NO_LAZYSTACKING -DTOPPERS_FPU_CONTEXT
else ifeq ($(FPU_USAGE),FPU_LAZYSTACKING)
	COPTS := $(COPTS) -mfloat-abi=$(FPU_ABI) -mfpu=$(FPU_ARCH_OPT)
	CDEFS := $(CDEFS) -D$(FPU_ARCH_MACRO) -DTOPPERS_FPU_ENABLE -DTOPPERS_FPU_LAZYSTACKING -DTOPPERS_FPU_CONTEXT
endif


#
#  依存関係の定義
#
cfg1_out.c: $(COREDIR)/prc_def.csv
kernel_cfg.timestamp: $(COREDIR)/prc.tf

#
#  コンフィギュレータ関係の変数の定義
#
CFG_TABS := $(CFG_TABS) --cfg1-def-table $(COREDIR)/prc_def.csv
