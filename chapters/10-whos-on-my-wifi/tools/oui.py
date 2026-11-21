"""Writes ../src/scanner/oui.h: who made a device, from the first half of its MAC address.

Every network card's MAC address starts with a three-byte prefix (an OUI) that the IEEE assigns to
its maker. The full public list has over 38,000 prefixes and would take most of the board's flash,
so this keeps only the makers you are likely to find in a home: phones, computers, routers, TVs,
speakers, consoles, printers, smart plugs. That is still several thousand prefixes, packed four
bytes each.

Source: the IEEE MA-L public listing, https://standards-oui.ieee.org/oui/oui.csv
Run:    python3 tools/oui.py path/to/oui.csv
"""
import csv
import pathlib
import re
import sys

# (pattern on the registered organisation name, the short name shown on the phone)
MAKERS = [
    (r"\bapple\b", "Apple"),
    (r"samsung", "Samsung"),
    (r"\bgoogle\b", "Google"),
    (r"amazon", "Amazon"),
    (r"tp-?link", "TP-Link"),
    (r"xiaomi|beijing xiaomi", "Xiaomi"),
    (r"huawei", "Huawei"),
    (r"honor device", "Honor"),
    (r"oneplus", "OnePlus"),
    (r"guangdong oppo|\boppo\b", "OPPO"),
    (r"vivo mobile", "vivo"),
    (r"realme", "realme"),
    (r"motorola", "Motorola"),
    (r"nokia", "Nokia"),
    (r"sony", "Sony"),
    (r"\blg electronics|lg innotek", "LG"),
    (r"microsoft", "Microsoft"),
    (r"intel corporate|intel corp", "Intel"),
    (r"realtek", "Realtek"),
    (r"mediatek", "MediaTek"),
    (r"qualcomm|atheros", "Qualcomm"),
    (r"broadcom", "Broadcom"),
    (r"^dell\b", "Dell"),
    (r"hewlett|^hp inc", "HP"),
    (r"lenovo", "Lenovo"),
    (r"asustek|\basus", "ASUS"),
    (r"^acer\b", "Acer"),
    (r"micro-star|^msi\b", "MSI"),
    (r"gigabyte|giga-byte", "Gigabyte"),
    (r"liteon|lite-on", "Lite-On"),
    (r"azurewave", "AzureWave"),
    (r"hon hai|foxconn", "Foxconn"),
    (r"murata", "Murata"),
    (r"chicony", "Chicony"),
    (r"netgear", "Netgear"),
    (r"d-link", "D-Link"),
    (r"ubiquiti", "Ubiquiti"),
    (r"cisco|linksys", "Cisco"),
    (r"mikrotik|routerboard", "MikroTik"),
    (r"zyxel", "Zyxel"),
    (r"^arris", "Arris"),
    (r"technicolor", "Technicolor"),
    (r"sagemcom", "Sagemcom"),
    (r"zte\b|zte corp", "ZTE"),
    (r"^tenda|shenzhen tenda", "Tenda"),
    (r"mercusys|mercury communication", "Mercusys"),
    (r"^eero", "eero"),
    (r"avm gmbh", "AVM"),
    (r"espressif", "Espressif"),
    (r"raspberry", "Raspberry Pi"),
    (r"tuya", "Tuya"),
    (r"shelly|allterco", "Shelly"),
    (r"signify|philips lighting", "Philips Hue"),
    (r"philips", "Philips"),
    (r"sonos", "Sonos"),
    (r"roku", "Roku"),
    (r"nintendo", "Nintendo"),
    (r"^valve corp", "Valve"),
    (r"\bcanon\b", "Canon"),
    (r"seiko epson|\bepson", "Epson"),
    (r"brother industries", "Brother"),
    (r"xerox", "Xerox"),
    (r"kyocera", "Kyocera"),
    (r"panasonic", "Panasonic"),
    (r"sharp corp", "Sharp"),
    (r"hisense", "Hisense"),
    (r"tcl\b|tcl king|tcl corp", "TCL"),
    (r"vizio", "Vizio"),
    (r"skyworth", "Skyworth"),
    (r"^bose corp", "Bose"),
    (r"harman", "Harman"),
    (r"jbl", "JBL"),
    (r"fitbit", "Fitbit"),
    (r"garmin", "Garmin"),
    (r"hikvision", "Hikvision"),
    (r"dahua", "Dahua"),
    (r"wyze", "Wyze"),
    (r"^ring llc", "Ring"),
    (r"^arlo", "Arlo"),
    (r"nest labs", "Google Nest"),
    (r"ecobee", "ecobee"),
    (r"synology", "Synology"),
    (r"qnap", "QNAP"),
    (r"western digital", "Western Digital"),
    (r"nvidia", "NVIDIA"),
    (r"vmware", "VMware"),
    (r"texas instruments", "Texas Instruments"),
    (r"silicon lab", "Silicon Labs"),
    (r"nordic semi", "Nordic"),
    (r"amlogic", "Amlogic"),
    (r"rockchip", "Rockchip"),
    (r"ampak", "AMPAK"),
    (r"universal global scientific|ugsi", "USI"),
    (r"wistron", "Wistron"),
    (r"compal", "Compal"),
    (r"pegatron", "Pegatron"),
    (r"quanta", "Quanta"),
    (r"reliance jio|\bjio\b", "Jio"),
    (r"airtel|bharti", "Airtel"),
    (r"fiberhome", "FiberHome"),
    (r"nothing technology", "Nothing"),
    (r"transsion|tecno mobile|infinix", "Transsion"),
    (r"lava international", "Lava"),
    (r"micromax", "Micromax"),
    (r"dyson", "Dyson"),
    (r"irobot", "iRobot"),
    (r"withings", "Withings"),
    (r"meta platforms|facebook|oculus", "Meta"),
    (r"peloton", "Peloton"),
    (r"belkin|wemo", "Belkin"),
    (r"lifx", "LIFX"),
    (r"yeelight|qingdao yeelink", "Yeelight"),
    (r"ezviz", "EZVIZ"),
    (r"^imou", "Imou"),
    (r"blackberry|research in motion", "BlackBerry"),
    (r"htc corp", "HTC"),
    (r"zebra tech", "Zebra"),
    (r"shenzhen bilian|bilian", "Bilian"),
]
OUT = pathlib.Path(__file__).resolve().parent.parent / "src" / "scanner" / "oui.h"


