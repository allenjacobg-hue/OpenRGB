# Windows x64 package

Hosted build succeeded. `OpenRGB.exe` was not started. No hardware was scanned.

| Item | Value |
| --- | --- |
| Run | https://github.com/allenjacobg-hue/OpenRGB/actions/runs/42388986602 |
| Artifact | https://github.com/allenjacobg-hue/OpenRGB/actions/runs/42388986602/artifacts/13581370496 |
| Artifact name | `OpenRGB-windows-x64-hid-safe` |
| Commit | `820a70a75254e4e660d0aa747ad26880c23e624d` |
| Baseline | `8e16de27858cf2be6af54a33ce003df74cb9fd3e` |
| Qt | 6.8.3 from `download.qt.io`, `win64_msvc2022_64`, plus `qt5compat` |
| Compiler image | `windows-2022`, Visual Studio 2022 Enterprise, dumpbin 14.44.35229.0 |
| Job | 2026-11-07 15:59:12Z to 16:09:45Z, conclusion success |

The Actions artifact is a zip that contains `OpenRGB-windows-x64-hid-safe.zip` and `OpenRGB-windows-x64-hid-safe.zip.sha256`. GitHub reports that wrapper digest as `sha256:0c7fa12dcd1f1f839d7e2c7e7886ff98ac477dd83979eb4fa6fd408e3cdbc3b3` (22,566,046 bytes).

The package zip inside it was downloaded and hashed again:

```
3552fd1a78a34f3e3aa8729f9fb4564837a539d08b56675a8bc126aa5e40b2a1  OpenRGB-windows-x64-hid-safe.zip
```

That matches the `.sha256` file produced by the job and the log line `ZIP_SHA256=3552fd1a78a34f3e3aa8729f9fb4564837a539d08b56675a8bc126aa5e40b2a1`.

`OpenRGB.exe` inside the package:

```
a1ccaf6c889b96f1ad567cde0b81f66860fb100bad38b3efff133aa0bf92eed0  OpenRGB Windows 64-bit/OpenRGB.exe
```

Static PE header, read without loading the image: signature `0x00004550`, machine `0x8664` (x64), magic `0x020B` (PE32+), subsystem 2 (Windows GUI). Size 10,550,272 bytes. It is the only `.exe` in the package.

`dumpbin /dependents` lists `Qt6Core.dll`, `Qt6Gui.dll`, `Qt6Widgets.dll`, `hidapi-hotplug.dll`, `libusb-1.0.dll`, `PawnIOLib.dll`, `MSVCP140.dll`, `VCRUNTIME140.dll`, and `VCRUNTIME140_1.dll`, plus Windows system DLLs. Those non-system imports are in the package, with `platforms\qwindows.dll`. Every file hash in `FILE-SHA256.txt` matched the downloaded bytes.

`separate-config\OpenRGB.json` has `hid_safe_mode` true and 1,937 detector names. The only true entries are `MSI Mystic Light MS_7D89` and `Razer Goliathus Extended`. `Corsair Scimitar Elite RGB` is false.

Mock HID test on the runner, before the OpenRGB link, printed:

```
hid safe-mode enumeration spy: ok
queries: 1111:2222 1532:c02 1462:7d89
matched: [Razer Goliathus Extended] [MSI Mystic Light MS_7D89]
```

That test does not show that a physical device enumerates or lights.

`dxcompiler.dll` and `dxil.dll` are in the folder and are not imports of `OpenRGB.exe`. `PawnIOLib.dll` and the `.bin` files are the runtime files the OpenRGB Windows project copies. There is no driver installer. None of these files were executed.

`BUILD-INFO.txt` records the compiler field as the `cl` usage line because `cl` with no source file prints usage. The toolset identity used for the link is the dumpbin version above.
