#!/bin/sh
# Build and run the HID safe-mode enumeration spy. No hardware, no HID library.
set -eu
cd "$(dirname "$0")/../.."

python3 tests/hid_safe_mode/extract_detector_names.py

g++ -std=c++17 -Wall -Wextra -Wno-unused-parameter \
    -I. \
    -Idependencies/json \
    -Idependencies/hidapi-hotplug-win/include \
    -Ihidapi_wrapper \
    -Ii2c_smbus \
    -IRGBController \
    -ISPDAccessor \
    -o /tmp/hid_safe_mode_enumeration_test \
    tests/hid_safe_mode/hid_safe_mode_enumeration_test.cpp \
    HIDSafeModeScan.cpp

/tmp/hid_safe_mode_enumeration_test experiment/OpenRGB.detectors.template.json

python3 - << 'PY'
import json
import pathlib
import subprocess
import sys
root = pathlib.Path(".").resolve()
subprocess.check_call([sys.executable, "tests/hid_safe_mode/extract_detector_names.py"])
text = (root / "experiment" / "OpenRGB.detectors.template.json").read_text(encoding="utf-8")
data = json.loads(text)
enabled = sorted(k for k, v in data["Detectors"]["detectors"].items() if v)
assert enabled == ["MSI Mystic Light MS_7D89", "Razer Goliathus Extended"]
assert data["Detectors"]["hid_safe_mode"] is True
assert data["Detectors"]["detectors"]["Corsair Scimitar Elite RGB"] is False
print("template re-extract matches the committed enablement")
PY
