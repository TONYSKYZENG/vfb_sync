#include <stdint.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <unistd.h>
#include <stdio.h>
#include <linux/gpio.h> // 标准 Linux GPIO 控制
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <string.h>
// 模拟 ESP32 的风格定义接口
typedef struct {
    int spi_fd;
    int dc_fd;
    int rst_fd;
    int sync_fd;
    uint16_t *fb_ptr;
} lcd_hw_t;

/**
 * 模拟 ESP32 的 gpio_set_level
 * 使用 Linux GPIO V2 请求单行输出
 */
int linux_gpio_export_output(int chip_fd, int line_num) {
    struct gpiohandle_request req;
    req.lineoffsets[0] = line_num;
    req.lines = 1;
    req.flags = GPIOHANDLE_REQUEST_OUTPUT;
    strcpy(req.consumer_label, "lcd_hal");
    
    if (ioctl(chip_fd, GPIO_GET_LINEHANDLE_IOCTL, &req) < 0) {
        perror("GPIO export failed");
        return -1;
    }
    return req.fd; // 返回用于控制该引脚的句柄
}

void hal_gpio_write(int line_fd, int val) {
    struct gpiohandle_data data;
    data.values[0] = val;
    ioctl(line_fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &data);
}
// 增强型的复位操作
void hal_lcd_reset(lcd_hw_t *hw) {
    hal_gpio_write(hw->rst_fd, 1);
    usleep(50000);
    hal_gpio_write(hw->rst_fd, 0);
    usleep(200000); // 必须拉低足够长时间
    hal_gpio_write(hw->rst_fd, 1);
    usleep(200000);
}
// SPI 发送字节 (利用 Linux 内核轮子)
void hal_spi_send(int fd, uint8_t *data, size_t len) {
    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)data,
        .len = len,
        .speed_hz = 32000000, // 32MHz
        .bits_per_word = 8,
    };
    ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
}


// 假设是常见的 ST7789 或 ILI9341
void lcd_write_cmd(lcd_hw_t *hw, uint8_t cmd) {
    hal_gpio_write(hw->dc_fd, 0); // DC 低电平写命令
    hal_spi_send(hw->spi_fd, &cmd, 1);
}

void lcd_write_data(lcd_hw_t *hw, uint8_t *data, size_t len) {
    hal_gpio_write(hw->dc_fd, 1); // DC 高电平写数据
    hal_spi_send(hw->spi_fd, data, len);
}

// ILI9486 特有初始化指令集
void lcd_init(lcd_hw_t *hw) {
    hal_lcd_reset(hw);

    // 1. 开启驱动能力 (Interface Mode Control)
    lcd_write_cmd(hw, 0xB0);
    uint8_t b0_data = 0x00; 
    lcd_write_data(hw, &b0_data, 1);

    // 2. 退出睡眠
    lcd_write_cmd(hw, 0x11);
    usleep(150000);

    // 3. 像素格式 (Pixel Format Set) - 16bit/pixel
    lcd_write_cmd(hw, 0x3A);
    uint8_t pixel_format = 0x55;
    lcd_write_data(hw, &pixel_format, 1);

    // 4. 设置屏幕方向与扫描方式 (关键：解决 1/3 闪烁问题)
    // 0x48 对应常见的竖屏模式，如果镜像不对可以尝试 0x88 或 0x28
    lcd_write_cmd(hw, 0x36);
    uint8_t madctl = 0x48; 
    lcd_write_data(hw, &madctl, 1);

    // 5. 显示时序与 Gamma 校正 (ILI9486 默认通常能亮，关键是显存对齐)
    lcd_write_cmd(hw, 0x20); // Display Inversion ON (如果颜色反转就关掉它)

    // 6. 开启显示
    lcd_write_cmd(hw, 0x29);
    usleep(150000);
}
void lcd_set_address_window(lcd_hw_t *hw, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    uint8_t data[4];

    // Column Address Set (0x2A)
    lcd_write_cmd(hw, 0x2A);
    data[0] = (x0 >> 8) & 0xFF; data[1] = x0 & 0xFF;
    data[2] = (x1 >> 8) & 0xFF; data[3] = x1 & 0xFF;
    lcd_write_data(hw, data, 4);

    // Page Address Set (0x2B)
    lcd_write_cmd(hw, 0x2B);
    data[0] = (y0 >> 8) & 0xFF; data[1] = y0 & 0xFF;
    data[2] = (y1 >> 8) & 0xFF; data[3] = y1 & 0xFF;
    lcd_write_data(hw, data, 4);

    // Memory Write (0x2C)
    lcd_write_cmd(hw, 0x2C);
}

 lcd_hw_t my_lcd;
