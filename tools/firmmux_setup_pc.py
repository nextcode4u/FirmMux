#!/usr/bin/env python3
import argparse
import json
import os
import shutil
import subprocess
import sys
import tempfile
import urllib.request
import zipfile
from pathlib import Path
from typing import Dict, List, Optional, Tuple

FIRMUX_REPO_API = "https://api.github.com/repos/nextcode4u/FirmMux/releases/latest"
FIRMUX_ZIP_NAME = "FirmMux-SD.zip"
PATHFILE_REPO_API = "https://api.github.com/repos/nextcode4u/Pathfile-Mod-3DS/releases/latest"

DEP_TASKS = {
    "bootstrap_cia": (
        "YANBF bootstrap.cia",
        "https://github.com/YANBForwarder/YANBF/releases/latest/download/bootstrap.cia",
        "cias/bootstrap.cia",
    ),
    "ntr_launcher_zip": (
        "NTR Launcher zip",
        "https://github.com/ApacheThunder/NTR_Launcher/releases/latest/download/NTR_Launcher.zip",
        "3ds/FirmMux/deps/NTR_Launcher.zip",
    ),
    "ntr_forwarder_7z": (
        "NTR Forwarder pack",
        "https://github.com/RocketRobz/NTR_Forwarder/releases/latest/download/DS.Game.Forwarder.pack.nds-bootstrap.7z",
        "3ds/FirmMux/deps/DS.Game.Forwarder.pack.nds-bootstrap.7z",
    ),
    "retroarch_7z": (
        "RetroArch 3DSX package",
        "https://buildbot.libretro.com/nightly/nintendo/3ds/RetroArch_3dsx.7z",
        "3ds/FirmMux/deps/RetroArch_3dsx.7z",
    ),
}


def print_line(msg: str = "") -> None:
    print(msg, flush=True)


def safe_exists(path: Path) -> bool:
    try:
        return path.exists()
    except OSError:
        return False


def pause() -> None:
    try:
        input("\nPress Enter to exit...")
    except EOFError:
        pass


def ask_yes_no(prompt: str, default_yes: bool = True) -> bool:
    suffix = " [Y/n]: " if default_yes else " [y/N]: "
    while True:
        v = input(prompt + suffix).strip().lower()
        if not v:
            return default_yes
        if v in ("y", "yes"):
            return True
        if v in ("n", "no"):
            return False
        print_line("Please answer y or n.")


def looks_like_sd_root(path: Path) -> bool:
    return safe_exists(path / "3ds") or safe_exists(path / "roms")


def find_drive_roots() -> List[Path]:
    roots: List[Path] = []
    if os.name == "nt":
        for letter in "DEFGHIJKLMNOPQRSTUVWXYZ":
            p = Path(f"{letter}:\\")
            if safe_exists(p):
                roots.append(p)
    else:
        for base in (Path("/media"), Path("/run/media"), Path("/mnt")):
            if not base.exists():
                continue
            for child in base.rglob("*"):
                if child.is_dir() and looks_like_sd_root(child):
                    roots.append(child)
    seen = set()
    out: List[Path] = []
    for p in roots:
        try:
            rp = p.resolve()
        except OSError:
            continue
        if str(rp) in seen:
            continue
        seen.add(str(rp))
        out.append(rp)
    return out


def choose_sd_root() -> Path:
    print_line("FirmMux PC Setup")
    print_line("----------------")
    print_line("Select your SD card root.\n")
    drives = find_drive_roots()
    idx_map: Dict[str, Path] = {}
    n = 1
    for d in drives:
        marks = []
        if safe_exists(d / "3ds"):
            marks.append("3ds")
        if safe_exists(d / "roms"):
            marks.append("roms")
        mark = f" ({', '.join(marks)})" if marks else ""
        print_line(f"{n}. {d}{mark}")
        idx_map[str(n)] = d
        n += 1
    print_line("M. Manual path")
    while True:
        sel = input("Choose drive/path: ").strip()
        if sel.lower() == "m":
            raw = input("Enter SD root path: ").strip().strip('"')
            p = Path(raw)
            if safe_exists(p):
                return p.resolve()
            print_line("Path not found.")
            continue
        if sel in idx_map:
            return idx_map[sel]
        print_line("Invalid selection.")


