/*---------------------------------------------------------*\
| hid_safe_mode_enumeration_test.cpp                        |
|                                                           |
|   Spy test for HID safe-mode enumeration. The spy is a   |
|   local function. It does not open a HID device, write   |
|   lighting, or scan hardware.                             |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "HIDSafeModeScan.h"

using json = nlohmann::json;

namespace
{
struct EnumerateCall
{
    unsigned short vid;
    unsigned short pid;
};

std::vector<EnumerateCall>  enumerate_calls;
std::vector<hid_device_info*> free_calls;
std::vector<std::string>    matched_names;
int                         function_calls = 0;

hid_device_info razer_wrong;
hid_device_info razer_match;
hid_device_info msi_match;

void ResetSpy()
{
    enumerate_calls.clear();
    free_calls.clear();
    matched_names.clear();
    function_calls = 0;
    std::memset(&razer_wrong, 0, sizeof(razer_wrong));
    std::memset(&razer_match, 0, sizeof(razer_match));
    std::memset(&msi_match, 0, sizeof(msi_match));
}

hid_device_info* SpyEnumerate(unsigned short vendor_id, unsigned short product_id)
{
    enumerate_calls.push_back({vendor_id, product_id});

    /*-----------------------------------------------------*\
    | Fabricated info structs only. No hid_open, no report  |
    | transfer, no lighting write.                          |
    \*-----------------------------------------------------*/
    if((vendor_id == 0x1532) && (product_id == 0x0C02))
    {
        razer_wrong.path                = (char*)"spy-razer-wrong-usage";
        razer_wrong.vendor_id           = 0x1532;
        razer_wrong.product_id          = 0x0C02;
        razer_wrong.usage_page          = 0x0001;
        razer_wrong.usage               = 0x0006;
        razer_wrong.interface_number    = 0;
        razer_wrong.bus_type            = HID_API_BUS_USB;
        razer_wrong.next                = &razer_match;

        razer_match.path                = (char*)"spy-razer-match";
        razer_match.vendor_id           = 0x1532;
        razer_match.product_id          = 0x0C02;
        razer_match.usage_page          = 0x0001;
        razer_match.usage               = 0x0002;
        razer_match.interface_number    = 0;
        razer_match.bus_type            = HID_API_BUS_USB;
        razer_match.next                = nullptr;

        return &razer_wrong;
    }

    if((vendor_id == 0x1462) && (product_id == 0x7D89))
    {
        msi_match.path                  = (char*)"spy-msi-match";
        msi_match.vendor_id             = 0x1462;
        msi_match.product_id            = 0x7D89;
        msi_match.usage_page            = 0x0001;
        msi_match.usage                 = 0x0000;
        msi_match.interface_number      = 0;
        msi_match.bus_type              = HID_API_BUS_USB;
        msi_match.next                  = nullptr;

        return &msi_match;
    }

    return nullptr;
}

void SpyFree(hid_device_info* devices)
{
    free_calls.push_back(devices);
}

DetectedControllers RecordMatch(hid_device_info* /*device*/, const std::string& name)
{
    function_calls++;
    matched_names.push_back(name);
    return {};
}

HIDDeviceDetectorBlock MakeDetector(const std::string& name, int vid, int pid, int interface_number, int usage_page, int usage)
{
    HIDDeviceDetectorBlock detector;

    detector.name               = name;
    detector.vid                = vid;
    detector.pid                = pid;
    detector.interface          = interface_number;
    detector.usage_page         = usage_page;
    detector.usage              = usage;
    detector.bus                = HID_BUS_ANY;
    detector.enabled_by_default = true;
    detector.function           = RecordMatch;

    return detector;
}

bool Fail(const std::string& message)
{
    std::cerr << "FAIL: " << message << std::endl;
    return false;
}

bool CallsContain(unsigned short vid, unsigned short pid)
{
    for(std::size_t i = 0; i < enumerate_calls.size(); i++)
    {
        if((enumerate_calls[i].vid == vid) && (enumerate_calls[i].pid == pid))
        {
            return true;
        }
    }

    return false;
}