void lcd_clear_landscape(lcd_hw_t *hw, uint16_t color) {
    // 1. 设置 MADCTL 为横屏 (Landscape)
    // 0x28: 交换行列 + BGR 顺序 (常见横屏设置)
    // 如果画面还是竖着的或者位移，尝试 0xE8, 0x48, 0xA8
    uint8_t madctl = 0x28; 
    lcd_write_cmd(hw, 0x36);
    lcd_write_data(hw, &madctl, 1);

    // 2. 设置窗口为全屏横屏范围
    // Column Address: 0 -> 479
    // Page Address:   0 -> 319
    lcd_set_address_window(hw, 0, 0, 479, 319);

    // 3. 准备颜色数据
    uint8_t color_high = (color >> 8) & 0xFF;
    uint8_t color_low  = color & 0xFF;
    uint8_t line_buf[480 * 2]; // 准备一整行的数据
    for (int i = 0; i < 480; i++) {
        line_buf[i * 2]     = color_high;
        line_buf[i * 2 + 1] = color_low;
    }

    // 4. 开始写入显存
    // DC 置高进入数据模式
    hal_gpio_write(hw->dc_fd, 1);
    
    // 循环 320 次，每次发 480 像素的数据
    for (int i = 0; i < 320; i++) {
        hal_spi_send(hw->spi_fd, line_buf, sizeof(line_buf));
    }
}
uint16_t *tdata;
void on_fb_dirty(int signum) {
    // 这里的逻辑就是：内核说脏了，我就通过 SPI 搬运到物理屏
    lcd_set_address_window(&my_lcd, 0, 0, 479, 319);
    //printf("on fb dirty \r\n");
    // DC 高电平，准备传数据
    struct gpiohandle_data d = {.values = {1}};
    ioctl(my_lcd.dc_fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &d);
    uint16_t *pdata;
    pdata = my_lcd.fb_ptr;
    for (int i=0;i<320*480;i++){
        uint16_t color =  pdata[i];
        // 将高 8 位和低 8 位交换
        tdata[i] = ((color & 0xFF) << 8) | ((color & 0xFF00) >> 8);
    }
   // 2. 按照新的行宽搬运数据
    // 现在一行是 480 像素，总共 320 行
    for (int i = 0; i < 320; i++) {
        hal_spi_send(my_lcd.spi_fd, (uint8_t *)(tdata + i * 480), 480 * 2);
    }
}

int main() {
 

    // 2. 打开 SPI 设备
    int spi_fd = open("/dev/spidev0.0", O_RDWR);
    uint8_t mode = SPI_MODE_0; // 确保是 Mode 0
    ioctl(spi_fd, SPI_IOC_WR_MODE, &mode);

    // 1. 打开 GPIO 控制器 (树莓派4的 GPIO 在 /dev/gpiochip4)
    int gpio_chip_fd = open("/dev/gpiochip4", O_RDWR);
    if (gpio_chip_fd < 0) {
        perror("Failed to open gpiochip (try sudo)");
        return -1;
    }
    my_lcd.spi_fd = spi_fd;
    my_lcd.dc_fd = linux_gpio_export_output(gpio_chip_fd, 20);  // GPIO24
    my_lcd.rst_fd = linux_gpio_export_output(gpio_chip_fd, 21); // GPIO25

    // 5. 驱动屏幕
    printf("Initializing LCD...\n");
    lcd_init(&my_lcd);

    // 6. 清屏示例 (刷红色)
    printf("Drawing screen...\n");
    tdata = (uint16_t *)malloc(320*480*sizeof(uint16_t));
    // 满屏红色测试

    // 满屏红色测试
lcd_set_address_window(&my_lcd, 0, 0, 319, 479);

// ILI9486 刷大块数据时，建议一次性组织好 Buffer 减少系统调用损耗
uint8_t color_buf[640]; // 存储一行 (320像素 * 2字节)
for(int i=0; i<320; i++) {
    color_buf[i*2] = 0xF8;    // Red High
    color_buf[i*2+1] = 0x00;  // Red Low
}

printf("Flashing ILI9486...\n");
for(int i=0; i<480; i++) {
    lcd_write_data(&my_lcd, color_buf, 640);
}
// 2. 映射 FB 显存
    int fb_fd = open("/dev/fb0", O_RDONLY);
    my_lcd.fb_ptr = mmap(NULL, 320*480*2, PROT_READ, MAP_SHARED, fb_fd, 0);

    // 3. 绑定异步信号
    int sync_fd = open("/dev/lcd_sync", O_RDONLY);
    my_lcd.sync_fd = sync_fd;
    signal(SIGIO, on_fb_dirty);
    fcntl(sync_fd, F_SETOWN, getpid());
    int flags = fcntl(sync_fd, F_GETFL);
    fcntl(sync_fd, F_SETFL, flags | FASYNC);

    // 让出 CPU，等待信号触发
    printf("Bridge active. Using SIGIO for async sync.\n");
    while(1) pause();

    close(spi_fd);
    free(tdata);
    return 0;
}