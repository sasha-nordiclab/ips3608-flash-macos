# ips3608flash: FNIRSI IPS3608 firmware flasher for macOS

Flash firmware to the **[FNIRSI IPS3608](https://www.fnirsi.com/products/ips3608)** bench power supply (36 V / 8 A / 285 W) from a Mac.

**One small program, nothing to install:** no Python, no Homebrew. Works on Apple Silicon and Intel Macs (macOS 11+).

- Device: <https://www.fnirsi.com/products/ips3608>
- Firmware downloads: <https://www.fnirsi.com/pages/software-downloads>
- Background write-up: <https://yakovlev.cc/articles/260818_fnirsi_ips3608_firmware_macos/>

> ⚠️ **Use the USB cable that came in the box with the IPS3608.** Ordinary USB-C cables often do not work. The power supply then never shows up on the Mac.

[По-русски ↓](#инструкция-по-русски)

---

## How to flash

**1. Download the firmware** from the [FNIRSI downloads page](https://www.fnirsi.com/pages/software-downloads) and unzip it. You need the file named like `APP_IPS3608_V1.9_260429.bin`.

**2. Put the power supply into Firmware Upgrade mode:**
1. Turn the IPS3608 **off**.
2. **Press and hold the upper button** on the front panel.
3. While holding it, **turn the power on**. The **Firmware Upgrade** screen appears.
4. Connect the **rear USB port** to the Mac with the **cable from the box**.

A disk named **IPS3608** appears in Finder.

**3. Open Terminal** (Spotlight → "Terminal") and download the flasher:

```sh
curl -L -o ips3608flash https://github.com/sasha-nordiclab/ips3608-flash-macos/releases/latest/download/ips3608flash
chmod +x ips3608flash
```

**4. Flash.** Type `sudo ./ips3608flash ` (with a space at the end), **drag the `.bin` file from Finder into the Terminal window**, and press Enter:

```sh
sudo ./ips3608flash ~/Downloads/APP_IPS3608_V1.9_260429.bin
```

Enter your Mac password (nothing shows while you type, that's normal), then answer `y`.

**5. Finish**, as the program tells you:
- *"updating by itself"*: wait for the power supply to reboot.
- *"written and verified"*: **short-press RUN/STOP** on the power supply.

Do not disconnect USB or power until it reboots. Then check the version in the device's settings.

### Problems

| Message | What to do |
|---|---|
| `IPS3608 not found` | Use the cable from the box. Check that the screen shows Firmware Upgrade. Press the button **before** turning the power on. |
| `already on the device` | That firmware is already there. Turn off, re-enter upgrade mode, retry. |
| `Operation not permitted` | System Settings → Privacy & Security → Full Disk Access → turn on **Terminal**, then retry. |
| any `ERROR` with **DO NOT PRESS RUN/STOP** | Don't press it. Turn off, re-enter upgrade mode, flash again. |

To check everything without writing: `sudo ./ips3608flash --dry-run file.bin`.

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

`ips3608flash` (plain C, [src/ips3608flash.c](src/ips3608flash.c)):

1. finds exactly one disk with media name `Msc iap` and checks block size 2048 and size 8386560, refusing to touch anything else;
2. unmounts it and reads the whole disk into memory;
3. adds the firmware to a copy (FAT12 long name + 8.3 entry, cluster chain in every FAT). The result is byte-identical to `mcopy` from mtools, verified on the real device;
4. writes back **only the 2048-byte sectors that changed** (80 for V1.9) and refuses to write more than 512;
5. reads the device back and compares byte-for-byte. Some units start flashing as soon as the file lands and leave USB; that is reported as success.

## Build from source

Needs Xcode Command Line Tools (`xcode-select --install`):

```sh
./build.sh      # -> build/ips3608flash (universal)
```

## Tested

| Firmware | Mac | Result |
|---|---|---|
| V1.7 (260327) | Apple Silicon MacBook Air | OK, RUN/STOP started the update |
| V1.9 (260429) | Apple Silicon MacBook Air | OK, auto-flashed right after the write |

---

## Инструкция по-русски

Программа прошивает блок питания **[FNIRSI IPS3608](https://www.fnirsi.com/products/ips3608)** с Mac. **Ничего устанавливать не нужно:** ни Python, ни Homebrew. Работает на Apple Silicon и Intel, macOS 11 и новее.

> ⚠️ **Используй USB-кабель, который был в коробке с IPS3608.** С обычными USB-C кабелями блок часто не определяется.

**1. Скачай прошивку** со [страницы загрузок FNIRSI](https://www.fnirsi.com/pages/software-downloads) и распакуй. Нужен файл вида `APP_IPS3608_V1.9_260429.bin`.

**2. Переведи блок в режим прошивки:**
1. **Выключи** блок.
2. **Зажми верхнюю кнопку** на передней панели.
3. Не отпуская её, **включи питание**. Появится экран **Firmware Upgrade**.
4. Подключи **задний USB-порт** к Mac **кабелем из коробки**.

В Finder появится диск **IPS3608**.

**3. Открой Терминал** (Spotlight → «Терминал») и скачай программу:

```sh
curl -L -o ips3608flash https://github.com/sasha-nordiclab/ips3608-flash-macos/releases/latest/download/ips3608flash
chmod +x ips3608flash
```

**4. Прошей.** Напечатай `sudo ./ips3608flash ` (с пробелом в конце), **перетащи файл `.bin` из Finder в окно Терминала** и нажми Enter:

```sh
sudo ./ips3608flash ~/Downloads/APP_IPS3608_V1.9_260429.bin
```

Введи пароль от Mac (при вводе символы не видны, это нормально) и ответь `y`.

**5. Заверши,** как скажет программа:
- *«updating by itself»*: блок прошивается сам, дождись перезагрузки.
- *«written and verified»*: **коротко нажми RUN/STOP** на блоке.

Не отключай USB и питание, пока блок не перезагрузится. Потом проверь версию в настройках.

### Если что-то не так

| Сообщение | Что делать |
|---|---|
| `IPS3608 not found` | Возьми кабель из коробки. Проверь, что на экране Firmware Upgrade. Кнопку нужно зажать **до** включения питания. |
| `already on the device` | Эта прошивка уже на блоке. Выключи блок, снова войди в режим прошивки и повтори. |
| `Operation not permitted` | Системные настройки → Конфиденциальность и безопасность → Полный доступ к диску → включи **Терминал**, затем повтори. |
| любая `ERROR` с **DO NOT PRESS RUN/STOP** | Не нажимай RUN/STOP. Выключи блок, снова войди в режим прошивки и прошей заново. |

Проверить всё без записи: `sudo ./ips3608flash --dry-run файл.bin`.

---

## License

MIT. See [LICENSE](LICENSE). Not affiliated with FNIRSI. Firmware files belong to FNIRSI and are not part of this repository.