def choose_mode(default_mode: Optional[str] = None) -> str:
    default_num = {"full": "1", "firmmux": "2", "deps": "3"}.get(default_mode, "1")
    print_line("\nSelect action:")
    print_line("1. Install FirmMux + dependencies")
    print_line("2. Update FirmMux only")
    print_line("3. Download dependencies only")
    print_line(f"Suggested default: {default_num}")
    while True:
        sel = input(f"Choose 1/2/3 [default {default_num}]: ").strip()
        if not sel:
            sel = default_num
        if sel == "1":
            return "full"
        if sel == "2":
            return "firmmux"
        if sel == "3":
            return "deps"
        print_line("Invalid selection.")


def parse_health_check(sd_root: Path) -> Dict[str, str]:
    out: Dict[str, str] = {}
    health = sd_root / "3ds" / "FirmMux" / "logs" / "health_check.txt"
    if not safe_exists(health):
        return out
    try:
        for line in health.read_text(encoding="utf-8", errors="ignore").splitlines():
            line = line.strip()
            if not line or "=" not in line:
                continue
            k, v = line.split("=", 1)
            out[k.strip()] = v.strip()
    except OSError:
        return {}
    return out


def print_health_summary(sd_root: Path) -> None:
    data = parse_health_check(sd_root)
    print_line("\nHealth check report:")
    if not data:
        print_line("- No health_check.txt found yet (launch FirmMux once to generate it).")
        return

    def is_ok(key: str) -> Optional[bool]:
        if key not in data:
            return None
        return data.get(key) == "1"

    checks = [
        ("retroarch_folder", "RetroArch data folder"),
        ("ntr_forwarder", "NTR Forwarder package"),
        ("dspfirm", "DSP firmware"),
    ]
    # Backward compatibility with older health_check.txt keys.
    if "ntr_forwarder" not in data and "nds_bootstrap" in data:
        data["ntr_forwarder"] = data.get("nds_bootstrap", "0")
    missing_items = 0
    for key, label in checks:
        ok = is_ok(key)
        if ok is True:
            print_line(f"- {label}: OK")
        elif ok is False:
            print_line(f"- {label}: MISSING")
            missing_items += 1
        else:
            print_line(f"- {label}: unknown")
    if "missing" in data:
        print_line(f"- Missing count (from FirmMux): {data['missing']}")
    if missing_items > 0:
        print_line("- Recommendation: run option 1 (Install FirmMux + dependencies) or option 3 (Dependencies only).")
    else:
        print_line("- Recommendation: use option 2 (Update FirmMux only) unless you want to refresh dependencies.")


def suggested_mode_from_health(sd_root: Path) -> str:
    data = parse_health_check(sd_root)
    if not data:
        return "full"

    def present(key: str) -> bool:
        return data.get(key) == "1"

    have_retro_folder = present("retroarch_folder")
    have_ntr_forwarder = present("ntr_forwarder")
    if "ntr_forwarder" not in data and "nds_bootstrap" in data:
        have_ntr_forwarder = present("nds_bootstrap")
    have_dsp = present("dspfirm")
    all_ok = have_retro_folder and have_ntr_forwarder and have_dsp
    if all_ok:
        return "firmmux"

    # If only DSP is missing, deps download won't fix it; default to FirmMux update.
    only_dsp_missing = have_retro_folder and have_ntr_forwarder and not have_dsp
    if only_dsp_missing:
        return "firmmux"

    # If core dependencies are missing, default to dependencies mode.
    deps_missing = (not have_retro_folder) or (not have_ntr_forwarder)
    if deps_missing:
        return "deps"

    return "full"


def files_equal(a: Path, b: Path) -> bool:
    try:
        if a.stat().st_size != b.stat().st_size:
            return False
        with a.open("rb") as fa, b.open("rb") as fb:
            while True:
                ba = fa.read(65536)
                bb = fb.read(65536)
                if ba != bb:
                    return False
                if not ba:
                    return True
    except OSError:
        return False


def copy_file_if_changed(src: Path, dst: Path) -> bool:
    dst.parent.mkdir(parents=True, exist_ok=True)
    if safe_exists(dst) and files_equal(src, dst):
        return False
    shutil.copy2(src, dst)
    return True


def download(url: str, out_path: Path) -> bool:
    out_path.parent.mkdir(parents=True, exist_ok=True)
    req = urllib.request.Request(url, headers={"User-Agent": "FirmMux-Setup-PC/1.0"})
    with urllib.request.urlopen(req, timeout=60) as r:
        code = getattr(r, "status", 200)
        if code < 200 or code >= 300:
            raise RuntimeError(f"HTTP {code} for {url}")
        tmp = out_path.with_suffix(out_path.suffix + ".tmp")
        with tmp.open("wb") as f:
            shutil.copyfileobj(r, f)
        if safe_exists(out_path) and files_equal(tmp, out_path):
            tmp.unlink(missing_ok=True)
            return False
        tmp.replace(out_path)
        return True


