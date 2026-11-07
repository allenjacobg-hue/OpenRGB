/*---------------------------------------------------------*\
| HIDSafeModeScan.h                                         |
|                                                           |
|   HID safe-mode scan. Disabled specific detectors are     |
|   skipped before hid_enumerate.                           |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <functional>
#include <string>
#include <vector>
#include "DetectionManager.h"

/*---------------------------------------------------------*\
| Callbacks used by the safe-mode scan. Empty callbacks     |
| are not invoked. enumerate and free_enumeration are       |
| required and are the only HID entry points this scan      |
| uses.                                                     |
\*---------------------------------------------------------*/
struct HIDSafeModeScanHooks
{
    std::function<void(const std::string& name, bool enabled)>                                      on_enabled_state;
    std::function<void(const HIDDeviceDetectorBlock& detector, unsigned short vid, unsigned short pid)> on_before_enumerate;
    std::function<void(HIDDeviceDetectorBlock& detector, hid_device_info* device)>                                prepare_match;
    std::function<void(HIDDeviceDetectorBlock& detector, hid_device_info* device, DetectedControllers& detected)> on_match;
    std::function<void(std::size_t detector_index)>                                                 on_enumerated_device;
};

typedef hid_device_info* (*hid_safe_mode_enumerate_fn)(unsigned short vendor_id, unsigned short product_id);
typedef void             (*hid_safe_mode_free_enumeration_fn)(hid_device_info* devices);

void RunHIDSafeModeSpecificDetectors(std::vector<HIDDeviceDetectorBlock>& detectors,
                                     const json& detector_settings,
                                     hid_safe_mode_enumerate_fn enumerate,
                                     hid_safe_mode_free_enumeration_fn free_enumeration,
                                     const HIDSafeModeScanHooks& hooks);
