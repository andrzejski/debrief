#!/usr/bin/env python3
"""Gathers Debrief.exe and everything it needs to run into one folder.

What windeployqt does on Windows, done from Linux: the QML modules the window
imports, the Qt plugins it loads, and then every DLL any of those name, found
by reading their import tables until nothing new turns up. A DLL that is
neither in the Qt folder, the compiler's runtime, nor part of Windows stops
the packaging, since on the user's machine it would stop the app.
"""

import argparse
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path

# Qt plugins the app loads: the window system, the icon and screenshot
# formats, the notification-area menu's look, and audio devices.
PLUGINS = [
    "platforms/qwindows.dll",
    "imageformats/qsvg.dll",
    "imageformats/qjpeg.dll",
    "imageformats/qico.dll",
    "iconengines/qsvgicon.dll",
    "styles/qmodernwindowsstyle.dll",
    "multimedia/windowsmediaplugin.dll",
]

# Qt Quick Controls styles the window doesn't use (it is Material throughout).
UNUSED_QML = re.compile(r"^QtQuick/(Controls/(Fusion|Imagine|Universal|FluentWinUI3|Windows)(/|$)|NativeStyle)")

# Part of Windows 10 and 11.
SYSTEM = re.compile(
    r"^(api-ms-win-.*|ext-ms-.*|kernel32|user32|gdi32|shell32|advapi32|ole32|oleaut32|ws2_32|winmm|imm32|version"
    r"|shlwapi|comdlg32|comctl32|uxtheme|dwmapi|d3d9|d3d11|d3d12|dxgi|dwrite|d2d1|opengl32|netapi32|userenv"
    r"|iphlpapi|dnsapi|crypt32|bcrypt|ncrypt|secur32|mpr|wtsapi32|setupapi|shcore|authz|winhttp|rpcrt4|msvcrt"
    r"|ntdll|powrprof|propsys|mf|mfplat|mfreadwrite|mfuuid|mfplay|evr|avrt|mmdevapi|ksuser|wmcodecdspuuid"
    r"|msdmo|strmiids|dxva2|uiautomationcore|winspool|cfgmgr32|psapi|dbghelp|d3dcompiler_47|dcomp|normaliz"
    r"|wintrust|msimg32|oleacc|usp10|ucrtbase|synchronization|bcryptprimitives|wldap32|mswsock|kernelbase"
    r"|comsvcs|virtdisk|pdh|wlanapi|winusb|hid|credui|urlmon|wininet|iertutil|gdiplus|wevtapi|mscms"
    r"|coremessaging|windowsapp|runtimeobject|combase|sspicli|cryptbase|dsound|mfsensorgroup|quartz"
    r"|avicap32|msvfw32|msacm32|audioses|devobj|shdocvw|ninput|inputhost|textinputframework)\.(dll|drv)$",
    re.IGNORECASE,
)


def imports(objdump, binary):
    listing = subprocess.run([objdump, "-p", str(binary)], capture_output=True, text=True, check=True).stdout
    return re.findall(r"DLL Name: (\S+)", listing)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", required=True, type=Path)
    parser.add_argument("--qt", required=True, type=Path, help="Qt for Windows (bin/, plugins/, qml/)")
    parser.add_argument("--runtime", required=True, type=Path, help="folder with libc++.dll and libunwind.dll")
    parser.add_argument("--objdump", required=True)
    parser.add_argument("--importscanner", required=True)
    parser.add_argument("--qml", required=True, type=Path, help="the app's QML files")
    parser.add_argument("--out", required=True, type=Path)
    args = parser.parse_args()

    out = args.out
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)
    shutil.copy2(args.exe, out / args.exe.name)

    for plugin in PLUGINS:
        target = out / plugin
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(args.qt / "plugins" / plugin, target)

    scan = subprocess.run(
        [args.importscanner, "-qmlFiles", *map(str, sorted(args.qml.glob("*.qml"))), "-importPath",
         str(args.qt / "qml")],
        capture_output=True, text=True, check=True).stdout
    modules = sorted({m["relativePath"] for m in json.loads(scan) if m.get("type") == "module" and m.get("relativePath")})
    for module in modules:
        if UNUSED_QML.match(module):
            continue
        source, target = args.qt / "qml" / module, out / "qml" / module
        target.mkdir(parents=True, exist_ok=True)
        for item in source.iterdir():
            # A folder with a qmldir is a module of its own, copied if imported.
            if item.is_dir():
                if not (item / "qmldir").exists():
                    shutil.copytree(item, target / item.name, dirs_exist_ok=True)
            elif item.suffix not in (".pdb", ".debug"):
                shutil.copy2(item, target / item.name)

    # Every DLL anything here names, and what those name in turn.
    places = [args.qt / "bin", args.runtime]
    seen, missing = set(), []
    queue = [p for p in out.rglob("*") if p.suffix.lower() in (".exe", ".dll")]
    while queue:
        binary = queue.pop()
        for name in imports(args.objdump, binary):
            key = name.lower()
            if key in seen or SYSTEM.match(name):
                continue
            seen.add(key)
            found = next((place / name for place in places if (place / name).exists()), None)
            if not found:
                missing.append(f"{name} (needed by {binary.relative_to(out)})")
                continue
            shutil.copy2(found, out / name)
            queue.append(out / name)
    if missing:
        sys.exit("These aren't in Qt, the compiler runtime, or Windows:\n  " + "\n  ".join(missing))

    # Qt Quick compiles its shaders with it; Windows has its own copy, and
    # Qt's is shipped beside the app the way windeployqt does.
    if (args.qt / "d3dcompiler_47.dll").exists():
        shutil.copy2(args.qt / "d3dcompiler_47.dll", out / "d3dcompiler_47.dll")

    files = [p for p in out.rglob("*") if p.is_file()]
    print(f"{out}: {len(files)} files, {sum(p.stat().st_size for p in files) / 1e6:.0f} MB")


if __name__ == "__main__":
    main()