def fetch_latest_firmux_asset_url() -> Tuple[str, str]:
    req = urllib.request.Request(FIRMUX_REPO_API, headers={"User-Agent": "FirmMux-Setup-PC/1.0"})
    with urllib.request.urlopen(req, timeout=30) as r:
        if getattr(r, "status", 200) < 200 or getattr(r, "status", 200) >= 300:
            raise RuntimeError("Could not read latest FirmMux release")
        data = json.loads(r.read().decode("utf-8"))
    tag = data.get("tag_name", "latest")
    assets = data.get("assets", [])
    exact = None
    fallback = None
    for a in assets:
        name = str(a.get("name", ""))
        url = str(a.get("browser_download_url", ""))
        if not url:
            continue
        if name == FIRMUX_ZIP_NAME:
            exact = url
            break
        if name.lower().endswith(".zip") and "sd" in name.lower() and fallback is None:
            fallback = url
    if exact:
        return exact, tag
    if fallback:
        return fallback, tag
    raise RuntimeError("No SD zip asset found on latest FirmMux release")


def fetch_latest_release_asset_url(repo_api: str, preferred_name_substr: Optional[str] = None) -> Tuple[str, str]:
    req = urllib.request.Request(repo_api, headers={"User-Agent": "FirmMux-Setup-PC/1.0"})
    with urllib.request.urlopen(req, timeout=30) as r:
        if getattr(r, "status", 200) < 200 or getattr(r, "status", 200) >= 300:
            raise RuntimeError(f"Could not read latest release: {repo_api}")
        data = json.loads(r.read().decode("utf-8"))
    tag = data.get("tag_name", "latest")
    assets = data.get("assets", [])

    def is_archive(name: str) -> bool:
        n = name.lower()
        return n.endswith(".zip") or n.endswith(".7z")

    exact = None
    preferred = None
    fallback = None
    for a in assets:
        name = str(a.get("name", ""))
        url = str(a.get("browser_download_url", ""))
        if not url or not is_archive(name):
            continue
        lname = name.lower()
        if preferred_name_substr and preferred_name_substr.lower() in lname:
            preferred = url
            break
        if "sd" in lname and fallback is None:
            fallback = url
        if exact is None:
            exact = url

    if preferred:
        return preferred, tag
    if fallback:
        return fallback, tag
    if exact:
        return exact, tag
    raise RuntimeError(f"No archive asset found on latest release: {repo_api}")


def copy_tree_contents(src: Path, dst: Path) -> None:
    dst.mkdir(parents=True, exist_ok=True)
    for item in src.iterdir():
        target = dst / item.name
        if item.is_dir():
            if target.exists():
                copy_tree_contents(item, target)
            else:
                shutil.copytree(item, target)
        else:
            copy_file_if_changed(item, target)


def apply_extracted_sd_layout(extract_root: Path, sd_root: Path) -> None:
    if (extract_root / "3ds").is_dir() or (extract_root / "roms").is_dir() or (extract_root / "retroarch").is_dir():
        copy_tree_contents(extract_root, sd_root)
        return
    children = [p for p in extract_root.iterdir() if p.is_dir()]
    if len(children) == 1:
        c = children[0]
        if (c / "3ds").is_dir() or (c / "roms").is_dir() or (c / "retroarch").is_dir():
            copy_tree_contents(c, sd_root)
            return
    copy_tree_contents(extract_root, sd_root)


def find_first_file_case_insensitive(root: Path, target_name: str) -> Optional[Path]:
    target = target_name.lower()
    for p in root.rglob("*"):
        if p.is_file() and p.name.lower() == target:
            return p
    return None


def find_dir_named_case_insensitive(root: Path, target_name: str) -> Optional[Path]:
    target = target_name.lower()
    for p in root.rglob("*"):
        if p.is_dir() and p.name.lower() == target:
            return p
    return None


