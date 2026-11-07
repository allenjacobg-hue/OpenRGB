OpenRGB Windows x64, separate HID safe-mode experiment
=========================================================

This folder is a separate build. It is not a replacement for an
already installed OpenRGB, and it has not been run against hardware.

Do not copy these files over an existing OpenRGB installation.
Do not replace the installed OpenRGB.json.
Do not start this OpenRGB.exe until a coordinated hardware test.

The executable is a normal Windows x64 PE built from this fork.
It does not include a driver installer. PawnIOLib.dll and the .bin
files beside it are the same runtime files the OpenRGB Windows build
copies. They were not installed or loaded while producing this package.

Separate configuration
----------------------
separate-config\OpenRGB.json is the safe-mode template.
hid_safe_mode is true. The only enabled detector names are
"Razer Goliathus Extended" and "MSI Mystic Light MS_7D89".

When a coordinated test is ready, create an empty directory that is
not the installed OpenRGB configuration directory, copy
separate-config\OpenRGB.json into it under the name OpenRGB.json,
and start this build with:

    OpenRGB.exe --config <that directory>

--config takes a directory, not a file.

I2C/SMBus bus discovery still runs. It is not controlled by the
detector list. See PROVENANCE.md.

What the compile-time test shows
--------------------------------
The mock HID test does not open a device and does not write lighting.
It does not show that a physical controller enumerates or lights.
