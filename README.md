# vfb_sync
This is a toy project to create vfb and implenment it in user space
- ili9486_vfb.c create a ko and opens up /dev/fbx
- lcd_bridge.c communicate with kernal module and implement its function at user space
## Sample Hardware
- Raspberry pi 4
- MHS-3.5inch RPi Display, https://www.lcdwiki.com/zh/MHS-3.5inch_RPi_Display
## How to run

```
make
sudo insmod *.ko
sudo ./lcd_bridge
```
Then you can manupulate /dev/fbx

### pyqt
If you have pyqt installed, you can do the following

```
export QT_QPA_PLATFORM=linuxfb:fb=/dev/fb0
python3 clock.py
```
## Known issues
- Only vertical now
- Perhaps wrong color