def stage_retroarch_dependencies_only(extract_root: Path, sd_root: Path) -> Dict[str, str]:
    out: Dict[str, str] = {}
    three_ds = sd_root / "3ds"
    three_ds.mkdir(parents=True, exist_ok=True)

    ra_3dsx = find_first_file_case_insensitive(extract_root, "retroarch.3dsx")
    if not ra_3dsx:
        raise RuntimeError("RetroArch package missing retroarch.3dsx")
    dst_3dsx = three_ds / "retroarch.3dsx"
    copy_file_if_changed(ra_3dsx, dst_3dsx)
    out["retroarch_3dsx"] = str(dst_3dsx)

    ra_smdh = find_first_file_case_insensitive(extract_root, "retroarch.smdh")
    if not ra_smdh:
        ra_smdh = find_first_file_case_insensitive(extract_root, "retroarch.smds")
    if ra_smdh:
        dst_smdh = three_ds / "retroarch.smdh"
        copy_file_if_changed(ra_smdh, dst_smdh)
        out["retroarch_smdh"] = str(dst_smdh)

    ra_data = find_dir_named_case_insensitive(extract_root, "retroarch")
    if not ra_data:
        raise RuntimeError("RetroArch package missing /retroarch folder")
    dst_data = sd_root / "retroarch"
    copy_tree_contents(ra_data, dst_data)
    out["retroarch_data"] = str(dst_data)

    return out


def extract_zip_to_sd(zip_path: Path, sd_root: Path) -> None:
    with tempfile.TemporaryDirectory(prefix="firmmux_release_") as td:
        tmp = Path(td)
        with zipfile.ZipFile(zip_path, "r") as zf:
            zf.extractall(tmp)
        if (tmp / "SD").is_dir():
            copy_tree_contents(tmp / "SD", sd_root)
            return
        if (tmp / "3ds").is_dir() or (tmp / "roms").is_dir():
            copy_tree_contents(tmp, sd_root)
            return
        children = [p for p in tmp.iterdir() if p.is_dir()]
        if len(children) == 1:
            c = children[0]
            if (c / "SD").is_dir():
                copy_tree_contents(c / "SD", sd_root)
                return
            if (c / "3ds").is_dir() or (c / "roms").is_dir():
                copy_tree_contents(c, sd_root)
                return
        raise RuntimeError("Could not detect SD root layout in FirmMux zip")


def extract_archive_to_tmp(archive_path: Path, out_dir: Path) -> None:
    name = archive_path.name.lower()
    if name.endswith(".zip"):
        out_dir.mkdir(parents=True, exist_ok=True)
        with zipfile.ZipFile(archive_path, "r") as zf:
            zf.extractall(out_dir)
        return
    if name.endswith(".7z"):
        extract_7z(archive_path, out_dir)
        return
    raise RuntimeError(f"Unsupported archive type: {archive_path.name}")


def stage_pathfile_standalone_package(sd_root: Path) -> Dict[str, str]:
    print_line("- Fetching latest Pathfile standalone package...")
    url, tag = fetch_latest_release_asset_url(PATHFILE_REPO_API, preferred_name_substr="sd")
    asset_name = Path(url).name
    pkg_out = sd_root / "3ds" / "FirmMux" / "deps" / asset_name
    updated = download(url, pkg_out)
    print_line(f"- Standalone package: {asset_name} ({tag}) [{ 'updated' if updated else 'up-to-date' }]")

    with tempfile.TemporaryDirectory(prefix="firmmux_pathfile_") as td:
        tmp = Path(td)
        extract_archive_to_tmp(pkg_out, tmp)
        apply_extracted_sd_layout(tmp, sd_root)

    return {
        "pathfile_release": tag,
        "pathfile_asset": str(pkg_out),
        "pathfile_marker": str(sd_root / "3ds" / "emulators" / "pathfile"),
    }


def extract_zip_cia(zip_path: Path, out_cia: Path) -> bool:
    with zipfile.ZipFile(zip_path, "r") as zf:
        for n in zf.namelist():
            if n.lower().endswith("ntr_launcher.cia"):
                out_cia.parent.mkdir(parents=True, exist_ok=True)
                tmp = out_cia.with_suffix(out_cia.suffix + ".tmp")
                with zf.open(n) as src, tmp.open("wb") as dst:
                    shutil.copyfileobj(src, dst)
                if safe_exists(out_cia) and files_equal(tmp, out_cia):
                    tmp.unlink(missing_ok=True)
                    return True
                tmp.replace(out_cia)
                return True
    return False


def have_py7zr() -> bool:
    try:
        import py7zr  # noqa: F401
        return True
    except Exception:
        return False


def try_install_py7zr() -> bool:
    cmd = [sys.executable, "-m", "pip", "install", "--user", "py7zr"]
    try:
        cp = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        if cp.returncode == 0:
            return have_py7zr()
    except Exception:
        return False
    return False


