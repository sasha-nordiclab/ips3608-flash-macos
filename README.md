# IPS3608 Flasher for macOS

A simple Mac app to flash firmware to the **[FNIRSI IPS3608](https://www.fnirsi.com/products/ips3608)** bench power supply (36 V / 8 A / 285 W).

**Nothing to install:** no Python, no Homebrew, no Terminal. Works on Apple Silicon and Intel Macs (macOS 11+).

- Device: <https://www.fnirsi.com/products/ips3608>
- Firmware downloads: <https://www.fnirsi.com/pages/software-downloads>
- Background write-up: <https://yakovlev.cc/articles/260818_fnirsi_ips3608_firmware_macos/>

> ⚠️ **Use the USB cable that came in the box with the IPS3608.** Ordinary USB-C cables often do not work. The power supply then never shows up on the Mac.

[По-русски ↓](#инструкция-по-русски)

---

## How to flash

1. **Download the app:** get **IPS3608-Flasher-macOS.zip** from [Releases](https://github.com/sasha-nordiclab/ips3608-flash-macos/releases/latest) and unzip it.
2. **Download the firmware** from the [FNIRSI downloads page](https://www.fnirsi.com/pages/software-downloads) and unzip it. You need the file named like `APP_IPS3608_V1.9_260429.bin`.
3. **Open IPS3608 Flasher.app.** The app is not notarized by Apple, so the first time **right-click → Open → Open**. If macOS still blocks it, go to System Settings → Privacy & Security → **Open Anyway**.
4. **Put the power supply into Firmware Upgrade mode.** The app shows these steps too:
   1. Turn the IPS3608 **off**.
   2. **Press and hold the upper button** on the front panel.
   3. While holding it, **turn the power on**. The **Firmware Upgrade** screen appears.
   4. Connect the **rear USB port** to the Mac with the **cable from the box**.
5. The app finds the device. **Choose the `.bin` file**, click **Yes**, and enter your Mac password.
6. **Finish**, as the app tells you:
   - *"The device started the update by itself"*: wait for it to reboot.
   - *"Written and verified"*: **short-press RUN/STOP** on the power supply.

   Do not disconnect USB or power until the device has rebooted. Then check the version in the device's settings.

**The app keeps waiting for the device?**
- Use the cable from the box.
- Check that the screen shows Firmware Upgrade. Press the button **before** turning the power on.

**Error?** Do not press RUN/STOP. Turn the unit off, enter upgrade mode again and retry.

---

## Why not just copy the file in Finder?

FNIRSI's instructions say to drag the `.bin` onto the USB drive. On macOS the file appears, but pressing RUN/STOP does nothing.

In upgrade mode the IPS3608 is a tiny FAT12 disk with a **2048-byte block size**:

| Property | Value |
|---|---|
| Media name | `Msc iap` |
| Volume | `IPS3608` (contains `Ready.TXT`) |
| File system | MS-DOS FAT12, 8 KB clusters |
| Disk size | 8 386 560 bytes |
| Device block size | 2048 bytes |
| USB name | `AT32 Mass Storage` (Artery AT32 MCU) |

The app (`src/ips3608flash.c`, plain C, no dependencies):

1. finds exactly one disk with media name `Msc iap`;
2. checks block size 2048 and disk size 8386560, and refuses to touch anything else;
3. reads the whole disk into memory;
4. adds the firmware to a copy (FAT12 long name + 8.3 entry, cluster chain in every FAT), with the same result as `mcopy` from mtools;
5. writes back **only the 2048-byte sectors that changed** (80 for V1.9), refusing to write more than 512;
6. reads the device back and compares byte-for-byte. Some units start flashing as soon as the file lands and leave USB; the app reports that as success.

## Command line

The flashing tool works without the app too:

```sh
diskutil list external                          # find the IPS3608 disk, e.g. disk4
diskutil unmountDisk force /dev/disk4
sudo ./ips3608flash /dev/rdisk4 APP_IPS3608_V1.9_260429.bin
sudo ./ips3608flash --dry-run /dev/rdisk4 APP_IPS3608_V1.9_260429.bin   # check only
```

## Build

Requires Xcode Command Line Tools (`xcode-select --install`):

```sh
./app/build.sh      # -> dist/IPS3608 Flasher.app and dist/IPS3608-Flasher-macOS.zip
```

## Tested

| Firmware | Mac | Result |
|---|---|---|
| V1.7 (260327) | Apple Silicon MacBook Air | OK, RUN/STOP started the update |
| V1.9 (260429) | Apple Silicon MacBook Air | OK, auto-flashed right after the write |

---

## Инструкция по-русски

Простое приложение для Mac, которое прошивает блок питания **[FNIRSI IPS3608](https://www.fnirsi.com/products/ips3608)**. **Ничего устанавливать не нужно:** ни Python, ни Homebrew, ни Терминал. Работает на Apple Silicon и Intel, macOS 11 и новее.

> ⚠️ **Используй USB-кабель, который был в коробке с IPS3608.** С обычными USB-C кабелями блок часто не определяется.

1. **Скачай приложение:** архив **IPS3608-Flasher-macOS.zip** на странице [Releases](https://github.com/sasha-nordiclab/ips3608-flash-macos/releases/latest). Распакуй его.
2. **Скачай прошивку** со [страницы загрузок FNIRSI](https://www.fnirsi.com/pages/software-downloads) и распакуй. Нужен файл вида `APP_IPS3608_V1.9_260429.bin`.
3. **Открой IPS3608 Flasher.app.** Приложение не нотаризовано Apple, поэтому в первый раз: **правый клик → Открыть → Открыть**. Если macOS не пускает, зайди в Системные настройки → Конфиденциальность и безопасность → **Всё равно открыть**.
4. **Переведи блок в режим прошивки.** Приложение тоже это подскажет:
   1. **Выключи** блок.
   2. **Зажми верхнюю кнопку** на передней панели.
   3. Не отпуская её, **включи питание**. Появится экран **Firmware Upgrade**.
   4. Подключи **задний USB-порт** к Mac **кабелем из коробки**.
5. Приложение найдёт блок. **Выбери файл `.bin`**, нажми **Да** и введи пароль от Mac.
6. **Заверши,** как скажет приложение:
   - *«Блок сам начал обновление»*: дождись перезагрузки.
   - *«Записана и проверена»*: **коротко нажми RUN/STOP** на блоке.

   Не отключай USB и питание, пока блок не перезагрузится. Потом проверь версию в настройках.

**Приложение всё ждёт блок?** Возьми кабель из коробки и проверь, что на экране Firmware Upgrade. Кнопку нужно зажать **до** включения питания.

**Ошибка?** Не нажимай RUN/STOP. Выключи блок, снова войди в режим прошивки и повтори.

---

## License

MIT. See [LICENSE](LICENSE). Not affiliated with FNIRSI. Firmware files belong to FNIRSI and are not part of this repository.
