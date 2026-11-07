# Separate OpenRGB HID safe-mode experiment

This branch is a local experiment on the fork. It is not an upstream submission. The installed OpenRGB binary and its settings were not modified here, and this work did not launch OpenRGB against hardware.

## Baseline

| Item | Value |
| --- | --- |
| Parent commit | `8e16de27858cf2be6af54a33ce003df74cb9fd3e` |
| Commit date | 2026-10-16 16:37:19 -0500 |
| Tag on that commit | `release_1.1` |
| Subject | OpenRGB version 1.1 |
| Version in `OpenRGB.pro` | 1.1 |
| hidapi header in tree | `dependencies/hidapi-hotplug-win/include/hidapi.h` reports 0.15.0 |
| Windows build script for this tree | `scripts/build-windows.bat 6.8.3 2022 64` (Qt 6.8.3, MSVC 2022, x64) |

Later commits on `master` are not part of this branch. The safe-mode HID loop itself is unchanged between this parent and current `master`; other detection code on `master` has moved.

## License

OpenRGB is under the GNU General Public License, version 2, in `LICENSE`. Source headers in this tree, including the files added here, use SPDX `GPL-2.0-or-later`. This experiment is a derivative of that code and stays under the same license. No extra license is added.

## What the baseline code does

`DetectionManager::BackgroundDetectDevices` reads `hid_safe_mode` from the `Detectors` settings object.

When `hid_safe_mode` is false, the next HID step is one `hid_enumerate(0, 0)`. Per-detector enablement is checked later, while walking that list. A disabled flag cannot skip that wildcard call. This patch does not change that path.

When `hid_safe_mode` is true, that wildcard call is not made. `BackgroundDetectHIDDevicesSafe` walks `hid_specific_detectors` instead. On the baseline, each entry logged `Trying to run detector for [<name>] (for vid:pid)` and then called `hid_enumerate` for that detector's VID and PID. The disabled flag was read only after `hid_enumerate` returned a device that `compare` accepted. A detector set to false in settings still caused a HID enumeration. Generic detectors (`HID_VID_ANY` and `HID_PID_ANY`) are not in that safe-mode list.

`WaitForDetection` stops the caller after `DETECTION_WAIT_TIMEOUT_MS` (60000) and logs that it gave up. It does not cancel the background detection thread. A 60 second wait is that caller bound. It is not a timeout inside `hid_enumerate`, and it is not a stack trace.

The reported last log line, `Trying to run detector Corsair Scimitar Elite RGB (1b1c:1b8b)`, is the verbose line the baseline prints immediately before `hid_enumerate(0x1B1C, 0x1B8B)`. That order is consistent with the next call being that enumeration. Nothing in this change captures a stack, so that device is still a hypothesis for where a call was stuck, not proof of a lock inside the detector or inside hidapi.

Two registrations share the settings name `Corsair Scimitar Elite RGB`:

- `Controllers/CorsairPeripheralController/CorsairPeripheralControllerDetect.cpp` VID `0x1B1C`, PID `0x1B8B`
- `Controllers/CorsairPeripheralV2Controller/CorsairPeripheralV2ControllerDetect.cpp` VID `0x1B1C`, PID `0x1BE3`

Settings are keyed by name, so one `false` covers both. On the baseline both still enumerated before the flag was read.

## Change

`RunHIDSafeModeSpecificDetectors` reads the same settings flag first. If the entry is false, it does not call `hid_enumerate` or `hid_free_enumeration` for that detector. If the entry is missing, the detector stays enabled, which is the existing default. An enabled detector still enumerates its own VID and PID (`HID_VID_ANY` / `HID_PID_ANY` still become 0, as before), compares each returned `hid_device_info`, and runs the detector function only on a compare match. Registration, progress, and the verbose `Trying to run detector` log happen only for detectors that are actually enumerated.

No detector implementation, driver, firmware, PawnIO module, or lighting write path was changed.

The safe-mode loop still passes the walked enumeration pointer to `hid_free_enumeration` after the list has been advanced to its end. That matches the baseline and is left as it was.

## Verified detector names

There is no registered detector named `Razer Goliathus Extended Chroma` or `MSI MPG Z790 CARBON WIFI`.

The pad registration is `Razer Goliathus Extended` in `Controllers/RazerController/RazerControllerDetect.cpp`, `REGISTER_HID_DETECTOR_IPU`, VID `0x1532` (`RAZER_VID`), PID `0x0C02` (`RAZER_GOLIATHUS_CHROMA_EXTENDED_PID`), interface 0, usage page `0x01`, usage `0x02`. `Razer Goliathus` (`0x0C01`) and `Razer Goliathus Chroma 3XL` (`0x0C06`) are different detectors and are left disabled in the template.

The motherboard registration is `MSI Mystic Light MS_7D89` in `Controllers/MSIMotherboardController/MSIMotherboardControllerDetect.cpp`, `REGISTER_HID_DETECTOR_PU`, VID `0x1462` (`MSI_USB_VID`), PID `0x7D89`, usage page `0x0001`, usage `0x00`. The zone table in `MSIMotherboard185Controller.cpp` maps USB product id `0x7D89` to the comment `MPG Z790 CARBON WIFI`. That string is not a settings key. `0x7D89` appears only on that detector and that zone row in this baseline.

