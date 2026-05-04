# vfb_sync
This is a toy project to create vfb and cirtual input and implenment them in user space
- ili9486_vfb.c create a ko and opens up /dev/fbx
- lcd_bridge.c communicate with ili9486_vfb kernal module and implement its function at user space
- ts_backend.c create a ko and allows user input dev to kernel
- xpt2046.c communicate with ts_backend kernal module and implement its function at user space
## Sample Hardware
- Raspberry pi 4
- MHS-3.5inch RPi Display, https://www.lcdwiki.com/zh/MHS-3.5inch_RPi_Display
## How to run

```
make
sudo insmod ili9486_vfb.ko
sudo ./lcd_bridge&
```
Then you can manupulate /dev/fbx

```
sudo insmod ts_backend.ko
sudo ./xpt2046&
```
Then the touch screen is online


## PyQt 
If you have pyqt installed, you can do the following
### Display Only

```
export QT_QPA_PLATFORM=linuxfb:fb=/dev/fb0
python3 clock.py
```
### Basic Input/Output

```
export QT_QPA_PLATFORM=linuxfb:fb=/dev/fb0
python3 touch_btn.py
```

### Calculator App

```
export QT_QPA_PLATFORM=linuxfb:fb=/dev/fb0
python3 calculator.py
```

## Known issues
- Only vertical now
- Perhaps wrong color