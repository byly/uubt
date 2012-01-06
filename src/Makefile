PRJ = uubt-noos
TRGT = arm-none-eabi-

include_fw =
shmem_pr   =
blink_leds =

LR = .

ifneq ($(UUBT),)
MY_INC = $(UUBT)/0my/include
MY_LIB = $(UUBT)/0my/lib

BTSTACK = $(UUBT)/btstack
ST      = $(UUBT)/stm32f4-discovery
else
BTSTACK = b
ST      = m
endif

ATH3K_FW = /lib/firmware/ath3k-1.fw

EXECUTABLE = $(PRJ).elf
BIN_IMAGE  = $(PRJ).bin

CC = $(TRGT)gcc
NM = $(TRGT)nm
OBJCOPY = $(TRGT)objcopy
OBJDUMP = $(TRGT)objdump

CFLAGS  = -g -O0 -mlittle-endian -mthumb
CFLAGS += -mcpu=cortex-m4
CFLAGS += -ffreestanding -nostdlib

CFLAGS += -DUSE_STDPERIPH_DRIVER=1 -DSTM32F4XX=1
CFLAGS += -DUSE_USB_OTG_FS=1
ifneq ($(shmem_pr),)
CFLAGS += -D__SHMEM_BOARD_SIDE
CFLAGS += -DPRINT_PACKETS
endif
ifneq ($(include_fw),)
CFLAGS += -DINCLUDE_FIRMWARE
endif
ifneq ($(blink_leds),)
CFLAGS += -DBLINK_LEDS
endif

# to run from FLASH
CFLAGS += -Wl,-T,$(LR)/clones/stm32_flash.ld

UINCDIR = $(LR) $(LR)/conf $(LR)/clones $(LR)/dummy       \
    $(BTSTACK)/include                                    \
    $(BTSTACK)/src                                        \
    $(ST)/Libraries/CMSIS/ST/STM32F4xx/Include            \
    $(ST)/Libraries/CMSIS/Include                         \
    $(ST)/Libraries/STM32F4xx_StdPeriph_Driver/inc        \
    $(ST)/Libraries/STM32_USB_HOST_Library/Core/inc       \
    $(ST)/Libraries/STM32_USB_OTG_Driver/inc              \
    $(ST)/Utilities/STM32F4-Discovery                     \

ifneq ($(MY_INC),)
UINCDIR += $(MY_INC)
endif

CFLAGS += $(patsubst %,-I%,$(UINCDIR))

BTS_CORE   = \
    $(BTSTACK)/src/btstack_memory.c          \
    $(BTSTACK)/src/linked_list.c             \
    $(BTSTACK)/src/memory_pool.c             \
    $(BTSTACK)/src/run_loop.c                \
    $(BTSTACK)/src/run_loop_embedded.c       \

BTS_COMMON =   \
    $(BTSTACK)/src/hci.c                     \
    $(BTSTACK)/src/hci_cmds.c                \
    $(BTSTACK)/src/hci_dump.c                \
    $(BTSTACK)/src/l2cap.c                   \
    $(BTSTACK)/src/l2cap_signaling.c         \
    $(BTSTACK)/src/remote_device_db_memory.c \
    $(BTSTACK)/src/rfcomm.c                  \
    $(BTSTACK)/src/sdp.c                     \
    $(BTSTACK)/src/sdp_util.c                \
    $(BTSTACK)/src/utils.c                   \

ST_SOURCES = \
    $(ST)/Libraries/CMSIS/ST/STM32F4xx/Source/Templates/gcc_ride7/startup_stm32f4xx.s \
    $(ST)/Libraries/STM32_USB_OTG_Driver/src/usb_core.c             \
    $(ST)/Libraries/STM32_USB_OTG_Driver/src/usb_hcd.c              \
    $(ST)/Libraries/STM32_USB_OTG_Driver/src/usb_hcd_int.c          \
    $(wildcard $(ST)/Libraries/STM32_USB_HOST_Library/Core/src/*.c) \
    $(ST)/Utilities/STM32F4-Discovery/stm32f4_discovery.c           \

LOCAL_SOURCES = \
    $(LR)/main.c $(LR)/usbh_bt_core.c $(LR)/hci_transport_uubt.c \
    $(LR)/os/none/hal_bt_os.c $(LR)/os/none/stm32f4xx_it.c       \
    $(LR)/clones/system_stm32f4xx.c $(LR)/clones/usb_bsp.c       \
    $(LR)/clones/newlib_stubs.c                                  \
    $(LR)/dummy/usbh_usr.c

MAIN_SOURCES := $(BTS_CORE) $(BTS_COMMON) $(ST_SOURCES) $(LOCAL_SOURCES)

ifneq ($(shmem_pr),)
SHMEM_SOURCES = $(MY_LIB)/shmem.c $(MY_LIB)/printf.c
MAIN_SOURCES := $(SHMEM_SOURCES) $(MAIN_SOURCES)
endif

ifneq ($(include_fw),)
MAIN_SOURCES := $(MAIN_SOURCES) ../firmware/ath3k_fw.c
endif

ifneq ($(blink_leds),)
MAIN_SOURCES := $(MAIN_SOURCES) $(MY_LIB)/led_pwm.c
endif

%.bin: %.elf
	$(OBJCOPY) -O binary $^ $@

ifeq ($(shmem_pr),)
all: $(BIN_IMAGE)
else
all: $(BIN_IMAGE) shmem-addr

shmem-addr:
	@$(NM) $(EXECUTABLE) | grep 'D shmem'
endif

$(EXECUTABLE): $(MAIN_SOURCES)
	$(CC) $(CFLAGS) $(filter-out ../firmware/ath3k_fw.c,$^) -o $@ -lc -lgcc \
	-L$(ST)/Libraries/STM32F4xx_StdPeriph_Driver -lSTM32F4xx_StdPeriph_Driver


../firmware/ath3k_fw.c:
	@mkdir -p ../firmware
	@(echo 'const unsigned char ath3k_fw_cmd[] = {' ; \
	dd if=$(ATH3K_FW) bs=1 count=20 2>/dev/null | xxd -i ; \
	echo '};' ; \
	echo 'const unsigned char ath3k_fw_data[] __attribute__ ((aligned (4))) = {' ; \
	dd if=$(ATH3K_FW) bs=1  skip=20 2>/dev/null | xxd -i ; \
	echo '};' ) > $@

tags:
	ctags -R --links=no . $(MY_LIB)

size: $(EXECUTABLE)
	$(OBJDUMP) -h $^

clean:
	@rm -rf $(EXECUTABLE) $(BIN_IMAGE) ../firmware tags

.PHONY: all clean tags size