`MSI-RGB` is a different detector. It probes Super IO and only lists older board ids. `7D89` is not in that list. The template disables `MSI-RGB`.

## I2C and bus discovery are separate

HID safe mode does not replace bus discovery. `BackgroundDetectI2CBuses` always runs before HID detection. On Windows x64 the bus detectors are `i2c_smbus_amdadl_detect`, `i2c_smbus_nvapi_detect`, and `i2c_smbus_pawnio_detect`. They are `REGISTER_I2C_BUS_DETECTOR` entries with no settings name, so the template cannot turn them off. Named I2C, DRAM, and I2C-PCI detectors are in the template and set false, so those detector functions are not called. The Z790 Carbon path used here is the USB HID detector above, not an I2C device detector.

`UpdateDetectorSettings` adds any missing static detector name using `enabled_by_default` (usually true) and drops names that are not registered. The template therefore lists every static Windows x64 detector name found in this baseline, with only the two names above set true. QMK `REGISTER_DYNAMIC_HID_DETECTOR_*` names are created at runtime from attached devices and are not in this static list. A later OpenRGB build that adds detectors will treat those new names as enabled unless the template is regenerated.

Regenerate with `python3 tests/hid_safe_mode/extract_detector_names.py`. This baseline produced 2132 Windows x64 registrations and 1937 unique names.

## Separate config

`experiment/OpenRGB.detectors.template.json` is a generic template. It has no machine names, paths, logs, or account identifiers.

Use it only with a separate directory and the separate build:

1. Create a new empty directory. Do not use the installed OpenRGB configuration directory, and do not replace the installed `OpenRGB.json`.
2. Copy the template into that directory and name the copy `OpenRGB.json`.
3. Start the separate build with `--config <that directory>`. `--config` selects a directory, not a file. `--localconfig` uses the current working directory instead of the installed one.

`hid_safe_mode` is true and `initial_detection_delay_ms` is 0. Do not launch that build against hardware until a coordinated test. This repository work did not do that launch.

## Test

`tests/hid_safe_mode/hid_safe_mode_enumeration_test.cpp` links `HIDSafeModeScan.cpp` and passes a local enumerate function. The spy does not call hidapi, does not open a device, and does not write lighting.

The case puts disabled Corsair blocks at VID `0x1B1C` (PIDs `0x1B8B`, `0x1BE3`, and another Corsair name) ahead of the enabled detectors, plus a disabled `HID_VID_ANY` / `HID_PID_ANY` block that would have become `hid_enumerate(0, 0)`. The spy recorded three queries, in order: the unlisted default-enabled probe `1111:2222`, `Razer Goliathus Extended` `1532:c02`, and `MSI Mystic Light MS_7D89` `1462:7d89`. Vendor `0x1B1C` was not queried. `(0, 0)` was not queried. The pad and MSI detector functions ran once each. A non-matching interface on the pad enumeration did not run the function.

Run `sh tests/hid_safe_mode/run_test.sh` from the repository root. It was run on this Linux environment with g++ 13.3.0. That produces `/tmp/hid_safe_mode_enumeration_test`, which is a Linux ELF used only for the spy. It is not a Windows OpenRGB executable.

## Windows x64 build

A local Linux compile cannot produce this executable. The hosted build is `.github/workflows/windows-x64-experiment.yml` on `allenjacobg-hue/OpenRGB` only. It runs on `windows-2022` and refuses any other repository. It does not launch `OpenRGB.exe`.

Toolchain, all from the publishers' public archives rather than a patched OpenRGB binary:

- Visual Studio 2022 already installed on the GitHub `windows-2022` image. `scripts/build-windows.bat` looks for Build Tools under `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools`. The hosted image provides Enterprise (or another full product) instead, so the workflow calls that image's `vcvarsall.bat x64` and then the same `qmake`, `jom`, and `windeployqt` steps as the batch script.
- Qt 6.8.3 `win64_msvc2022_64` plus the `qt5compat` module, downloaded by pinned `aqtinstall==3.3.0` from `https://download.qt.io`. `qmake -v` must report 6.8.3 or the job fails.
- jom 1.1.7 from `https://download.qt.io/official_releases/jom/jom_1_1_7.zip`.

`windeployqt` uses the same flags as `scripts/build-windows.bat`, including `--no-compiler-runtime`. The package then copies the MSVC x64 CRT DLLs from that Visual Studio installation's redistributable directory so the EXE does not depend on a separate runtime installer. No driver installer is added. `PawnIOLib.dll` and the `.bin` modules are only the files `OpenRGB.pro` already copies into the official Windows build output. They are not installed or loaded by this job.

The job also compiles and runs `hid_safe_mode_enumeration_test.exe`. That test uses the mock enumerator. It is not `OpenRGB.exe`.

The zip and its SHA256 are uploaded as the Actions artifact `OpenRGB-windows-x64-hid-safe`. The result, including the real hash and download URL, is recorded in `experiment/WINDOWS_BUILD_RESULT.md` after the run. A commit that only changes that file or this provenance file does not start another build.

No issue, comment, or pull request was opened against the OpenRGB upstream project.
