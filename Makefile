# --- 通用设置 ---
# 如果是交叉编译，可以在命令行指定 CROSS_COMPILE=arm-linux-gnueabihf-
CC := $(CROSS_COMPILE)gcc
KERNEL_DIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

# 目标文件名
MODULE_NAME := ili9486_vfb
APP_NAME := lcd_bridge

# --- 内核模块部分 ---
# obj-m 表示编译成模块
obj-m := $(MODULE_NAME).o

# --- 编译规则 ---

all: module app

# 编译内核模块
# 调用内核源码树的 Makefile 来处理复杂的内核依赖
module:
	@echo "--- Building Kernel Module ---"
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules

# 编译用户态干活程序
# -O3 优化对 SPI 搬运性能很有帮助
app:
	@echo "--- Building Userspace Bridge App ---"
	$(CC) -O3 lcd_bridge.c -o $(APP_NAME)

# --- 清理规则 ---
clean:
	@echo "--- Cleaning up ---"
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) clean
	rm -f $(APP_NAME)

# --- 辅助指令 ---
# 加载驱动并自动赋予权限（方便调试）
install:
	@sudo insmod $(MODULE_NAME).ko
	@echo "Driver inserted. Checking /dev/..."
	@ls -l /dev/fb* /dev/lcd_sync
	@sudo chmod 666 /dev/lcd_sync
	@sudo chmod 666 /dev/fb1

remove:
	@sudo rmmod $(MODULE_NAME)
	@echo "Driver removed."

.PHONY: all module app clean install remove