# ips3608-flash-macos

Flash firmware to the **FNIRSI IPS3608** bench power supply from **macOS**.

FNIRSI's instructions say to drag the `.bin` onto the USB drive the power supply shows in upgrade mode. On macOS that often does nothing. This repo has a small Python tool that writes the firmware so the bootloader accepts it.

Full write-up: <https://yakovlev.cc/articles/260818_fnirsi_ips3608_firmware_macos/>

> ⚠️ This tool writes raw sectors to a USB block device. It checks that the target is the IPS3608 bootloader disk before writing anything, but you use it at your own risk. Do not disconnect USB or power during an update.

## Why Finder copy fails

In upgrade mode the IPS3608 shows up as a tiny FAT12 disk:

| Property | Value |
|---|---|
| Media name | `Msc iap` |
| Volume | `IPS3608` (contains `Ready.TXT`) |
| File system | MS-DOS FAT12 |
| Disk size | 8 386 560 bytes |
| Device block size | **2048 bytes** |
| USB name | `AT32 Mass Storage` (Artery AT32 MCU) |

- A Finder copy creates the file, but pressing RUN/STOP does nothing.
- `mcopy` straight on `/dev/rdiskN` fails with `plain_io read/write: Invalid argument`, because of the 2048-byte block size.

The fix is to copy the whole disk into an image, add the file with mtools, and write back **only the sectors that changed**.

## Requirements

- macOS (tested on Apple Silicon)
- Python 3
- [mtools](https://www.gnu.org/software/mtools/): `brew install mtools`
- A USB cable that carries data. Several ordinary USB-C cables did not work. The original FNIRSI cable did.
- The firmware `.bin` from FNIRSI (not included here; download it from <https://www.fnirsi.com>)

## Usage

1. **Enter Firmware Upgrade mode:** power the IPS3608 off. Hold the **upper button** and power it on while still holding it. The *Firmware Upgrade* screen appears.
2. Connect the **rear USB data port** to the Mac. A volume named `IPS3608` should mount. In normal mode the unit shows up as `AT32 Virtual Com Port` instead, so it is not in upgrade mode yet.
3. Run:

   ```sh
   sudo python3 -I ips3608_flash.py ~/Downloads/APP_IPS3608_V1.9_260429.bin
   ```

   Answer `y` to confirm.

   To check everything without writing, add `--dry-run`.

4. Then either of these happens:
   - **The disk disappears right after the write.** The bootloader picked up the file and started flashing on its own. This is what happened on our unit with V1.9. Wait for the device to reboot and check the version on screen.
   - **`Verification successful`.** Short-press **RUN/STOP** on the IPS3608 to start the update.

   If you see `ERROR` before either of those, **do not press RUN/STOP**.

## What the script does

1. Finds exactly one disk with media `Msc iap`. It checks the volume name, USB protocol, disk size (`Size` = 8386560, not `TotalSize`, which is the FAT size 8355840), 2048-byte block size, and FAT12.
2. Asks for confirmation, unmounts the disk, and reads it in full with `dd bs=2048`.
3. Adds the firmware to a copy of the image with `mcopy`.
4. Compares the images in 2048-byte sectors. It refuses to write if nothing changed or if more than 512 sectors changed.
5. Writes only the changed sectors with `O_SYNC`, then calls `fsync`.
6. Reads the device back and compares it byte-for-byte, unless the device has already left USB to flash itself.

## Manual method

```sh
diskutil list external                      # find the IPS3608 disk, e.g. disk4
diskutil unmountDisk force /dev/disk4
sudo dd if=/dev/rdisk4 of=before.img bs=2048   # must be exactly 8386560 bytes
cp before.img after.img
MTOOLS_SKIP_CHECK=1 mcopy -o -i after.img APP_IPS3608_V1.9_260429.bin ::/APP_IPS3608_V1.9_260429.bin
MTOOLS_SKIP_CHECK=1 mdir -i after.img ::
```

Do **not** `dd` the whole `after.img` back. Write only the changed sectors, which is what the script does.

## Tested

| Firmware | Mac | Result |
|---|---|---|
| V1.7 (260327) | Apple Silicon MacBook Air | OK, RUN/STOP started the update |
| V1.9 (260429) | Apple Silicon MacBook Air | OK, auto-flashed right after the write (80 sectors) |

## Кратко по-русски

Скрипт прошивает блок питания FNIRSI IPS3608 с macOS. Если просто перетащить файл в Finder, прошивка обычно не запускается.

1. Выключи блок, зажми верхнюю кнопку и, не отпуская её, включи питание. Появится экран Firmware Upgrade.
2. Подключи задний USB-порт. Должен появиться диск `IPS3608`.
3. Запусти `sudo python3 -I ips3608_flash.py путь/к/APP_IPS3608_*.bin` и ответь `y`.
4. Если диск пропал сразу после записи, блок прошивается сам. Если появилась строка «Verification successful», коротко нажми RUN/STOP.

Подробная статья: <https://yakovlev.cc/articles/260818_fnirsi_ips3608_firmware_macos/>

## License

MIT. See [LICENSE](LICENSE). Not affiliated with FNIRSI. Firmware files belong to FNIRSI and are not part of this repository.