def extract_7z_py7zr(archive: Path, out_dir: Path) -> None:
    import py7zr
    out_dir.mkdir(parents=True, exist_ok=True)
    with py7zr.SevenZipFile(archive, mode="r") as z:
        z.extractall(path=out_dir)


def extract_7z_cmd(archive: Path, out_dir: Path) -> bool:
    out_dir.mkdir(parents=True, exist_ok=True)
    cmds = (["7z", "x", str(archive), f"-o{out_dir}", "-y"], ["7za", "x", str(archive), f"-o{out_dir}", "-y"])
    for cmd in cmds:
        try:
            cp = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
            if cp.returncode == 0:
                return True
        except FileNotFoundError:
            continue
    return False


def extract_7z(archive: Path, out_dir: Path) -> None:
    if have_py7zr():
        extract_7z_py7zr(archive, out_dir)
        return
    if extract_7z_cmd(archive, out_dir):
        return
    print_line("")
    print_line("7z support not found.")
    print_line("Required to extract dependency packages.")
    if ask_yes_no("Install py7zr automatically now?", default_yes=True):
        if try_install_py7zr():
            print_line("py7zr installed. Retrying extraction...")
            extract_7z_py7zr(archive, out_dir)
            return
        print_line("Auto-install failed.")
    print_line(f"Manual fix: {sys.executable} -m pip install --user py7zr")
    raise RuntimeError("7z extraction unavailable. Install py7zr or 7z/7za.")


def pick_forwarder_root(extract_root: Path) -> Optional[Path]:
    found: List[Path] = []
    for p in extract_root.rglob("*"):
        if p.is_dir() and "for sd card root" in p.name.lower():
            found.append(p)
    if not found:
        return None
    found.sort(key=lambda p: len(str(p)))
    return found[0]


def install_or_update_firmux(sd_root: Path) -> Dict[str, str]:
    deps_dir = sd_root / "3ds" / "FirmMux" / "deps"
    deps_dir.mkdir(parents=True, exist_ok=True)
    out_zip = deps_dir / FIRMUX_ZIP_NAME
    print_line("\nFetching latest FirmMux release...")
    url, tag = fetch_latest_firmux_asset_url()
    print_line(f"- Latest tag: {tag}")
    print_line(f"- Downloading: {Path(url).name}")
    updated = download(url, out_zip)
    print_line(f"- SD package is {'updated' if updated else 'up-to-date'}")
    print_line("- Applying FirmMux SD package...")
    extract_zip_to_sd(out_zip, sd_root)
    sync_autoboot_active_if_enabled(sd_root)
    return {"firmmux_release": tag, "firmmux_zip": str(out_zip)}


def sync_autoboot_active_if_enabled(sd_root: Path) -> None:
    backup_boot = sd_root / "boot.3dsx.bak"
    backup_smdh = sd_root / "boot.smdh.bak"
    if not safe_exists(backup_boot) and not safe_exists(backup_smdh):
        return

    tpl_boot = sd_root / "3ds" / "FirmMux" / "boot.3dsx"
    tpl_smdh = sd_root / "3ds" / "FirmMux" / "boot.smdh"
    dst_boot = sd_root / "boot.3dsx"
    dst_smdh = sd_root / "boot.smdh"
    if safe_exists(tpl_boot):
        copy_file_if_changed(tpl_boot, dst_boot)
    if safe_exists(tpl_smdh):
        copy_file_if_changed(tpl_smdh, dst_smdh)


