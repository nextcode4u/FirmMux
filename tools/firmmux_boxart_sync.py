#!/usr/bin/env python3
import argparse
import hashlib
import io
import json
import os
import re
import sys
import tempfile
import threading
import time
import urllib.error
import urllib.parse
import urllib.request
from concurrent.futures import ThreadPoolExecutor, as_completed
from datetime import datetime, timedelta, timezone
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    print("Error: Pillow is required. Install with: pip install pillow", file=sys.stderr)
    sys.exit(2)


PLAYLIST_MAP = {
    "a26": "Atari - 2600",
    "a52": "Atari - 5200",
    "a78": "Atari - 7800",
    "col": "Coleco - ColecoVision",
    "cpc": "Amstrad - CPC",
    "gb": "Nintendo - Game Boy",
    "gen": "Sega - Mega Drive - Genesis",
    "gg": "Sega - Game Gear",
    "intv": "Mattel - Intellivision",
    "m5": "Sord - M5",
    "nes": "Nintendo - Nintendo Entertainment System",
    "ngp": "SNK - Neo Geo Pocket",
    "pkmni": "Nintendo - Pokemon Mini",
    "pknmini": "Nintendo - Pokemon Mini",
    "sg": "Sega - SG-1000",
    "sms": "Sega - Master System - Mark III",
    "snes": "Nintendo - Super Nintendo Entertainment System",
    "tg16": "NEC - PC Engine - TurboGrafx 16",
    "ws": "Bandai - WonderSwan - Color",
}

ROM_EXTENSIONS = {
    "a26": {".a26", ".bin", ".rom"},
    "a52": {".a52", ".bin", ".rom"},
    "a78": {".a78", ".bin", ".rom"},
    "col": {".col", ".rom", ".bin"},
    "cpc": {".dsk", ".sna", ".cdt"},
    "gb": {".gb", ".gbc"},
    "gen": {".gen", ".md", ".smd", ".bin"},
    "gg": {".gg"},
    "intv": {".intv", ".int", ".bin", ".rom"},
    "m5": {".rom", ".m5", ".bin"},
    "nes": {".nes", ".fds", ".unf", ".unif"},
    "ngp": {".ngp", ".ngc", ".npc"},
    "pkmni": {".min"},
    "pknmini": {".min"},
    "sg": {".sg", ".bin", ".rom"},
    "sms": {".sms", ".bin", ".rom"},
    "snes": {".sfc", ".smc", ".fig", ".swc"},
    "tg16": {".pce", ".sgx", ".cue", ".chd"},
    "ws": {".ws", ".wsc"},
}

TAG_PATTERNS = [
    re.compile(r"\((?:USA|Europe|Japan|Rev ?[A-Za-z0-9]+|Beta|Proto|Demo|Unl|Hack)\)", re.IGNORECASE),
    re.compile(r"\[(?:USA|Europe|Japan|Rev ?[A-Za-z0-9]+|Beta|Proto|Demo|Unl|Hack)\]", re.IGNORECASE),
]

ROMAN_VARIANTS = [
    (re.compile(r"\bII\b"), "2"),
    (re.compile(r"\bIII\b"), "3"),
    (re.compile(r"\bIV\b"), "4"),
]

CATEGORIES = ["Named_Titles", "Named_Boxarts", "Named_Snaps"]
IMAGE_EXTS = ["png", "jpg", "jpeg"]
LOCK = threading.Lock()


def now_utc():
    return datetime.now(timezone.utc)


def parse_iso(ts):
    try:
        return datetime.fromisoformat(ts.replace("Z", "+00:00"))
    except Exception:
        return None


def safe_name(value):
    value = re.sub(r'[<>:"/\\|?*\x00-\x1F]', "_", value)
    value = re.sub(r"\s+", " ", value).strip()
    return value or "unknown"


def rom_id(path: Path):
    st = path.stat()
    payload = f"{path.resolve()}|{st.st_size}|{int(st.st_mtime)}".encode("utf-8", "ignore")
    return hashlib.sha1(payload).hexdigest()


def normalize_spaces(text):
    return re.sub(r"\s+", " ", text).strip()


def strip_after_token(text, token):
    pos = text.find(token)
    if pos >= 0:
        return text[:pos].strip()
    return text


