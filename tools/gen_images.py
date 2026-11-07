#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Генератор картинок пазла через локальный сервис SD Studio.

Читает банк промптов (prompts.txt), генерирует по одной картинке на каждый
промпт и кладёт результат в подкатегорию assets/images, указанную в заголовке
секции. Картинки сохраняются как есть, 1552x872 (кратно 8 — требование сервиса).

Использование:
    python tools/gen_images.py                     # все промпты
    python tools/gen_images.py --limit 5           # только первые 5
    python tools/gen_images.py --only mandalas     # только секции с этим путём
    python tools/gen_images.py --dry-run           # показать план, ничего не делая
    python tools/gen_images.py --redo              # игнорировать журнал, генерировать заново

Прогресс пишется в консоль и в tools/gen_images.log; журнал уже готовых
промптов — tools/.gen_state.json (нужен для продолжения после обрыва).
"""

import argparse
import json
import os
import random
import sys
import time
import urllib.request
import urllib.error

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(SCRIPT_DIR, ".."))
IMAGES = os.path.join(ROOT, "assets", "images")
PROMPTS_FILE = os.path.join(ROOT, "prompts.txt")
STATE_FILE = os.path.join(SCRIPT_DIR, ".gen_state.json")
LOG_FILE = os.path.join(SCRIPT_DIR, "gen_images.log")

BASE_URL = "http://192.168.1.61:5000"
PRESET = "z_image_turbo"

# Размер генерации: сервис принимает только кратное 8, 1552x872 — минимальный
# размер не меньше 1546x870 с сохранением 16:9. Картинки не масштабируются.
GEN_W, GEN_H = 1552, 872

JPEG_QUALITY = 85

NEGATIVE = (
    "text, letters, words, typography, watermark, signature, logo, caption, "
    "frame, border, vignette, blurry, out of focus, low detail, flat empty areas, "
    "seam, tiling artifacts, distorted anatomy, extra limbs, oversaturated mess"
)


def log(msg):
    line = f"{time.strftime('%H:%M:%S')} {msg}"
    print(line, flush=True)
    with open(LOG_FILE, "a", encoding="utf-8") as fh:
        fh.write(line + "\n")


def http_json(url, payload=None, timeout=60):
    data = None
    headers = {"Accept": "application/json"}
    if payload is not None:
        data = json.dumps(payload).encode("utf-8")
        headers["Content-Type"] = "application/json"
    req = urllib.request.Request(url, data=data, headers=headers,
                                 method="POST" if data else "GET")
    with urllib.request.urlopen(req, timeout=timeout) as resp:
        return json.loads(resp.read().decode("utf-8"))


def parse_prompts(path):
    """-> [(folder, russian_name, [prompt, ...]), ...]"""
    sections = []
    cur = None
    with open(path, encoding="utf-8") as fh:
        for raw in fh:
            line = raw.rstrip("\r\n")
            s = line.strip()
            if not s or s.startswith("=") or s.startswith("#"):
                continue
            if " — " in line and not line.startswith(" "):
                folder, name = line.split(" — ", 1)
                cur = (folder.strip(), name.strip(), [])
                sections.append(cur)
                continue
            if cur is not None:
                cur[2].append(s)
    return sections


def load_state():
    if os.path.isfile(STATE_FILE):
        with open(STATE_FILE, encoding="utf-8") as fh:
            return json.load(fh)
    return {}


def save_state(state):
    tmp = STATE_FILE + ".tmp"
    with open(tmp, "w", encoding="utf-8") as fh:
        json.dump(state, fh, ensure_ascii=False, indent=1)
    os.replace(tmp, STATE_FILE)


def submit(prompt, seed, steps, cfg):
    payload = {
        "preset_id": PRESET,
        "params": {
            "prompt": prompt,
            "negative_prompt": NEGATIVE,
            "width": GEN_W,
            "height": GEN_H,
            "seed": seed,
            "sample_steps": steps,
            "cfg_scale": cfg,
            "sample_method": "euler",
            "output_format": "png",
        },
        "images": {},
    }
    resp = http_json(f"{BASE_URL}/api/generate", payload)
    job = resp.get("job_id") or resp.get("id")
    if not job:
        raise RuntimeError(f"no job id in response: {resp}")
    return job


def wait_job(job, timeout=900):
    deadline = time.time() + timeout
    delay = 1.0
    while time.time() < deadline:
        st = http_json(f"{BASE_URL}/api/jobs/{job}")
        status = (st.get("status") or "").lower()
        if status == "completed":
            return st
        if status in ("failed", "error", "cancelled", "canceled"):
            raise RuntimeError(f"job {job} {status}: {st.get('error') or st}")
        time.sleep(delay)
        delay = min(delay * 1.4, 4.0)
    raise RuntimeError(f"job {job} timeout")


def result_url(job_status):
    files = job_status.get("result_files") or []
    for f in files:
        if isinstance(f, dict):
            url = f.get("file_url") or f.get("url") or f.get("file")
            if url:
                return url
        elif isinstance(f, str):
            return f
    raise RuntimeError(f"no result file: {job_status}")


def save_image(url, folder, job, seed, index):
    if url.startswith("http"):
        full = url
    else:
        full = BASE_URL + ("" if url.startswith("/") else "/") + url
    name = os.path.basename(full.split("?")[0]) or f"{job}_0.png"
    stem = os.path.splitext(name)[0]
    dst_dir = os.path.join(IMAGES, folder)
    os.makedirs(dst_dir, exist_ok=True)
    dst = os.path.join(dst_dir, f"{stem}.jpg")

    tmp = dst + ".part"
    with urllib.request.urlopen(full, timeout=120) as resp, open(tmp, "wb") as fh:
        fh.write(resp.read())

    from PIL import Image
    img = Image.open(tmp)
    img.load()
    if img.mode not in ("RGB", "L"):
        img = img.convert("RGB")
    img.save(dst, "JPEG", quality=JPEG_QUALITY, optimize=True, progressive=True)
    os.remove(tmp)
    return dst


def generate_one(prompt, folder, seed, steps, cfg, retries=2):
    last = None
    for attempt in range(retries + 1):
        try:
            job = submit(prompt, seed, steps, cfg)
            st = wait_job(job)
            return save_image(result_url(st), folder, job, seed, attempt), job
        except Exception as e:  # noqa: BLE001 - сеть/сервис, повторяем
            last = e
            log(f"    ! попытка {attempt + 1} не удалась: {e}")
            time.sleep(3 * (attempt + 1))
    raise last


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--limit", type=int, default=0)
    ap.add_argument("--only", default="")
    ap.add_argument("--steps", type=int, default=8)
    ap.add_argument("--cfg", type=float, default=1.0)
    ap.add_argument("--seed", type=int, default=0, help="база сида (0 = случайно)")
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--redo", action="store_true")
    args = ap.parse_args()

    if not os.path.isfile(PROMPTS_FILE):
        print(f"[ERROR] prompts.txt not found: {PROMPTS_FILE}")
        return 1

    sections = parse_prompts(PROMPTS_FILE)
    total = sum(len(p) for _, _, p in sections)
    plan = [(f, n, p) for f, n, ps in sections for p in ps]
    if args.only:
        plan = [x for x in plan if args.only.lower() in x[0].lower()]
    if args.limit:
        plan = plan[:args.limit]

    log(f"секций: {len(sections)}, промптов всего: {total}, к генерации: {len(plan)}")
    log(f"пресет {PRESET}, {GEN_W}x{GEN_H}, "
        f"steps {args.steps}, cfg {args.cfg}")

    if args.dry_run:
        for folder, _name, prompt in plan[:5]:
            print(f"  {folder} <- {prompt[:70]}...")
        print(f"  ... всего {len(plan)}")
        return 0

    state = {} if args.redo else load_state()
    done = failed = 0
    t_start = time.time()

    for i, (folder, _rub, prompt) in enumerate(plan, 1):
        key = f"{folder}|{prompt}"
        if not args.redo and key in state and os.path.isfile(os.path.join(IMAGES, state[key]["file"])):
            done += 1
            continue

        seed = args.seed + i if args.seed else random.randint(1, 2**31 - 1)
        t0 = time.time()
        try:
            path, job = generate_one(prompt, folder, seed, args.steps, args.cfg)
        except Exception as e:  # noqa: BLE001
            failed += 1
            log(f"[{i}/{len(plan)}] FAIL {folder}: {e}")
            continue

        rel = os.path.relpath(path, IMAGES).replace(os.sep, "/")
        state[key] = {"file": rel, "seed": seed, "job": job}
        save_state(state)
        done += 1
        log(f"[{i}/{len(plan)}] {folder} -> {rel} "
            f"({time.time() - t0:.1f}s, seed {seed})")

    spent = time.time() - t_start
    log(f"готово: {done}, ошибок: {failed}, время {spent / 60:.1f} мин")
    if failed:
        log("часть промптов не сгенерирована — запустите скрипт ещё раз, "
            "готовые он пропустит")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
