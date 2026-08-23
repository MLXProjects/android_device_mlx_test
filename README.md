# Generic device tree for CyanogenMod 6
Because yes :P

## What's this
I always wanted to downgrade newer devices to older Android versions, and this is my best attempt to do so.  
For testing, I use a few 2012-era Samsung devices which run 3.0/3.4 kernels and proved to be good enough targets.  
These devices are prepared to run Android 4.0 onwards, so some workarounds were needed to run the old HALs and framework.  
I still need to check whether this tree works on anything newer than these devices, since the kernels I'm using already support most Android functions out of the box.

## Before using this
Notice that you have to change a few things so that it boots on a real device:
* partitions mounted at init.rc (search for "mount ext4" and change the block devices used there)
* a prebuilt kernel, in order to generate a proper boot.img. Just overwrite the empty file at prebuilt/
* for brightness/LED control, fix paths on lights/ module and init.rc

## What do you need
Aside from a reason to do this (because really, why tf are you messing around with Froyo after +15 years?), you need some things before even start thinking about using this:
* A kernel with the following drivers enabled and working:
  - Binder
  - Ashmem
* If you want logs, a device with UART output or kernel <= 3.8, since ADB on froyo uses an older /dev node.
* A 32-bit Ubuntu 12.04 or older machine, to build CyanogenMod 6 correctly. 64-bit is a pain to set up and isn't worth it.
* Time (if you are here, I think you already have plenty)
* Patience
* Luck, tons of it

## Current features: 
* It boots lol
* Software rendering for everything
* Generates EXT partition image for system.img
* Controls brightness, touch key LEDs and flashlight if the right paths are set
* Init keeps ADB always enabled, and doesn't remount / as read-only (/system is read-only tho)

## TODO: 
* Check vibration support, since libhardware-legacy implements it and paths seems right but it doesn't work
* Implement on-tree ADB since the default one doesn't work on too new kernels (+3.10)
* Automatic partition handling? I couldn't find a better approach than "change things in init.rc"
* Better display management? Surfaceflinger complains but works, albeit slowly on some devices
* Framebuffer depth & pixel layout detection, should be read from FBIOGET_FSCREENINFO or ported from Libaroma
* Enable sound using ALSA
* Modular Wi-Fi and Bluetooth (Wi-Fi is hard since NL80211 wasn't available until ICS, not sure about bluetooth)

## Patches:
The following modifications need to be made to the CyanogenMod source:
* (mandatory) cm6-build-fixes: some repositories were left in broken state during the froyo->gingerbread CyanogenMod upgrade, this fixes that.
* (optional) mkbootimg: patched to use pagesize of 4096 instead of the hardcoded 2048 - default value doesn't seem to work on eMMC devices

## Modules
* `lights/`: a minimal lights module for allowing to control brightness/leds given their paths.

* `gralloc/`: a framebuffer-only copy of the stock gralloc, built as `gralloc.test` and
  patched for the more common **32-bit BGRA** format.
  `TARGET_BOOTLOADER_BOARD_NAME := test` sets `ro.product.board=test`, so `hw_get_module()`
  loads `gralloc.test.so` before the stock `gralloc.default.so`.

* `vtouch/`: a minimal static boot-time daemon (`/sbin/vtouch`) that finds the
  first `/dev/input/event*` node matching a multi-touch screen, creates a
  single-touch uinput device `mlx_vtouch` forwarding only the first slot, then
  removes the original node file and renames the virtual node in its place before
  the framework parses input devices.
  I didn't like keeping both devices available since it may lead to funny interactions.