def main(path):
    rules = [(re.compile(p, re.I), n) for p, n in MAKERS]
    names, idx, rows = [], {}, {}
    with open(path, newline="", encoding="utf-8", errors="replace") as f:
        for r in csv.DictReader(f):
            if r["Registry"] != "MA-L":
                continue
            org = r["Organization Name"]
            for rx, name in rules:
                if rx.search(org):
                    if name not in idx:
                        idx[name] = len(names)
                        names.append(name)
                    rows[int(r["Assignment"], 16)] = idx[name]
                    break
    assert len(names) < 256
    keys = sorted(rows)
    lines = ["#pragma once",
             "// GENERATED by tools/oui.py from the IEEE MA-L public listing — who made a network card,",
             "// from the first three bytes of its MAC address. %d prefixes from %d makers." % (len(keys), len(names)),
             "// Each entry is (prefix << 8) | maker, sorted, so the board can binary-search it.",
             "#include <stdint.h>",
             "#ifndef PROGMEM",
             "#define PROGMEM",
             "#endif",
             "static const char *const OUI_MAKERS[] = {"]
    for i in range(0, len(names), 8):
        lines.append("  " + ", ".join('"%s"' % n for n in names[i:i + 8]) + ",")
    lines += ["};", "static const uint32_t OUI_TABLE[] PROGMEM = {"]
    vals = ["0x%08x" % ((k << 8) | rows[k]) for k in keys]
    for i in range(0, len(vals), 8):
        lines.append("  " + ",".join(vals[i:i + 8]) + ",")
    lines += ["};", "static const uint32_t OUI_COUNT = %d;" % len(keys), ""]
    OUT.write_text("\n".join(lines))
    print("wrote %s: %d prefixes, %d makers, %d KB" % (OUT.name, len(keys), len(names), len(keys) * 4 // 1024))


if __name__ == "__main__":
    main(sys.argv[1])
