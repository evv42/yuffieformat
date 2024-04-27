#yuffieformat - D88 disk image writer for PC-98 DOS

This software is still under development, expect bugs in the writing procedure.
Compiled executable (yf.com) is included for convinience.

##Usage

```
yf w <file> <drive> : write file to specified drive number
yf i <file> : get tracks informations for the specified file image
yf s <track> <drive> : seek drive to specified track
```

##Limitations (may be fixed in a future update)

- Copy-protected disks are not supported
- Disks images that contain deleted sectors (maked as "D" in the info option)
- Double-density (2D / 2DD) images
