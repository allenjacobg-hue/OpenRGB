/*---------------------------------------------------------*\
| HIDSafeModeScan.cpp                                       |
|                                                           |
|   HID safe-mode scan. Disabled specific detectors are     |
|   skipped before hid_enumerate.                           |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include "HIDSafeModeScan.h"

void RunHIDSafeModeSpecificDetectors(std::vector<HIDDeviceDetectorBlock>& detectors,
                                     const json& detector_settings,
                                     hid_safe_mode_enumerate_fn enumerate,
                                     hid_safe_mode_free_enumeration_fn free_enumeration,
                                     const HIDSafeModeScanHooks& hooks)
{
    /*-----------------------------------------------------*\
    | Loop through specific detectors. A detector whose     |
    | settings entry is false is skipped before any HID     |
    | enumeration. Missing entries stay enabled, matching   |
    | the other detector paths.                             |
    \*-----------------------------------------------------*/
    for(std::size_t hid_detector_idx = 0; hid_detector_idx < detectors.size(); hid_detector_idx++)
    {
        HIDDeviceDetectorBlock& detector = detectors[hid_detector_idx];

        bool this_device_enabled = true;

        if(detector_settings.contains("detectors") && detector_settings["detectors"].contains(detector.name))
        {
            this_device_enabled = detector_settings["detectors"][detector.name];
        }

        if(hooks.on_enabled_state)
        {
            hooks.on_enabled_state(detector.name, this_device_enabled);
        }

        /*-------------------------------------------------*\
        | Do not call hid_enumerate for a disabled          |
        | detector. The enabled check used to run only      |
        | after a compared device was already returned.     |
        \*-------------------------------------------------*/
        if(!this_device_enabled)
        {
            continue;
        }

        unsigned short enumerate_vid = (detector.vid == HID_VID_ANY) ? 0 : detector.vid;
        unsigned short enumerate_pid = (detector.pid == HID_PID_ANY) ? 0 : detector.pid;

        if(hooks.on_before_enumerate)
        {
            hooks.on_before_enumerate(detector, enumerate_vid, enumerate_pid);
        }

        hid_device_info* current_hid_device = enumerate(enumerate_vid, enumerate_pid);

        while(current_hid_device)
        {
            if(detector.compare(current_hid_device))
            {
                if(hooks.prepare_match)
                {
                    hooks.prepare_match(detector, current_hid_device);
                }

                DetectedControllers detected_controllers = detector.function(current_hid_device, detector.name);

                if(hooks.on_match)
                {
                    hooks.on_match(detector, current_hid_device, detected_controllers);
                }
            }

            current_hid_device = current_hid_device->next;

            if(hooks.on_enumerated_device)
            {
                hooks.on_enumerated_device(hid_detector_idx);
            }
        }

        free_enumeration(current_hid_device);
    }
}
