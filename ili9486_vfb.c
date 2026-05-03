#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fb.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/mm.h>

#define WIDTH 320
#define HEIGHT 480
#define BPP 16
#define VIDEOMEMSIZE (WIDTH * HEIGHT * BPP / 8)

static struct fb_info *info;
static struct fasync_struct *async_queue;
static int major;
static struct class *lcd_class;
static struct device *lcd_device;

/* 
 * 脏页回调：
 * 当用户态通过 mmap 写入 fb1 时，内核会捕获到脏页，
 * 并在此回调中通知我们的用户态“干活程序”。
 */
static void vfb_deferred_io(struct fb_info *info, struct list_head *pagelist)
{
    if (async_queue) {
        // 发送异步通知信号 (SIGIO)
        kill_fasync(&async_queue, SIGIO, POLL_IN);
    }
}

static struct fb_deferred_io vfb_defio = {
    .delay          = HZ / 30,    // 刷新率上限 30fps
    .deferred_io    = vfb_deferred_io,
};

/* 异步信号注册逻辑 */
static int lcd_sync_fasync(int fd, struct file *filp, int on)
{
    return fasync_helper(fd, filp, on, &async_queue);
}

static const struct file_operations sync_fops = {
    .owner  = THIS_MODULE,
    .fasync = lcd_sync_fasync,
};

/* Framebuffer 操作集 */
static struct fb_ops vfb_ops = {
    .owner          = THIS_MODULE,
    .fb_read        = fb_sys_read,
    .fb_write       = fb_sys_write,
    .fb_fillrect    = sys_fillrect,
    .fb_copyarea    = sys_copyarea,
    .fb_imageblit   = sys_imageblit,
    .fb_mmap        = fb_deferred_io_mmap, // 必须用这个处理写保护映射
};

static int __init vfb_init(void)
{
    int ret;

    // 1. 分配 fb_info 结构体
    info = framebuffer_alloc(0, NULL);
    if (!info)
        return -ENOMEM;

    // 2. 分配显存页 - 这是解决 OOM 崩溃的关键
    // 它会自动分配连续页并处理好 mkwrite 所需的页面标志
    info->screen_base = vmalloc(VIDEOMEMSIZE);
    if (!info->screen_base) {
        ret = -ENOMEM;
        goto err_fb_alloc;
    }
    memset(info->screen_base, 0, VIDEOMEMSIZE);

    // 3. 配置 fb_info 参数
    info->fbops = &vfb_ops;
    
    // 设置显示参数
    info->var.xres = info->var.xres_virtual = WIDTH;
    info->var.yres = info->var.yres_virtual = HEIGHT;
    info->var.bits_per_pixel = BPP;
    
    // RGB565 颜色定义
    info->var.red.offset = 11; info->var.red.length = 5;
    info->var.green.offset = 5; info->var.green.length = 6;
    info->var.blue.offset = 0; info->var.blue.length = 5;

    info->fix.type = FB_TYPE_PACKED_PIXELS;
    info->fix.visual = FB_VISUAL_TRUECOLOR;
    info->fix.line_length = WIDTH * 2;
    info->fix.smem_len = VIDEOMEMSIZE;
    strcpy(info->fix.id, "ili9486_vfb");

    // 4. 启用 Deferred IO (脏页跟踪)
    info->fbdefio = &vfb_defio;
    fb_deferred_io_init(info);

    // 5. 注册 Framebuffer
    ret = register_framebuffer(info);
    if (ret < 0)
        goto err_vfree;

    // 6. 自动创建字符设备节点 /dev/lcd_sync
    major = register_chrdev(0, "lcd_sync", &sync_fops);
    if (major < 0) {
        ret = major;
        goto err_unreg_fb;
    }
    
    lcd_class = class_create("lcd_bridge");
    if (IS_ERR(lcd_class)) {
        ret = PTR_ERR(lcd_class);
        goto err_unreg_chr;
    }
    
    lcd_device = device_create(lcd_class, NULL, MKDEV(major, 0), NULL, "lcd_sync");
    if (IS_ERR(lcd_device)) {
        ret = PTR_ERR(lcd_device);
        goto err_destroy_cls;
    }

    printk(KERN_INFO "ILI9486 VFB: Driver loaded. /dev/fb%d and /dev/lcd_sync ready.\n", info->node);
    return 0;

err_destroy_cls:
    class_destroy(lcd_class);
err_unreg_chr:
    unregister_chrdev(major, "lcd_sync");
err_unreg_fb:
    unregister_framebuffer(info);
err_vfree:
    fb_deferred_io_cleanup(info);
    vfree(info->screen_base);
err_fb_alloc:
    framebuffer_release(info);
    return ret;
}

static void __exit vfb_exit(void)
{
    device_destroy(lcd_class, MKDEV(major, 0));
    class_destroy(lcd_class);
    unregister_chrdev(major, "lcd_sync");
    
    fb_deferred_io_cleanup(info);
    unregister_framebuffer(info);
    vfree(info->screen_base);
    framebuffer_release(info);
    
    printk(KERN_INFO "ILI9486 VFB: Driver unloaded.\n");
}

module_init(vfb_init);
module_exit(vfb_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Gemini");
MODULE_DESCRIPTION("ILI9486 Virtual Framebuffer with Async Sync");