def with_roman_variants(name):
    variants = [name]
    for pattern, replacement in ROMAN_VARIANTS:
        candidate = pattern.sub(replacement, name)
        if candidate != name:
            variants.append(candidate)
    if " 2 " in f" {name} ":
        variants.append(re.sub(r"\b2\b", "II", name))
    if " 3 " in f" {name} ":
        variants.append(re.sub(r"\b3\b", "III", name))
    if " 4 " in f" {name} ":
        variants.append(re.sub(r"\b4\b", "IV", name))
    return variants


def generate_candidates(base_name, mode):
    names = []

    def add(value):
        value = normalize_spaces(value)
        if value and value not in names:
            names.append(value)

    add(base_name)
    add(strip_after_token(base_name, "("))
    add(strip_after_token(base_name, "["))

    if mode in ("balanced", "aggressive"):
        cleaned = base_name
        for pattern in TAG_PATTERNS:
            cleaned = pattern.sub("", cleaned)
        add(cleaned)
        add(cleaned.replace("&", "and"))
        add(cleaned.replace("'", ""))
        add(cleaned.replace("’", ""))
        add(cleaned.replace(" and ", " & "))

    if mode == "aggressive":
        for name in list(names):
            for variant in with_roman_variants(name):
                add(variant)

    if mode == "fast":
        return names[:2]
    return names


def get_provider_url(provider, playlist, category, candidate, ext):
    playlist_enc = urllib.parse.quote(playlist, safe="")
    candidate_enc = urllib.parse.quote(candidate, safe="")
    if provider == "cdn":
        return f"https://thumbnails.libretro.com/{playlist_enc}/{category}/{candidate_enc}.{ext}"
    if provider == "githubraw":
        return (
            "https://raw.githubusercontent.com/libretro-thumbnails/"
            f"{playlist_enc}/master/{category}/{candidate_enc}.{ext}"
        )
    raise ValueError(f"Unknown provider: {provider}")


def read_json(path, default):
    if not path.exists():
        return default
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except Exception:
        return default


