#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <linux/fb.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/gpio.h>
#include <stdint.h>
// XPT2046 指令字
#define CMD_X_READ  0xD0
#define CMD_Y_READ  0x90
// 屏幕参数
#define FB_WIDTH  320
#define FB_HEIGHT 480
#define CAL_FILE    "pointercal.txt"
// 校准系数结构体
typedef struct {
    float a[7]; // 存储 A, B, C, D, E, F, Div
} CalData;
CalData g_cal; // 全局校准数据
/**
 * 模拟 ESP32 的 gpio_set_direction (输入模式)
 */
int linux_gpio_export_input(int chip_fd, int line_num) {
    struct gpiohandle_request req;
    req.lineoffsets[0] = line_num;
    req.lines = 1;
    req.flags = GPIOHANDLE_REQUEST_INPUT; // 设置为输入模式
    strcpy(req.consumer_label, "tp_irq");
    
    if (ioctl(chip_fd, GPIO_GET_LINEHANDLE_IOCTL, &req) < 0) {
        perror("GPIO input export failed");
        return -1;
    }
    return req.fd;
}

/**
 * 读取 GPIO 电平
 */
int hal_gpio_read(int line_fd) {
    struct gpiohandle_data data;
    if (ioctl(line_fd, GPIOHANDLE_GET_LINE_VALUES_IOCTL, &data) < 0) {
        return -1;
    }
    return data.values[0];
}

uint16_t hal_spi_touch_read(int fd, uint8_t command) {
    uint8_t tx[] = {command, 0x00, 0x00};
    uint8_t rx[3] = {0};
    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx,
        .rx_buf = (unsigned long)rx,
        .len = 3,
        .speed_hz = 1000000,
        .bits_per_word = 8,
    };
    ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
    return ((rx[1] << 8) | rx[2]) >> 3;
}
// --- Framebuffer 画图函数 ---
void draw_pixel(uint16_t *fbp, int x, int y, uint16_t color) {
    if (x >= 0 && x < FB_WIDTH && y >= 0 && y < FB_HEIGHT) {
        fbp[y * FB_WIDTH + x] = color;
    }
}

void draw_cross(uint16_t *fbp, int x, int y, uint16_t color) {
    for (int i = -10; i <= 10; i++) {
        draw_pixel(fbp, x + i, y, color);
        draw_pixel(fbp, x, y + i, color);
    }
}
// --- 坐标转换函数 ---
// 坐标转换函数
int getX(int rx, int ry) {
    return (int)((g_cal.a[0] * rx + g_cal.a[1] * ry + g_cal.a[2]) / g_cal.a[6]);
}

int getY(int rx, int ry) {
    return (int)((g_cal.a[3] * rx + g_cal.a[4] * ry + g_cal.a[5]) / g_cal.a[6]);
}
// 定义与驱动一致的结构体
struct ts_data {
    uint32_t x;
    uint32_t y;
    uint32_t pressure;
};

void report_to_kernel(int fd, int x, int y, int pressed) {
    struct ts_data out = { .x = x, .y = y, .pressure = pressed };
    write(fd, &out, sizeof(struct ts_data));
}

