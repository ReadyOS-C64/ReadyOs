# readyos easyflash

- attach `readyos_easyflash.crt` as an easyflash cartridge
- mount `readyos_data.d64` on drive `8`
- enable REU with at least `1MB`; `8MB` or `16MB` is recommended where available
- if an app snapshot preloaded from the cartridge is unloaded from REU, ReadyOS cannot load it again from the cartridge until you restart ReadyOS

## vice example

```sh
x64sc -reu -reusize 16384 -cartcrt readyos_easyflash.crt -drive8type 1541 -devicebackend8 0 +busdevice8 -8 readyos_data.d64
```
