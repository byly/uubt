PRJ = uubt-noos
TRGT = arm-none-eabi-

include_fw = 1
shmem_pr   =
blink_leds =

ifneq ($(UUBT),)
MY_INC = $(UUBT)/0my/include
MY_LIB = $(UUBT)/0my/lib

BTSTACK_ROOT = $(UUBT)/btstack
else
BTSTACK_ROOT = b
endif

ST = m

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
#CFLAGS += -msoft-float

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
CFLAGS += -Wl,-T,stm32_flash.ld

CFLAGS += -I.
ifneq ($(MY_INC),)
CFLAGS += -I$(MY_INC)
endif
CFLAGS += -I$(BTSTACK_ROOT)/include
CFLAGS += -I$(BTSTACK_ROOT)/src
CFLAGS += -I$(ST)/Libraries/CMSIS/ST/STM32F4xx/Include
CFLAGS += -I$(ST)/Libraries/CMSIS/Include
CFLAGS += -I$(ST)/Libraries/STM32F4xx_StdPeriph_Driver/inc
CFLAGS += -I$(ST)/Libraries/STM32_USB_HOST_Library/Core/inc
CFLAGS += -I$(ST)/Libraries/STM32_USB_HOST_Library/Class/HID/inc
CFLAGS += -I$(ST)/Libraries/STM32_USB_OTG_Driver/inc
CFLAGS += -I$(ST)/Utilities/STM32F4-Discovery

BTS_CORE   = \
    $(BTSTACK_ROOT)/src/btstack_memory.c        \
    $(BTSTACK_ROOT)/src/linked_list.c	        \
    $(BTSTACK_ROOT)/src/memory_pool.c           \
    $(BTSTACK_ROOT)/src/run_loop.c		\
    $(BTSTACK_ROOT)/src/run_loop_embedded.c	\

BTS_COMMON = $(BTS_CORE) \
    $(BTSTACK_ROOT)/src/hci.c			       	\
    $(BTSTACK_ROOT)/src/hci_cmds.c		        \
    $(BTSTACK_ROOT)/src/hci_dump.c		        \
    $(BTSTACK_ROOT)/src/l2cap.c			        \
    $(BTSTACK_ROOT)/src/l2cap_signaling.c         	\
    $(BTSTACK_ROOT)/src/remote_device_db_memory.c 	\
    $(BTSTACK_ROOT)/src/rfcomm.c			\
    $(BTSTACK_ROOT)/src/sdp.c			        \
    $(BTSTACK_ROOT)/src/sdp_util.c                	\
    $(BTSTACK_ROOT)/src/utils.c			        \

MAIN_SOURCES = \
	$(ST)/Libraries/CMSIS/ST/STM32F4xx/Source/Templates/gcc_ride7/startup_stm32f4xx.s \
	main.c stm32f4xx_it.c system_stm32f4xx.c usbh_bt_core.c hci_transport_usb.c usbh_usr.c usb_bsp.c \
	$(ST)/Utilities/STM32F4-Discovery/stm32f4_discovery.c  \
	$(ST)/Libraries/STM32_USB_OTG_Driver/src/usb_core.c    \
	$(ST)/Libraries/STM32_USB_OTG_Driver/src/usb_hcd.c     \
	$(ST)/Libraries/STM32_USB_OTG_Driver/src/usb_hcd_int.c \
	$(ST)/Libraries/STM32_USB_HOST_Library/Core/src/*.c    \
	$(BTS_COMMON)  \
	newlib_stubs.c \

%.bin: %.elf
	$(OBJCOPY) -O binary $^ $@

ifeq ($(shmem_pr),)

all: $(BIN_IMAGE)
$(EXECUTABLE): $(MAIN_SOURCES)
else

all: $(BIN_IMAGE) shmem-addr

shmem-addr:
	@$(NM) $(EXECUTABLE) | grep 'D shmem'

$(EXECUTABLE): $(MY_LIB)/shmem.c $(MY_LIB)/printf.c $(MAIN_SOURCES)
endif
	$(CC) $(CFLAGS) $^ -o $@ -lc -lgcc \
	-L$(ST)/Libraries/STM32F4xx_StdPeriph_Driver -lSTM32F4xx_StdPeriph_Driver


ifneq ($(include_fw),)
$(EXECUTABLE): ath3k_fw.h
endif

ifneq ($(blink_leds),)
$(EXECUTABLE): $(MY_LIB)/led_pwm.c
endif

ath3k_fw.h:
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
	@rm -f $(EXECUTABLE) $(BIN_IMAGE) ath3k_fw.h tags

.PHONY: all clean tags size