def stage_dependencies(sd_root: Path) -> Dict[str, str]:
    deps = sd_root / "3ds" / "FirmMux" / "deps"
    cias = sd_root / "cias"
    deps.mkdir(parents=True, exist_ok=True)
    cias.mkdir(parents=True, exist_ok=True)

    downloaded: Dict[str, Path] = {}
    print_line("\nDownloading dependencies...")
    for key, (name, url, rel) in DEP_TASKS.items():
        out = sd_root / rel
        print_line(f"- {name}")
        updated = download(url, out)
        print_line(f"  {'updated' if updated else 'up-to-date'}")
        downloaded[key] = out

    print_line("- Extracting NTR_Launcher.cia...")
    if not extract_zip_cia(downloaded["ntr_launcher_zip"], sd_root / "cias" / "NTR_Launcher.cia"):
        raise RuntimeError("Could not extract NTR_Launcher.cia")

    with tempfile.TemporaryDirectory(prefix="firmmux_setup_") as td:
        td_path = Path(td)
        fwd_tmp = td_path / "forwarder"
        print_line("- Extracting NTR Forwarder package...")
        extract_7z(downloaded["ntr_forwarder_7z"], fwd_tmp)
        fwd_root = pick_forwarder_root(fwd_tmp)
        if not fwd_root:
            raise RuntimeError("Could not locate 'for SD card root' in NTR Forwarder package")
        print_line("- Applying NTR Forwarder files...")
        copy_tree_contents(fwd_root, sd_root)

        ra_tmp = td_path / "retroarch"
        print_line("- Extracting RetroArch package...")
        extract_7z(downloaded["retroarch_7z"], ra_tmp)
        print_line("- Staging RetroArch dependency files (without touching FirmMux custom RetroArch)...")
        retro_paths = stage_retroarch_dependencies_only(ra_tmp, sd_root)

    result = {
        "bootstrap_cia": str(sd_root / "cias" / "bootstrap.cia"),
        "ntr_launcher_cia": str(sd_root / "cias" / "NTR_Launcher.cia"),
    }
    result.update(retro_paths)
    try:
        print_line("- Staging optional standalone pathfile package...")
        result.update(stage_pathfile_standalone_package(sd_root))
    except Exception as e:
        print_line(f"- Optional standalone package skipped: {e}")
        result["pathfile_package"] = f"skipped: {e}"
    return result


def write_report(sd_root: Path, mode: str, summary: Dict[str, str]) -> Path:
    logs = sd_root / "3ds" / "FirmMux" / "logs"
    logs.mkdir(parents=True, exist_ok=True)
    report = logs / "setup_pc.log"
    lines = []
    lines.append("FirmMux PC setup complete.")
    lines.append(f"mode={mode}")
    for k in sorted(summary.keys()):
        lines.append(f"{k}={summary[k]}")
    lines.append("next=install CIAs manually via FBI if needed")
    report.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return report


def run_flow(sd_root: Path, mode: str) -> None:
    summary: Dict[str, str] = {}
    if mode in ("full", "firmmux"):
        summary.update(install_or_update_firmux(sd_root))
    if mode in ("full", "deps"):
        summary.update(stage_dependencies(sd_root))
    report = write_report(sd_root, mode, summary)
    print_line("\nDone.")
    print_line(f"Report: {report}")
    if mode in ("full", "deps"):
        print_line("Install CIAs with FBI if needed:")
        print_line("- sd:/cias/bootstrap.cia")
        print_line("- sd:/cias/NTR_Launcher.cia")
    dsp_path = sd_root / "3ds" / "dspfirm.cdc"
    if dsp_path.exists():
        print_line("DSP firmware: found (sd:/3ds/dspfirm.cdc)")
    else:
        print_line("")
        print_line("DSP firmware: missing (sd:/3ds/dspfirm.cdc)")
        print_line("Required action on 3DS:")
        print_line("- Open Rosalina menu (L + Down + Select)")
        print_line("- Miscellaneous options -> Dump DSP firmware")


def main() -> None:
    parser = argparse.ArgumentParser(description="FirmMux PC setup/update tool")
    parser.add_argument("--sd-root", help="Mounted 3DS SD root path")
    parser.add_argument("--mode", choices=["full", "firmmux", "deps"], help="full|firmmux|deps")
    args = parser.parse_args()

    sd_root = Path(args.sd_root).resolve() if args.sd_root else choose_sd_root()
    if not safe_exists(sd_root):
        raise RuntimeError(f"SD root does not exist: {sd_root}")
    if not looks_like_sd_root(sd_root):
        print_line("Selected path does not look like a 3DS SD card.")
        if not ask_yes_no("Continue anyway?", default_yes=False):
            print_line("Cancelled.")
            return

    print_health_summary(sd_root)
    suggested = suggested_mode_from_health(sd_root)
    mode = args.mode if args.mode else choose_mode(default_mode=suggested)
    if mode == "full":
        print_line("\nThis will install/update FirmMux and stage dependencies.")
    elif mode == "firmmux":
        print_line("\nThis will update FirmMux only.")
    else:
        print_line("\nThis will download/stage dependencies only.")
    if not ask_yes_no("Proceed?", default_yes=True):
        print_line("Cancelled.")
        return

    run_flow(sd_root, mode)


if __name__ == "__main__":
    try:
        main()
    except Exception as e:
        print_line(f"\nError: {e}")
        pause()
        raise SystemExit(1)
    pause()
