"""Build every project for every device and assemble the flasher site.

    python workbench/build.py [out-dir]          # default: site/

Reads workbench/projects.json, compiles each project's sketch with arduino-cli for each device,
and writes:

    site/index.html, app.js, style.css        the flasher page (copied from workbench/)
    site/projects.json                        project and device metadata (copied)
    site/firmware/<device>/<project>.bin      the merged flash image, written at 0x0, with the
                                              empty tail trimmed (see trim())
    site/firmware/manifest.json               what was built, for which device, and how big

A project that fails to compile for a device is recorded in the manifest with its first error
line, not treated as fatal: the page shows it as unavailable for that device.

Extra arduino-cli build properties can be passed in ARDUINO_EXTRA, one per line. On an Apple
Silicon Mac without Rosetta the bundled ctags will not run, and

    ARDUINO_EXTRA=$'tools.ctags.cmd.path=/usr/bin/true\\ntools.ctags.path=/usr/bin'

skips it (every sketch here declares its functions before use). CI on Linux needs nothing.
"""

import datetime
import json
import os
import pathlib
import shutil
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
HERE = ROOT / "workbench"
PAGE_FILES = ["index.html", "app.js", "style.css", "projects.json"]


def git(*args):
    try:
        return subprocess.run(["git", *args], cwd=ROOT, capture_output=True, text=True,
                              check=True).stdout.strip()
    except (subprocess.CalledProcessError, FileNotFoundError):
        return ""


def trim(image):
    """Drop the erased tail of a merged image.

    arduino-cli pads the merged image to the whole 4 MB flash with 0xFF — the value erased flash
    already reads as. Most of a 4 MB file is that padding, so it is cut off, keeping the length
    a whole number of 4 KB flash sectors. The trimmed tail is simply not written.

    Saved settings are a different matter: the NVS partition sits near the start, inside the
    image, so cutting the tail does not spare it. The flasher page does that, by reading the
    image's partition table and writing around NVS (keepSettings() in app.js).
    """
    end = len(image.rstrip(b"\xff"))
    end = (end + 4095) // 4096 * 4096
    return image[:end]


def compile_one(sketch, fqbn, outdir, extra):
    cmd = ["arduino-cli", "compile", "--fqbn", fqbn, "--output-dir", str(outdir)]
    for prop in extra:
        cmd += ["--build-property", prop]
    cmd.append(str(sketch))
    run = subprocess.run(cmd, capture_output=True, text=True)
    log = run.stdout + run.stderr
    if run.returncode != 0:
        first = next((l.strip() for l in log.splitlines()
                      if "error:" in l or "too big" in l.lower()), "compile failed")
        return None, first
    merged = list(outdir.glob("*.merged.bin"))
    if not merged:
        return None, "no merged.bin produced"
    return merged[0], None


def main():
    out = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else ROOT / "site").resolve()
    meta = json.loads((HERE / "projects.json").read_text())
    extra = [p for p in os.environ.get("ARDUINO_EXTRA", "").splitlines() if p.strip()]

    if out.exists():
        shutil.rmtree(out)
    (out / "firmware").mkdir(parents=True)
    for name in PAGE_FILES:
        shutil.copy(HERE / name, out / name)

    manifest = {
        "built": datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "commit": git("rev-parse", "--short", "HEAD"),
        "core": "esp32:esp32@3.3.11",
        "firmware": {},
    }
    failures = 0
    with tempfile.TemporaryDirectory() as tmp:
        for dev in meta["devices"]:
            (out / "firmware" / dev["id"]).mkdir()
            manifest["firmware"][dev["id"]] = {}
            for proj in meta["projects"]:
                fqbn = dev["fqbn"]
                if proj.get("partition", "default") != "default":
                    fqbn += ":PartitionScheme=" + proj["partition"]
                built, err = compile_one(ROOT / proj["sketch"], fqbn,
                                         pathlib.Path(tmp) / dev["id"] / proj["id"], extra)
                entry = {"ok": built is not None}
                if built:
                    dest = out / "firmware" / dev["id"] / (proj["id"] + ".bin")
                    dest.write_bytes(trim(built.read_bytes()))
                    entry["size"] = dest.stat().st_size
                    entry["file"] = f"firmware/{dev['id']}/{proj['id']}.bin"
                else:
                    entry["error"] = err
                    failures += 1
                manifest["firmware"][dev["id"]][proj["id"]] = entry
                print(f"{'ok  ' if built else 'FAIL'} {dev['id']:8} {proj['id']:20} "
                      f"{entry.get('size', err)}", flush=True)

    (out / "firmware" / "manifest.json").write_text(json.dumps(manifest, indent=1))
    print(f"\nsite written to {out} — {failures} combination(s) did not build")
    # Every tested-device build must succeed; untested devices may fail without failing CI.
    tested = {d["id"] for d in meta["devices"] if d.get("tested")}
    broken = [f"{d}/{p}" for d in tested for p, e in manifest["firmware"][d].items() if not e["ok"]]
    if broken:
        print("tested device failed to build: " + ", ".join(broken), file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