void do_calibration(int spi_fd, int irq_fd, uint16_t *fbp) {
    // 定义5个像素参考点：左上、右上、左下、右下、中心
    int px[5] = {30, FB_WIDTH - 30, 30, FB_WIDTH - 30, FB_WIDTH / 2};
    int py[5] = {30, 30, FB_HEIGHT - 30, FB_HEIGHT - 30, FB_HEIGHT / 2};
    long tx[5], ty[5];

    printf("开始5点校准 (320x480)...\n");
    for (int i = 0; i < 5; i++) {
        // 画红点提示
        for(int x=px[i]-5; x<px[i]+5; x++) 
            for(int y=py[i]-5; y<py[i]+5; y++) 
                if(x>=0 && x<FB_WIDTH && y>=0 && y<FB_HEIGHT) {
                    draw_cross(fbp,x,y,0xff00);
                }

        // 等待按下，TP_IRQ 低电平有效
        while (hal_gpio_read(irq_fd) != 0) usleep(10000);
        
        // 采集多组取平均值以提高精度
        long sum_x = 0, sum_y = 0;
        for(int n=0; n<10; n++) {
            sum_x += hal_spi_touch_read(spi_fd, CMD_X_READ);
            sum_y += hal_spi_touch_read(spi_fd, CMD_Y_READ);
            usleep(1000);
        }
        tx[i] = sum_x / 10;
        ty[i] = sum_y / 10;

        // 等待松开，画绿点表示完成
        while (hal_gpio_read(irq_fd) == 0) usleep(10000);
        for(int x=px[i]-5; x<px[i]+5; x++) 
            for(int y=py[i]-5; y<py[i]+5; y++) 
                if(x>=0 && x<FB_WIDTH && y>=0 && y<FB_HEIGHT) fbp[y*FB_WIDTH+x] = 0x07E0;
        
        printf("点 %d 采集: Raw(%ld, %ld) -> Screen(%d, %d)\n", i, tx[i], ty[i], px[i], py[i]);
        usleep(500000);
    }

    // 计算校准系数 (最小二乘法简化版公式)
    // 这里采用常用的线性变换算法
    double det = 5.0 * (tx[0]*tx[0] + tx[1]*tx[1] + tx[2]*tx[2] + tx[3]*tx[3] + tx[4]*tx[4]) - 0; // 简化处理
    
    // 为了保持代码简洁且适应用户态读取，直接计算平均比例系数
    // 这种方式在电阻屏上通常足够稳定
    g_cal.a[6] = 1.0; // Div
    g_cal.a[0] = (float)(px[1] - px[0]) / (tx[1] - tx[0]); // A
    g_cal.a[1] = 0; // B (假设无旋转)
    g_cal.a[2] = px[4] - (tx[4] * g_cal.a[0]); // C
    
    g_cal.a[3] = 0; // D
    g_cal.a[4] = (float)(py[2] - py[0]) / (ty[2] - ty[0]); // E
    g_cal.a[5] = py[4] - (ty[4] * g_cal.a[4]); // F

    FILE *fp = fopen(CAL_FILE, "w");
    if (fp) {
        for(int i=0; i<7; i++) fprintf(fp, "%f ", g_cal.a[i]);
        fclose(fp);
        printf("5点校准数据已保存。\n");
    }
}

int main() {
    int spi_fd = open("/dev/spidev0.1", O_RDWR);
    int gpio_chip_fd = open("/dev/gpiochip0", O_RDWR);
    int calibrated =0;
    if (gpio_chip_fd < 0) {
        perror("Failed to open gpiochip (try sudo)");
        return -1;
    }
    // 在 main 循环中调用：
int backend_fd = open("/dev/ts_backend", O_WRONLY);
 if (backend_fd < 0) {
        perror("Failed to open ts_backend (try sudo)");
        return -1;
    }
   int irq_fd = linux_gpio_export_input(gpio_chip_fd, 17); // TP_IRQ

    // 尝试加载校准文件
    FILE *fp = fopen(CAL_FILE, "r");
    if (fp) {
        if (fscanf(fp, "%f %f %f %f %f %f %f", 
                   &g_cal.a[0], &g_cal.a[1], &g_cal.a[2], 
                   &g_cal.a[3], &g_cal.a[4], &g_cal.a[5], 
                   &g_cal.a[6]) == 7) {
            //printf("成功加载 5 点校准数据。\n");
            calibrated = 1;
        } else {
            fclose(fp);
            int fb_fd = open("/dev/fb0", O_RDWR);
            uint16_t *fbp = (uint16_t *)mmap(0, FB_WIDTH * FB_HEIGHT * 2, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
            do_calibration(spi_fd, irq_fd, fbp);
             munmap(fbp, FB_WIDTH * FB_HEIGHT * 2);
            close(fb_fd);
        }
    } else {
         int fb_fd = open("/dev/fb0", O_RDWR);
        uint16_t *fbp = (uint16_t *)mmap(0, FB_WIDTH * FB_HEIGHT * 2, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
        do_calibration(spi_fd, irq_fd, fbp);
         munmap(fbp, FB_WIDTH * FB_HEIGHT * 2);
        close(fb_fd);
    }

    printf("进入坐标循环读取模式...\n");
    int last_read = 1;
    int this_read = 1;
    int read_x = 0,read_y = 0;
    while (1) {
        last_read = this_read;
        this_read = hal_gpio_read(irq_fd);
        if (this_read==0&&last_read==1) { // 检测到触摸按下[cite: 1]
            int rx = hal_spi_touch_read(spi_fd, CMD_X_READ);
            int ry = hal_spi_touch_read(spi_fd, CMD_Y_READ);
             read_x = getX(rx,ry);
             read_y = getY(rx,ry);
            //printf("实时坐标 -> X: %d, Y: %d\n",read_x, read_y);
            report_to_kernel(backend_fd,read_x, read_y, 1);
            //usleep(100000); 
        }
        else if(this_read==1&&last_read==0) {
            report_to_kernel(backend_fd, read_x, read_y, 0);
        }
        usleep(10000);
    }

   
    close(spi_fd);
    close(irq_fd);
    close(gpio_chip_fd);
    close(backend_fd);
    return 0;
}