bool RunEnumerationSpy()
{
    ResetSpy();

    std::vector<HIDDeviceDetectorBlock> detectors;
    detectors.push_back(MakeDetector("Corsair Scimitar Elite RGB", 0x1B1C, 0x1B8B, 1, 0xFFC2, HID_USAGE_ANY));
    detectors.push_back(MakeDetector("Wildcard HID", HID_VID_ANY, HID_PID_ANY, HID_INTERFACE_ANY, HID_USAGE_PAGE_ANY, HID_USAGE_ANY));
    detectors.push_back(MakeDetector("Corsair Scimitar Elite RGB", 0x1B1C, 0x1BE3, 1, 0xFF42, HID_USAGE_ANY));
    detectors.push_back(MakeDetector("Probe Unlisted Detector", 0x1111, 0x2222, HID_INTERFACE_ANY, HID_USAGE_PAGE_ANY, HID_USAGE_ANY));
    detectors.push_back(MakeDetector("Corsair K95 RGB", 0x1B1C, 0x1B11, 1, 0xFFC2, HID_USAGE_ANY));
    detectors.push_back(MakeDetector("Razer Goliathus Extended", 0x1532, 0x0C02, 0x00, 0x01, 0x02));
    detectors.push_back(MakeDetector("MSI Mystic Light MS_7D89", 0x1462, 0x7D89, HID_INTERFACE_ANY, 0x0001, 0x00));
    detectors.push_back(MakeDetector("Razer Goliathus", 0x1532, 0x0C01, 0x00, 0x01, 0x02));

    json settings;
    settings["hid_safe_mode"] = true;
    settings["detectors"]["Corsair Scimitar Elite RGB"] = false;
    settings["detectors"]["Wildcard HID"] = false;
    settings["detectors"]["Corsair K95 RGB"] = false;
    settings["detectors"]["Razer Goliathus Extended"] = true;
    settings["detectors"]["MSI Mystic Light MS_7D89"] = true;
    settings["detectors"]["Razer Goliathus"] = false;

    HIDSafeModeScanHooks hooks;
    int progress_steps = 0;
    hooks.on_enumerated_device = [&progress_steps](std::size_t) { progress_steps++; };

    RunHIDSafeModeSpecificDetectors(detectors, settings, SpyEnumerate, SpyFree, hooks);

    for(std::size_t i = 0; i < enumerate_calls.size(); i++)
    {
        if(enumerate_calls[i].vid == 0x1B1C)
        {
            return Fail("disabled Corsair vendor 1b1c was queried");
        }

        if((enumerate_calls[i].vid == 0) && (enumerate_calls[i].pid == 0))
        {
            return Fail("safe mode issued wildcard hid_enumerate(0, 0)");
        }
    }

    if(enumerate_calls.size() != 3)
    {
        return Fail("expected exactly the unlisted probe, the pad, and the MSI detector to be queried");
    }

    if(!CallsContain(0x1111, 0x2222) || !CallsContain(0x1532, 0x0C02) || !CallsContain(0x1462, 0x7D89))
    {
        return Fail("an enabled detector VID/PID was not queried");
    }

    if((enumerate_calls[0].vid != 0x1111) || (enumerate_calls[0].pid != 0x2222)
    || (enumerate_calls[1].vid != 0x1532) || (enumerate_calls[1].pid != 0x0C02)
    || (enumerate_calls[2].vid != 0x1462) || (enumerate_calls[2].pid != 0x7D89))
    {
        return Fail("enabled queries were not in detector order");
    }

    if(free_calls.size() != enumerate_calls.size())
    {
        return Fail("free was not paired with each enabled enumeration");
    }

    if(function_calls != 2)
    {
        return Fail("enabled matching detectors did not run, or a non-match ran");
    }

    if((matched_names.size() != 2)
    || (matched_names[0] != "Razer Goliathus Extended")
    || (matched_names[1] != "MSI Mystic Light MS_7D89"))
    {
        return Fail("detector function ran for the wrong device");
    }

    if(progress_steps != 3)
    {
        return Fail("progress advanced for a detector that was not enumerated");
    }

    return true;
}

bool RunTemplateCheck(const std::string& path)
{
    json settings;

    try
    {
        std::ifstream input(path);
        if(!input)
        {
            return Fail("could not open config template " + path);
        }
        input >> settings;
    }
    catch(const std::exception& ex)
    {
        return Fail(std::string("config template is not JSON: ") + ex.what());
    }

    if(!settings.contains("Detectors") || !settings["Detectors"].is_object())
    {
        return Fail("template is missing the Detectors object");
    }

    const json& detectors_settings = settings["Detectors"];

    if(!detectors_settings.contains("hid_safe_mode") || detectors_settings["hid_safe_mode"] != true)
    {
        return Fail("template hid_safe_mode is not true");
    }

    if(!detectors_settings.contains("detectors") || !detectors_settings["detectors"].is_object())
    {
        return Fail("template is missing detectors");
    }

    int enabled_count = 0;
    bool saw_pad = false;
    bool saw_msi = false;
    bool saw_scimitar = false;

    for(json::const_iterator it = detectors_settings["detectors"].begin(); it != detectors_settings["detectors"].end(); ++it)
    {
        if(!it.value().is_boolean())
        {
            return Fail("detector value is not a boolean: " + it.key());
        }

        if(it.key().find('@') != std::string::npos)
        {
            return Fail("detector name contains an identifier marker");
        }

        if(it.value() == true)
        {
            enabled_count++;
            if(it.key() == "Razer Goliathus Extended")
            {
                saw_pad = true;
            }
            else if(it.key() == "MSI Mystic Light MS_7D89")
            {
                saw_msi = true;
            }
            else
            {
                return Fail("unexpected enabled detector: " + it.key());
            }
        }

        if(it.key() == "Corsair Scimitar Elite RGB")
        {
            saw_scimitar = true;
            if(it.value() != false)
            {
                return Fail("Corsair Scimitar Elite RGB is not disabled");
            }
        }
    }

    if(!saw_pad || !saw_msi || !saw_scimitar || (enabled_count != 2))
    {
        return Fail("template enablement does not match the verified pad and MSI detectors");
    }

    return true;
}
}

int main(int argc, char** argv)
{
    const std::string template_path = (argc > 1) ? argv[1] : "experiment/OpenRGB.detectors.template.json";

    if(!RunEnumerationSpy())
    {
        return EXIT_FAILURE;
    }

    if(!RunTemplateCheck(template_path))
    {
        return EXIT_FAILURE;
    }

    std::cout << "hid safe-mode enumeration spy: ok" << std::endl;
    std::cout << "queries:";
    for(std::size_t i = 0; i < enumerate_calls.size(); i++)
    {
        std::cout << " " << std::hex << enumerate_calls[i].vid << ":" << enumerate_calls[i].pid;
    }
    std::cout << std::dec << std::endl;
    std::cout << "matched:";
    for(std::size_t i = 0; i < matched_names.size(); i++)
    {
        std::cout << " [" << matched_names[i] << "]";
    }
    std::cout << std::endl;

    return EXIT_SUCCESS;
}
