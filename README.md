# Generic device tree for Android 2.2
No partitions, just IMGs and some fun :)

## What's this
I always wanted to downgrade newer devices to older Android versions, and this is my best attempt to do so.  
For testing, I use a pretty old Samsung phone (GT-S6810) with UART output at it's USB port, so I don't rely on ADB or other things.  
That device is prepared to run Android 4.1 onwards, but due to it's UART port it was the best target I have available at the moment.  
I still need to check whether this tree works on anything other than this device, since the kernel I'm using already supports most Android functions out of the box.

## What do you need
Aside from a reason to do this (because really, why tf are you messing around with Froyo after +15 years?), you need some things before even start thinking about using this:
* A kernel with the following drivers enabled and working:
  - Binder
  - Ashmem
* If you want logs, a device with UART output
* A 32-bit Ubuntu 12.04 or older machine, to build AOSP 2.2 correctly. Not sure if 64-bit works.
* Time (if you are here, I think you already have plenty)
* Patience
* Luck, too much luck

## Current features: 
* It works lol
* Software rendering for everything
* Generates EXT2 image system.img
* Ramdisk expects /system and /data to be already mounted, and doesn't remount / as read-only

## TODO: 
* try to enable ADB? UART debugging is annoying at times
* Enable sound using ALSA
* Modular Wi-Fi and Bluetooth
* Better display management? Surfaceflinger complains but works
* Check wakelocks/sleep support
* Framebuffer depth & pixel layout detection, should be read from FBIOGET_FSCREENINFO or ported from Libaroma

## Patches:
The following modifications were made to the Froyo framework, and not yet uploaded due to my laziness to create .patch files from my experiments:
* Multitouch input devices are treated as single-touch (MotionEvent API was not widely used until Gingerbread)
* Framebuffer drawing was forced to 32-bit BGRA (quite device specific)
