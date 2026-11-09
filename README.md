# ips3608-flash-macos

Flash firmware to the **[FNIRSI IPS3608](https://www.fnirsi.com/products/ips3608)** bench power supply (36 V / 8 A / 285 W) from **macOS**.

FNIRSI's instructions say to drag the `.bin` onto the USB drive the power supply shows in upgrade mode. On macOS that often does nothing. This tool writes the firmware so the bootloader accepts it.

- Device: <https://www.fnirsi.com/products/ips3608>
- Firmware downloads: <https://www.fnirsi.com/pages/software-downloads>
- Write-up with background: <https://yakovlev.cc/articles/260818_fnirsi_ips3608_firmware_macos/>

> ⚠️ **Use the USB cable that came in the box with the IPS3608.** Ordinary USB-C cables often do not work. The power supply then never shows up as a disk, or does not show up at all.

> ⚠️ The tool writes raw sectors to a USB disk. Before writing anything it checks that the target is the IPS3608 bootloader disk. Do not disconnect USB or power during an update.

[По-русски ↓](#инструкция-по-русски)

---

## Step-by-step

### 1. Install the tools (once)

Install [Homebrew](https://brew.sh) if you don't have it, then mtools:

```sh
brew install mtools
```

Python 3 must be installed (check with `python3 --version`).

### 2. Get this tool

```sh
git clone https://github.com/sasha-nordiclab/ips3608-flash-macos.git
cd ips3608-flash-macos
```

Or download `ips3608_flash.py` from this page.

### 3. Get the firmware

Download the IPS3608 firmware from the [FNIRSI downloads page](https://www.fnirsi.com/pages/software-downloads) and unzip it. You need the file named like `APP_IPS3608_V1.9_260429.bin`. Firmware is not included in this repo.

### 4. Put the IPS3608 into Firmware Upgrade mode

1. Turn the power supply **off**.
2. **Press and hold the upper button** on the front panel.
3. While still holding it, **turn the power on**.
4. The **Firmware Upgrade** screen appears. You can release the button.
5. Connect the **rear USB data port** to the Mac with the **original FNIRSI cable from the box**.

### 5. Check that the Mac sees it

A disk named **IPS3608** should appear in Finder (it contains `Ready.TXT`). Or check in Terminal:

```sh
diskutil list external
```

You should see an 8.4 MB disk named `IPS3608`.

**No disk?**
- Make sure you used the cable from the box.
- Make sure the Firmware Upgrade screen is showing. If the unit is in normal mode, the Mac sees it as `AT32 Virtual Com Port` instead of a disk. Repeat step 4, and press the button **before** turning the power on.

### 6. Flash

```sh
sudo python3 -I ips3608_flash.py ~/Downloads/APP_IPS3608_V1.9_260429.bin
```

Enter your Mac password, then answer `y`.

The script finds the disk, checks it, and writes the firmware. To test everything without writing, add `--dry-run`.

### 7. Finish

One of two things happens:

- **The disk disappears right after the write.** This is normal: the power supply picked up the firmware and is flashing itself. Wait until it reboots.
- **`Verification successful`** is printed. **Short-press RUN/STOP** on the power supply to start the update.

Do not unplug USB or power until the device has rebooted. Then check the firmware version in the device's settings.

If the script prints `ERROR` before writing, nothing was changed. Fix the cause and run it again. If it prints `ERROR` with **"Do NOT press RUN/STOP"**, don't press it. Re-enter Firmware Upgrade mode and flash again.

---

## Why Finder copy fails

In upgrade mode the IPS3608 is a tiny FAT12 disk with a **2048-byte block size**:

| Property | Value |
|---|---|
| Media name | `Msc iap` |
| Volume | `IPS3608` (contains `Ready.TXT`) |
| File system | MS-DOS FAT12 |
| Disk size | 8 386 560 bytes |
| Device block size | 2048 bytes |
| USB name | `AT32 Mass Storage` (Artery AT32 MCU) |

- A Finder copy creates the file, but pressing RUN/STOP does nothing.
- `mcopy` straight on `/dev/rdiskN` fails with `plain_io read/write: Invalid argument`.

The script reads the whole disk into an image with `dd` and adds the file to a copy of the image with `mtools`. It then writes back **only the 2048-byte sectors that changed** and reads the device back to verify.

Safety checks before writing:
- exactly one `Msc iap` disk is connected;
- volume `IPS3608`, USB, `Size` = 8386560, block size 2048, FAT12;
- the firmware file name is `APP_IPS3608_*.bin`;
- at least 1 and at most 512 sectors changed.

## Tested

| Firmware | Mac | Result |
|---|---|---|
| V1.7 (260327) | Apple Silicon MacBook Air | OK, RUN/STOP started the update |
| V1.9 (260429) | Apple Silicon MacBook Air | OK, auto-flashed right after the write |

---

## Инструкция по-русски

Блок питания: [FNIRSI IPS3608](https://www.fnirsi.com/products/ips3608). Прошивки: [страница загрузок FNIRSI](https://www.fnirsi.com/pages/software-downloads).

> ⚠️ **Используй USB-кабель, который был в коробке с IPS3608.** С обычными USB-C кабелями блок часто не определяется как диск или не определяется вообще.

### 1. Установи инструменты (один раз)

Поставь [Homebrew](https://brew.sh), если его нет, затем mtools:

```sh
brew install mtools
```

Нужен Python 3 (проверка: `python3 --version`).

### 2. Скачай этот скрипт

```sh
git clone https://github.com/sasha-nordiclab/ips3608-flash-macos.git
cd ips3608-flash-macos
```

### 3. Скачай прошивку

Скачай прошивку IPS3608 со [страницы загрузок FNIRSI](https://www.fnirsi.com/pages/software-downloads) и распакуй архив. Нужен файл вида `APP_IPS3608_V1.9_260429.bin`.

### 4. Переведи блок в режим прошивки

1. **Выключи** блок питания.
2. **Зажми верхнюю кнопку** на передней панели.
3. Не отпуская её, **включи питание**.
4. Появится экран **Firmware Upgrade**. Кнопку можно отпустить.
5. Подключи **задний USB-порт** к Mac **кабелем из коробки**.

### 5. Проверь, что Mac видит блок

В Finder должен появиться диск **IPS3608** с файлом `Ready.TXT`. Или проверь в Терминале:

```sh
diskutil list external
```

**Диска нет?**
- Проверь, что подключён кабель из коробки.
- Проверь, что на экране Firmware Upgrade. Если блок в обычном режиме, Mac видит его как `AT32 Virtual Com Port`, а не как диск. Повтори шаг 4: кнопку нужно зажать **до** включения питания.

### 6. Прошей

```sh
sudo python3 -I ips3608_flash.py ~/Downloads/APP_IPS3608_V1.9_260429.bin
```

Введи пароль от Mac и ответь `y`. Чтобы проверить всё без записи, добавь `--dry-run`.

### 7. Заверши

- **Если диск пропал сразу после записи**, это нормально: блок сам подхватил прошивку и прошивается. Дождись перезагрузки.
- **Если появилась строка `Verification successful`**, коротко нажми **RUN/STOP** на блоке.

Не отключай USB и питание, пока блок не перезагрузится. Потом проверь версию прошивки в настройках.

Если появилась ошибка `ERROR` до записи, на блок ничего не записано: исправь причину и запусти снова. Если в ошибке написано **«Do NOT press RUN/STOP»**, не нажимай RUN/STOP. Заново войди в режим прошивки и повтори.

---

## License

MIT. See [LICENSE](LICENSE). Not affiliated with FNIRSI. Firmware files belong to FNIRSI and are not part of this repository.
