#!/usr/bin/env python3
"""Flash FNIRSI IPS3608 firmware from macOS.

The IPS3608 bootloader exposes a small FAT12 disk ("Msc iap", volume IPS3608)
with a 2048-byte block size. Copying the .bin in Finder does not trigger the
update reliably, and mtools cannot work on the raw device directly. This tool:

  1. finds exactly one IPS3608 bootloader disk and checks its geometry,
  2. reads the whole disk into an image with dd,
  3. adds the firmware to a copy of the image with mtools,
  4. writes back ONLY the 2048-byte sectors that changed,
  5. reads the device back and compares it byte-for-byte.

Usage:
  sudo python3 -I ips3608_flash.py APP_IPS3608_Vx.y_YYMMDD.bin
  sudo python3 -I ips3608_flash.py APP_IPS3608_Vx.y_YYMMDD.bin --dry-run
  sudo python3 -I ips3608_flash.py APP_IPS3608_Vx.y_YYMMDD.bin --yes

Requires: macOS, Python 3, mtools (brew install mtools).
"""
import argparse
import os
import plistlib
import shutil
import subprocess
import sys
import tempfile
import time

DISK_SIZE = 8386560      # bytes, whole bootloader disk
BLOCK = 2048             # device block size
MAX_CHANGED = 512        # refuse to write more sectors than this
MEDIA_NAME = "Msc iap"
VOLUME_NAME = "IPS3608"


def die(msg, code=1):
    print(f"ERROR: {msg}", file=sys.stderr)
    sys.exit(code)


def run(*args, **kw):
    return subprocess.run(args, check=True, capture_output=True, **kw)


def disk_info(dev):
    return plistlib.loads(run("diskutil", "info", "-plist", dev).stdout)


def find_device():
    listing = plistlib.loads(
        run("diskutil", "list", "-plist", "external", "physical").stdout)
    hits = []
    for disk in listing.get("WholeDisks", []):
        info = disk_info(disk)
        if info.get("MediaName", "").strip() == MEDIA_NAME:
            hits.append((disk, info))

    if not hits:
        die("IPS3608 bootloader disk not found.\n"
            "  Power the IPS3608 off, hold the UPPER button, power it on while\n"
            "  holding it (Firmware Upgrade screen), then connect the REAR USB\n"
            "  data port with a cable that carries data.")
    if len(hits) > 1:
        die(f"more than one '{MEDIA_NAME}' device connected; disconnect the others.")

    disk, info = hits[0]
    # Note: "Size" is the device size. "TotalSize"/"VolumeSize" are the FAT
    # filesystem size (8355840) and must not be used for this check.
    expected = {
        "BusProtocol": "USB",
        "Size": DISK_SIZE,
        "DeviceBlockSize": BLOCK,
        "VolumeName": VOLUME_NAME,
    }
    for key, want in expected.items():
        got = info.get(key)
        if got != want:
            die(f"unexpected {key} on /dev/{disk}: {got!r} (expected {want!r}).")
    fs = info.get("FilesystemName") or info.get("FilesystemUserVisibleName")
    if fs and "FAT12" not in fs:
        die(f"unexpected filesystem on /dev/{disk}: {fs!r} (expected FAT12).")

    print(f"Found IPS3608: /dev/{disk}")
    print(f"  Media:      {MEDIA_NAME}")
    print(f"  Volume:     {VOLUME_NAME}")
    print(f"  Size:       {DISK_SIZE} bytes")
    print(f"  Block size: {BLOCK} bytes")
    print(f"  Protocol:   USB")
    return disk


def read_image(raw, path):
    run("dd", f"if={raw}", f"of={path}", f"bs={BLOCK}")
    size = os.path.getsize(path)
    if size != DISK_SIZE:
        die(f"read {size} bytes from {raw}, expected {DISK_SIZE}.")