def write_json_atomic(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile("w", encoding="utf-8", dir=str(path.parent), delete=False) as tmp:
        json.dump(data, tmp, indent=2, ensure_ascii=False)
        tmp.write("\n")
        tmp_path = Path(tmp.name)
    os.replace(tmp_path, path)


def resize_to_square_png(src_bytes, out_path, size):
    with Image.open(io.BytesIO(src_bytes)) as image:
        image = image.convert("RGBA")
        image.thumbnail((size, size), Image.Resampling.LANCZOS)
        canvas = Image.new("RGBA", (size, size), (0, 0, 0, 0))
        x = (size - image.width) // 2
        y = (size - image.height) // 2
        canvas.paste(image, (x, y))
        out_path.parent.mkdir(parents=True, exist_ok=True)
        with tempfile.NamedTemporaryFile("wb", dir=str(out_path.parent), delete=False) as tmp:
            canvas.save(tmp, format="PNG")
            tmp_path = Path(tmp.name)
        os.replace(tmp_path, out_path)


def fetch_with_retry(url, timeout, retries, log):
    delay = 0.4
    for attempt in range(retries + 1):
        try:
            req = urllib.request.Request(url, headers={"User-Agent": "FirmMux-BoxartSync/1.0"})
            with urllib.request.urlopen(req, timeout=timeout) as response:
                status = getattr(response, "status", 200)
                if status != 200:
                    return None, status, None
                data = response.read()
                if not data:
                    return None, 502, None
                return data, 200, None
        except urllib.error.HTTPError as err:
            return None, err.code, err
        except Exception as err:
            if attempt == retries:
                return None, 0, err
            log(f"retry {attempt + 1}/{retries} {url}")
            time.sleep(delay)
            delay *= 2
    return None, 0, None


class SyncContext:
    def __init__(self, args):
        self.args = args
        self.sd_root = Path(args.sd_root).resolve()
        self.rom_root = self.sd_root / "roms"
        self.cache_root = Path(args.cache_dir).resolve() if args.cache_dir else self.sd_root / "3ds" / "FirmMux" / "cache" / "covers"
        self.user_override_root = self.cache_root / "_user"
        self.index_path = self.cache_root / "index.json"
        self.neg_path = self.cache_root / "negative_cache.json"
        self.log_path = Path(args.log_file).resolve() if args.log_file else self.sd_root / "3ds" / "FirmMux" / "logs" / "cover_sync.log"
        self.providers = [x.strip().lower() for x in args.providers.split(",") if x.strip()]
        self.system_filter = {x.strip().lower() for x in args.systems.split(",")} if args.systems else None
        self.sha1_cache = {}
        self.dat_name_cache = {}
        self.dat_lock = threading.Lock()
        self.dat_cache_dir = self.cache_root / "_sync_dat"
        self.stats = {
            "scanned": 0,
            "eligible": 0,
            "cached": 0,
            "downloaded": 0,
            "user_override": 0,
            "not_found": 0,
            "network_fail": 0,
            "skipped_negative": 0,
            "errors": 0,
        }
        self.index = read_json(self.index_path, {})
        self.negative = read_json(self.neg_path, {})
        self.log_path.parent.mkdir(parents=True, exist_ok=True)
        self._log_handle = self.log_path.open("a", encoding="utf-8")

    def log(self, message):
        ts = now_utc().strftime("%Y-%m-%d %H:%M:%S")
        line = f"[{ts}] {message}"
        with LOCK:
            print(line)
            self._log_handle.write(line + "\n")
            self._log_handle.flush()

    def close(self):
        self._log_handle.close()

    def should_scan_system(self, system_key):
        if not self.system_filter:
            return True
        return system_key in self.system_filter


def parse_roms(ctx: SyncContext):
    if not ctx.rom_root.exists():
        raise FileNotFoundError(f"rom root not found: {ctx.rom_root}")
    roms = []
    for system_dir in sorted(ctx.rom_root.iterdir()):
        if not system_dir.is_dir():
            continue
        system_key = system_dir.name.lower()
        ctx.stats["scanned"] += 1
        if system_key not in PLAYLIST_MAP:
            continue
        if not ctx.should_scan_system(system_key):
            continue
        allowed_exts = ROM_EXTENSIONS.get(system_key)
        for root, _, files in os.walk(system_dir):
            for name in files:
                full = Path(root) / name
                ext = full.suffix.lower()
                if allowed_exts and ext not in allowed_exts:
                    continue
                ctx.stats["eligible"] += 1
                roms.append((system_key, full))
    return roms


def rom_sha1(ctx: SyncContext, rom_path: Path):
    key = str(rom_path)
    cached = ctx.sha1_cache.get(key)
    if cached:
        return cached
    sha1 = hashlib.sha1()
    with rom_path.open("rb") as handle:
        while True:
            chunk = handle.read(1024 * 1024)
            if not chunk:
                break
            sha1.update(chunk)
    digest = sha1.hexdigest().upper()
    ctx.sha1_cache[key] = digest
    return digest


def libretro_dat_url(playlist):
    playlist_enc = urllib.parse.quote(playlist, safe="")
    return f"https://raw.githubusercontent.com/libretro/libretro-database/master/dat/No-Intro/{playlist_enc}.dat"


def parse_libretro_dat_sha1_map(dat_text):
    mapping = {}
    blocks = []
    i = 0
    while True:
        start = dat_text.find("game (", i)
        if start < 0:
            break
        j = start + len("game ")
        depth = 0
        block_start = -1
        while j < len(dat_text):
            c = dat_text[j]
            if c == "(":
                if depth == 0:
                    block_start = j + 1
                depth += 1
            elif c == ")":
                depth -= 1
                if depth == 0 and block_start >= 0:
                    blocks.append(dat_text[block_start:j])
                    i = j + 1
                    break
            j += 1
        else:
            break
    for block in blocks:
        name_m = re.search(r'name\s+"([^"]+)"', block)
        sha1_m = re.search(r"sha1\s+([0-9A-Fa-f]{40})", block)
        if not name_m or not sha1_m:
            continue
        mapping[sha1_m.group(1).upper()] = normalize_spaces(name_m.group(1))
    return mapping


def load_dat_map_for_playlist(ctx: SyncContext, playlist):
    with ctx.dat_lock:
        if playlist in ctx.dat_name_cache:
            return ctx.dat_name_cache[playlist]

    ctx.dat_cache_dir.mkdir(parents=True, exist_ok=True)
    local_dat = ctx.dat_cache_dir / f"{safe_name(playlist)}.dat"

    dat_bytes = b""
    if local_dat.exists() and local_dat.stat().st_size > 0 and (now_utc() - datetime.fromtimestamp(local_dat.stat().st_mtime, tz=timezone.utc)) < timedelta(days=3):
        dat_bytes = local_dat.read_bytes()
    else:
        url = libretro_dat_url(playlist)
        data, status, _ = fetch_with_retry(url, ctx.args.timeout, ctx.args.retries, ctx.log)
        if data and status == 200:
            with tempfile.NamedTemporaryFile("wb", dir=str(local_dat.parent), delete=False) as tmp:
                tmp.write(data)
                tmp_path = Path(tmp.name)
            os.replace(tmp_path, local_dat)
            dat_bytes = data
        else:
            dat_bytes = b""

    mapping = {}
    if dat_bytes:
        try:
            mapping = parse_libretro_dat_sha1_map(dat_bytes.decode("utf-8", errors="ignore"))
            ctx.log(f"nointro dat loaded playlist={playlist} entries={len(mapping)}")
        except Exception as ex:
            ctx.log(f"nointro dat parse-failed playlist={playlist} err={ex}")
            mapping = {}
    else:
        ctx.log(f"nointro dat unavailable playlist={playlist}")

    with ctx.dat_lock:
        ctx.dat_name_cache[playlist] = mapping
    return mapping


def prepend_nointro_candidate(ctx: SyncContext, playlist, rom_path, candidates):
    try:
        dat_map = load_dat_map_for_playlist(ctx, playlist)
        if not dat_map:
            return candidates
        sha = rom_sha1(ctx, rom_path)
        title = dat_map.get(sha)
        if not title:
            return candidates
        out = [title]
        for item in candidates:
            if item != title:
                out.append(item)
        ctx.log(f"nointro match rom={rom_path.name} sha1={sha} title={title}")
        return out
    except Exception as ex:
        ctx.log(f"nointro error rom={rom_path.name} err={ex}")
        return candidates


def negative_key(rom_hash):
    return rom_hash


def negative_expired(entry):
    expiry = parse_iso(entry.get("expires", ""))
    if not expiry:
        return True
    return now_utc() >= expiry


def resolve_target_path(ctx: SyncContext, system_key, rom_path):
    rom_stem = safe_name(rom_path.stem)
    return ctx.cache_root / system_key / f"{rom_stem}.png"


def check_user_override(ctx: SyncContext, playlist, candidates, out_path, size):
    for candidate in candidates:
        p = ctx.user_override_root / safe_name(playlist) / f"{safe_name(candidate)}.png"
        if p.exists():
            data = p.read_bytes()
            resize_to_square_png(data, out_path, size)
            return str(p)
    return None


def fetch_cover_for_rom(ctx: SyncContext, system_key, rom_path):
    playlist = PLAYLIST_MAP.get(system_key)
    if not playlist:
        ctx.log(f"skip no-playlist system={system_key} rom={rom_path.name}")
        return

    out_path = resolve_target_path(ctx, system_key, rom_path)
    rom_hash = rom_id(rom_path)
    base_name = rom_path.stem
    candidates = generate_candidates(base_name, ctx.args.mode)
    if ctx.args.hash_mode in ("missing", "all"):
        candidates = prepend_nointro_candidate(ctx, playlist, rom_path, candidates)

    if not ctx.args.force and out_path.exists() and out_path.stat().st_size > 0:
        ctx.stats["cached"] += 1
        ctx.index[rom_hash] = {
            "rom": str(rom_path),
            "playlist": playlist,
            "resolved_name": base_name,
            "category": "local_cache",
            "path": str(out_path),
            "provider": "cache",
            "last_checked": now_utc().isoformat(),
            "status": "cached",
        }
        return

    if not ctx.args.force:
        neg = ctx.negative.get(negative_key(rom_hash))
        if neg and not negative_expired(neg):
            ctx.stats["skipped_negative"] += 1
            ctx.log(f"skip negative-cache rom={rom_path.name} reason={neg.get('reason')}")
            return

    override = check_user_override(ctx, playlist, candidates, out_path, ctx.args.size)
    if override:
        ctx.stats["user_override"] += 1
        ctx.index[rom_hash] = {
            "rom": str(rom_path),
            "playlist": playlist,
            "resolved_name": base_name,
            "category": "user_override",
            "path": str(out_path),
            "provider": "user",
            "last_checked": now_utc().isoformat(),
            "status": "downloaded",
        }
        ctx.log(f"user-override rom={rom_path.name} source={override}")
        return

    for candidate in candidates:
        for category in CATEGORIES:
            for provider in ctx.providers:
                for ext in IMAGE_EXTS:
                    url = get_provider_url(provider, playlist, category, candidate, ext)
                    data, status, err = fetch_with_retry(url, ctx.args.timeout, ctx.args.retries, ctx.log)
                    ctx.log(f"try rom={rom_path.name} candidate={candidate} category={category} provider={provider} status={status}")
                    if data and status == 200:
                        if ctx.args.dry_run:
                            ctx.stats["downloaded"] += 1
                            return
                        try:
                            resize_to_square_png(data, out_path, ctx.args.size)
                        except Exception as ex:
                            ctx.stats["errors"] += 1
                            ctx.log(f"error resize rom={rom_path.name} err={ex}")
                            return
                        ctx.stats["downloaded"] += 1
                        ctx.index[rom_hash] = {
                            "rom": str(rom_path),
                            "playlist": playlist,
                            "resolved_name": candidate,
                            "category": category,
                            "path": str(out_path),
                            "provider": provider,
                            "last_checked": now_utc().isoformat(),
                            "status": "downloaded",
                        }
                        if negative_key(rom_hash) in ctx.negative:
                            del ctx.negative[negative_key(rom_hash)]
                        return
                    if status not in (404, 0):
                        break
                    if err and status == 0:
                        break

    reason = "not_found"
    ttl = timedelta(days=7)
    if any(v.get("status") == "network_error" for v in []):
        reason = "network_error"
        ttl = timedelta(minutes=30)
    ctx.negative[negative_key(rom_hash)] = {
        "rom": str(rom_path),
        "reason": reason,
        "expires": (now_utc() + ttl).isoformat(),
    }
    if reason == "not_found":
        ctx.stats["not_found"] += 1
    else:
        ctx.stats["network_fail"] += 1
    ctx.index[rom_hash] = {
        "rom": str(rom_path),
        "playlist": playlist,
        "resolved_name": base_name,
        "category": "none",
        "path": str(out_path),
        "provider": "none",
        "last_checked": now_utc().isoformat(),
        "status": reason,
    }


def run(args):
    ctx = SyncContext(args)
    started = time.time()
    try:
        roms = parse_roms(ctx)
        ctx.log(f"scan roms={len(roms)} mode={args.mode} providers={','.join(ctx.providers)} size={args.size}")
        if args.limit > 0:
            roms = roms[: args.limit]
        with ThreadPoolExecutor(max_workers=args.workers) as executor:
            futures = [executor.submit(fetch_cover_for_rom, ctx, system_key, rom_path) for system_key, rom_path in roms]
            for future in as_completed(futures):
                try:
                    future.result()
                except Exception as ex:
                    ctx.stats["errors"] += 1
                    ctx.log(f"error worker={ex}")
        if not args.dry_run:
            write_json_atomic(ctx.index_path, ctx.index)
            write_json_atomic(ctx.neg_path, ctx.negative)
        elapsed = time.time() - started
        ctx.log(
            "done "
            + " ".join([f"{k}={v}" for k, v in ctx.stats.items()])
            + f" elapsed_sec={elapsed:.1f}"
        )
        found_total = ctx.stats["cached"] + ctx.stats["downloaded"] + ctx.stats["user_override"]
        unresolved_total = ctx.stats["not_found"] + ctx.stats["network_fail"] + ctx.stats["skipped_negative"]
        print("\n================ Cover Sync Summary ================")
        print(f"ROM files scanned:            {ctx.stats['eligible']}")
        print(f"Covers available (total):     {found_total}")
        print(f"  - Already cached:           {ctx.stats['cached']}")
        print(f"  - Downloaded this run:      {ctx.stats['downloaded']}")
        print(f"  - User override used:       {ctx.stats['user_override']}")
        print(f"Unresolved this run:          {unresolved_total}")
        print(f"  - Not found:                {ctx.stats['not_found']}")
        print(f"  - Network failures:         {ctx.stats['network_fail']}")
        print(f"  - Skipped (negative cache): {ctx.stats['skipped_negative']}")
        print(f"Errors:                       {ctx.stats['errors']}")
        print(f"Elapsed time (seconds):       {elapsed:.1f}")
        print("====================================================\n")
    finally:
        ctx.close()


def build_parser():
    parser = argparse.ArgumentParser(description="FirmMux ROM cover sync (Libretro-only, host-side).")
    parser.add_argument("--sd-root", required=True, help="Mounted SD card root (example: E:\\ or /media/user/SD)")
    parser.add_argument("--cache-dir", default="", help="Override cover cache output dir")
    parser.add_argument("--log-file", default="", help="Override log file path")
    parser.add_argument("--systems", default="", help="Comma list of system keys to scan (example: gb,snes,gen)")
    parser.add_argument("--providers", default="cdn,githubraw", help="Libretro providers: cdn,githubraw")
    parser.add_argument("--mode", choices=["fast", "balanced", "aggressive"], default="balanced", help="Match aggressiveness")
    parser.add_argument("--hash-mode", choices=["none", "missing", "all"], default="missing", help="No-Intro SHA1 matching mode")
    parser.add_argument("--workers", type=int, default=6, help="Parallel worker count")
    parser.add_argument("--timeout", type=float, default=10.0, help="Per-request timeout seconds")
    parser.add_argument("--retries", type=int, default=1, help="Retry count for transient errors")
    parser.add_argument("--size", type=int, default=92, help="Square output size (default: 92)")
    parser.add_argument("--force", action="store_true", help="Ignore existing and negative cache")
    parser.add_argument("--dry-run", action="store_true", help="Do not write images or index files")
    parser.add_argument("--limit", type=int, default=0, help="Process only first N ROMs (0 = all)")
    return parser


def parse_args():
    return build_parser().parse_args()


def prompt(prompt_text, default=""):
    if default:
        value = input(f"{prompt_text} [{default}]: ").strip()
        return value if value else default
    return input(f"{prompt_text}: ").strip()


def prompt_bool(prompt_text, default=False):
    default_char = "y" if default else "n"
    value = input(f"{prompt_text} (y/n) [{default_char}]: ").strip().lower()
    if not value:
        return default
    return value in ("y", "yes", "1", "true")


def choose_menu(title, options, default_index=1):
    print(f"\n{title}")
    for idx, (label, _) in enumerate(options, start=1):
        marker = " (default)" if idx == default_index else ""
        print(f"  {idx}. {label}{marker}")
    while True:
        choice = input(f"Select option [default {default_index}]: ").strip()
        if not choice:
            return options[default_index - 1][1]
        if choice.isdigit():
            idx = int(choice)
            if 1 <= idx <= len(options):
                return options[idx - 1][1]
        print("Invalid selection.")


def safe_exists(path: Path):
    try:
        return path.exists()
    except Exception:
        return False


def safe_is_dir(path: Path):
    try:
        return path.is_dir()
    except Exception:
        return False


def safe_iterdir(path: Path):
    try:
        return list(path.iterdir())
    except Exception:
        return []


def candidate_sd_roots():
    roots = []
    if os.name == "nt":
        for letter in "DEFGHIJKLMNOPQRSTUVWXYZ":
            path = Path(f"{letter}:/")
            if safe_exists(path):
                roots.append(path)
    else:
        candidates = [Path("/media"), Path("/run/media"), Path("/mnt"), Path("/Volumes")]
        seen = set()
        for base in candidates:
            if not safe_exists(base):
                continue
            for entry in safe_iterdir(base):
                if safe_is_dir(entry):
                    for sub in safe_iterdir(entry):
                        if safe_is_dir(sub):
                            rp = str(sub.resolve())
                            if rp not in seen:
                                roots.append(sub.resolve())
                                seen.add(rp)
                    rp = str(entry.resolve())
                    if rp not in seen:
                        roots.append(entry.resolve())
                        seen.add(rp)
    filtered = []
    seen = set()
    for root in roots:
        key = str(root)
        if key in seen:
            continue
        seen.add(key)
        if safe_exists(root / "roms") or safe_exists(root / "3ds"):
            filtered.append(root)
    return filtered if filtered else roots


def choose_sd_root():
    roots = candidate_sd_roots()
    if roots:
        print("Detected drives/roots:")
        for i, root in enumerate(roots, start=1):
            hints = []
            if (root / "roms").exists():
                hints.append("roms")
            if (root / "3ds").exists():
                hints.append("3ds")
            suffix = f" ({', '.join(hints)})" if hints else ""
            print(f"  {i}. {root}{suffix}")
        print("  M. Manual path")
        while True:
            choice = input("Select SD root by number (or M): ").strip().lower()
            if choice == "m":
                break
            if choice.isdigit():
                idx = int(choice)
                if 1 <= idx <= len(roots):
                    return str(roots[idx - 1])
            print("Invalid selection.")
    sd_root = prompt("SD root path", "")
    while not sd_root:
        sd_root = prompt("SD root path", "")
    return sd_root


def interactive_args():
    print("FirmMux Cover Art Sync")
    print("----------------------")
    sd_root = choose_sd_root()
    preset = choose_menu(
        "Choose scan type:",
        [
            ("Recommended (best for most users)", "recommended"),
            ("Fast (quicker, fewer matches)", "fast"),
            ("Deep Scan (slower, highest match rate)", "deep"),
            ("Advanced (manual options)", "advanced"),
        ],
        default_index=1,
    )

    mode = "balanced"
    hash_mode = "missing"
    providers = "cdn,githubraw"
    workers = 6
    systems = ""
    force = False
    dry_run = False
    limit = 0
    timeout = 10.0
    retries = 1
    size = 92

    if preset == "fast":
        mode = "fast"
        hash_mode = "none"
        workers = 4
    elif preset == "deep":
        mode = "aggressive"
        hash_mode = "all"
        workers = 8
    elif preset == "advanced":
        mode = choose_menu(
            "Match mode:",
            [
                ("balanced - recommended speed/accuracy", "balanced"),
                ("fast - quickest, fewer variants", "fast"),
                ("aggressive - slowest, most variants", "aggressive"),
            ],
            default_index=1,
        )

        hash_mode = choose_menu(
            "Hash mode:",
            [
                ("missing - recommended (hash only when needed)", "missing"),
                ("none - fastest (no hashing)", "none"),
                ("all - best match rate, slowest", "all"),
            ],
            default_index=1,
        )

        providers = choose_menu(
            "Providers:",
            [
                ("CDN + GitHub Raw fallback (recommended)", "cdn,githubraw"),
                ("CDN only", "cdn"),
                ("GitHub Raw only", "githubraw"),
                ("Custom provider list", "CUSTOM"),
            ],
            default_index=1,
        )
        if providers == "CUSTOM":
            providers = prompt("Enter providers (comma list)", "cdn,githubraw")
            if not providers.strip():
                providers = "cdn,githubraw"

        workers_raw = prompt("Workers", "6")
        try:
            workers = max(1, int(workers_raw))
        except Exception:
            workers = 6
        systems = prompt("Systems filter (comma list, blank = all)", "")
        dry_run = prompt_bool("Dry run (test only, do not write files)", False)
        limit_raw = prompt("Limit ROM count (0 = all)", "0")
        try:
            limit = max(0, int(limit_raw))
        except Exception:
            limit = 0
        timeout_raw = prompt("Request timeout seconds", "10")
        retries_raw = prompt("Retries", "1")
        size_raw = prompt("Output size", "92")
        try:
            timeout = float(timeout_raw)
        except Exception:
            timeout = 10.0
        try:
            retries = max(0, int(retries_raw))
        except Exception:
            retries = 1
        try:
            size = max(16, int(size_raw))
        except Exception:
            size = 92

    force = prompt_bool("Force refresh existing covers", False)

    print("\nRun summary:")
    print(f"  SD root: {sd_root}")
    print(f"  Match mode: {mode}")
    print(f"  Hash mode: {hash_mode}")
    print(f"  Providers: {providers}")
    print(f"  Workers: {workers}")
    print(f"  Systems filter: {systems or 'all'}")
    print(f"  Force refresh: {'yes' if force else 'no'}")
    print(f"  Dry run: {'yes' if dry_run else 'no'}")
    print(f"  Limit: {limit}")
    print(f"  Output size: {size}x{size}")
    if not prompt_bool("Start sync now", True):
        raise KeyboardInterrupt()

    ns = argparse.Namespace(
        sd_root=sd_root,
        cache_dir="",
        log_file="",
        systems=systems,
        providers=providers,
        mode=mode,
        hash_mode=hash_mode,
        workers=workers,
        timeout=timeout,
        retries=retries,
        size=size,
        force=force,
        dry_run=dry_run,
        limit=limit,
    )
    return ns


def pause_exit():
    try:
        input("\nPress Enter to exit...")
    except Exception:
        pass


if __name__ == "__main__":
    if len(sys.argv) == 1:
        try:
            run(interactive_args())
        except KeyboardInterrupt:
            print("\nCancelled.")
        except Exception as ex:
            print(f"Error: {ex}", file=sys.stderr)
        finally:
            pause_exit()
    else:
        run(parse_args())
