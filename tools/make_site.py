"""Assembles the web installer site from built boards.

Reads boards/<board>/board.json and dist/<board>/ (from tools/build.sh) and
writes site/: the page from web/, one ESP Web Tools manifest per board, the
merged firmware images, and boards.json listing what the page offers.

usage: python tools/make_site.py
"""
import json
import pathlib
import shutil

ROOT = pathlib.Path(__file__).resolve().parent.parent
SITE = ROOT / "site"


def main() -> None:
    version = (ROOT / "version.txt").read_text().strip()
    if SITE.exists():
        shutil.rmtree(SITE)
    shutil.copytree(ROOT / "web", SITE)
    (SITE / "firmware").mkdir()
    (SITE / "manifests").mkdir()

    boards = []
    for meta_path in sorted((ROOT / "boards").glob("*/board.json")):
        board = meta_path.parent.name
        image = ROOT / "dist" / board / f"coverart-{board}.bin"
        if not image.exists():
            raise SystemExit(f"{image} missing: run tools/build.sh {board} first")
        meta = json.loads(meta_path.read_text())
        shutil.copy2(image, SITE / "firmware" / image.name)
        manifest = {
            "name": f"CoverArt ESP32 for {meta['title']}",
            "version": version,
            # A clean install: old settings from other firmware must not linger.
            "new_install_prompt_erase": True,
            "builds": [
                {
                    "chipFamily": meta["chip_family"],
                    "parts": [{"path": f"../firmware/{image.name}", "offset": 0}],
                }
            ],
        }
        (SITE / "manifests" / f"{board}.json").write_text(json.dumps(manifest, indent=2))
        boards.append({"id": board, "title": meta["title"], "description": meta["description"],
                       "url": meta["url"], "manifest": f"manifests/{board}.json"})

    (SITE / "boards.json").write_text(json.dumps({"version": version, "boards": boards}, indent=2))
    print(f"site/ ready: {len(boards)} boards, version {version}")


if __name__ == "__main__":
    main()