def main():
    ap = argparse.ArgumentParser(description="Flash FNIRSI IPS3608 firmware on macOS.")
    ap.add_argument("firmware", help="APP_IPS3608_*.bin")
    ap.add_argument("--dry-run", action="store_true",
                    help="prepare the image and show changed sectors, write nothing")
    ap.add_argument("--yes", action="store_true", help="skip the confirmation prompt")
    args = ap.parse_args()

    if sys.platform != "darwin":
        die("macOS only.")
    if os.geteuid() != 0:
        die("run this script with sudo.")
    if not (shutil.which("mcopy") and shutil.which("mdir")):
        die("mtools not found: brew install mtools")

    fw = os.path.abspath(args.firmware)
    name = os.path.basename(fw)
    if not os.path.isfile(fw):
        die(f"firmware not found: {fw}")
    if not (name.startswith("APP_IPS3608_") and name.lower().endswith(".bin")):
        die(f"firmware name must look like APP_IPS3608_*.bin, got {name!r}.")

    disk = find_device()
    raw = f"/dev/r{disk}"

    if not args.dry_run and not args.yes:
        if input(f"Write {name} to /dev/{disk}? [y/N] ").strip().lower() not in ("y", "yes"):
            die("cancelled.")

    run("diskutil", "unmountDisk", "force", f"/dev/{disk}")

    tmp = tempfile.mkdtemp(prefix="ips3608_")
    before = os.path.join(tmp, "before.img")
    after = os.path.join(tmp, "after.img")
    read_image(raw, before)
    shutil.copyfile(before, after)

    env = dict(os.environ, MTOOLS_SKIP_CHECK="1")
    run("mcopy", "-o", "-i", after, fw, "::/" + name, env=env)
    print(run("mdir", "-i", after, "::", env=env).stdout.decode(errors="replace"))

    with open(before, "rb") as f:
        old = f.read()
    with open(after, "rb") as f:
        new = f.read()
    changed = [n for n in range(DISK_SIZE // BLOCK)
               if old[n * BLOCK:(n + 1) * BLOCK] != new[n * BLOCK:(n + 1) * BLOCK]]
    print(f"Changed sectors: {len(changed)}")

    if not changed:
        die("no changes to write (firmware already on the disk?).")
    if len(changed) > MAX_CHANGED:
        die(f"{len(changed)} changed sectors exceeds safety limit {MAX_CHANGED}.")
    if args.dry_run:
        print(f"Dry run: nothing written. Images kept in {tmp}")
        run("diskutil", "mountDisk", f"/dev/{disk}")
        return

    fd = os.open(raw, os.O_RDWR | os.O_SYNC)
    try:
        for n in changed:
            os.lseek(fd, n * BLOCK, os.SEEK_SET)
            if os.write(fd, new[n * BLOCK:(n + 1) * BLOCK]) != BLOCK:
                die(f"short write at sector {n}. Do NOT press RUN/STOP.")
        os.fsync(fd)
    finally:
        os.close(fd)
    print(f"Wrote {len(changed)} sectors.")

    # Some units start flashing on their own as soon as the file lands and
    # drop off USB, so a missing device here is expected, not a failure.
    time.sleep(1)
    if not os.path.exists(raw):
        print("\nThe IPS3608 disk disappeared right after the write.")
        print("The bootloader most likely picked up the firmware and is flashing/rebooting.")
        print("Wait, then check the firmware version on the device screen.")
        return

    check = os.path.join(tmp, "check.img")
    try:
        read_image(raw, check)
    except subprocess.CalledProcessError:
        if not os.path.exists(raw):
            print("\nThe IPS3608 disk disappeared during verification.")
            print("The bootloader most likely started the update by itself.")
            print("Wait, then check the firmware version on the device screen.")
            return
        die("could not read the device back. Do NOT press RUN/STOP.")

    with open(check, "rb") as f:
        if f.read() != new:
            die("verification failed: device does not match the prepared image.\n"
                "Do NOT press RUN/STOP.")

    print("\nVerification successful: device matches prepared image byte-for-byte.")
    print("Now SHORT-PRESS RUN/STOP on the IPS3608 to start the firmware update.")
    print("Do not disconnect power or USB while the update is running.")


if __name__ == "__main__":
    main()
