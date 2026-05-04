#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/input.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>

struct ts_data {
    int x;
    int y;
    int pressure; // 1 为按下, 0 为抬起
};

static struct input_dev *v_ts_dev;

/**
 * 用户态写入处理函数
 * 格式要求：写入 struct ts_data 结构体数据
 */
static ssize_t ts_backend_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos) {
    struct ts_data data;

    if (count != sizeof(struct ts_data)) {
        return -EINVAL;
    }

    if (copy_from_user(&data, buffer, sizeof(struct ts_data))) {
        return -EFAULT;
    }

    // 报告触摸位置
    input_report_abs(v_ts_dev, ABS_X, data.x);
    input_report_abs(v_ts_dev, ABS_Y, data.y);
    
    // 报告触摸压力和 BTN_TOUCH 状态
    input_report_abs(v_ts_dev, ABS_PRESSURE, data.pressure);
    input_report_key(v_ts_dev, BTN_TOUCH, data.pressure > 0 ? 1 : 0);

    // 同步事件
    input_sync(v_ts_dev);

    return count;
}

static const struct file_operations ts_backend_fops = {
    .owner = THIS_MODULE,
    .write = ts_backend_write,
};

static struct miscdevice ts_misc_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "ts_backend",
    .fops = &ts_backend_fops,
};

static int __init ts_backend_init(void) {
    int ret;

    // 1. 注册杂项设备，生成 /dev/ts_backend
    ret = misc_register(&ts_misc_device);
    if (ret) return ret;

    // 2. 分配并注册输入设备
    v_ts_dev = input_allocate_device();
    if (!v_ts_dev) {
        misc_deregister(&ts_misc_device);
        return -ENOMEM;
    }

    v_ts_dev->name = "Virtual MHS-3.5 Touchscreen";
    
    // 设置支持的事件类型
    __set_bit(EV_ABS, v_ts_dev->evbit);
    __set_bit(EV_KEY, v_ts_dev->evbit);
    __set_bit(BTN_TOUCH, v_ts_dev->keybit);

    // 根据屏幕参数设置坐标范围[cite: 1]
    input_set_abs_params(v_ts_dev, ABS_X, 0, 320, 0, 0);
    input_set_abs_params(v_ts_dev, ABS_Y, 0, 480, 0, 0);
    input_set_abs_params(v_ts_dev, ABS_PRESSURE, 0, 1, 0, 0);

    ret = input_register_device(v_ts_dev);
    if (ret) {
        input_free_device(v_ts_dev);
        misc_deregister(&ts_misc_device);
        return ret;
    }

    return 0;
}

static void __exit ts_backend_exit(void) {
    input_unregister_device(v_ts_dev);
    misc_deregister(&ts_misc_device);
}

module_init(ts_backend_init);
module_exit(ts_backend_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tony");
MODULE_DESCRIPTION("User-space to Input Event Backend");