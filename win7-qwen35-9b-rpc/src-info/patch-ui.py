#!/usr/bin/env python3
# Adds a small "Minimal" thinking level to the official llama.cpp web UI (prebuilt dist).
#
# The Reasoning effort menu offers Off / Low (512) / Medium (2048) / High (8192) / Max.
# 512 tokens is long at ~2.5 t/s, so this inserts "Minimal" (default 160 tokens) after Off.
# The bundle is renamed and the service worker manifest updated, so browsers that cached
# the old UI pick up the new one.
#
# usage: python3 -I patch-ui.py DIST_DIR [BUDGET_TOKENS]
#   DIST_DIR: the extracted official dist (e.g. build/tools/ui/dist from dist.tar.gz).
#   Copy the patched tree to tools/ui/dist and rebuild llama-server: llama.cpp embeds
#   tools/ui/dist with the highest priority.
import hashlib
import os
import re
import sys


def replace_once(text, old, new, what):
    n = text.count(old)
    if n != 1:
        sys.exit(f"patch-ui: expected exactly one {what}, found {n} (UI version changed?)")
    return text.replace(old, new)


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__ or "usage: patch-ui.py DIST_DIR [BUDGET_TOKENS]")
    dist = sys.argv[1]
    budget = int(sys.argv[2]) if len(sys.argv) > 2 else 160

    index_path = os.path.join(dist, "index.html")
    sw_path = os.path.join(dist, "sw.js")
    index = open(index_path, encoding="utf-8").read()
    sw = open(sw_path, encoding="utf-8").read()

    m = re.search(r"_app/immutable/(bundle\.[A-Za-z0-9_-]+)\.js", index)
    if not m:
        sys.exit("patch-ui: bundle not found in index.html")
    old_name = m.group(1)
    if old_name.endswith("-min"):
        sys.exit("patch-ui: already patched")
    new_name = old_name + "-min"
    bundle_path = os.path.join(dist, "_app", "immutable", old_name + ".js")
    js = open(bundle_path, encoding="utf-8").read()

    # menu entry, after "Off"
    js = replace_once(js, '{label:"Off",value:ss.OFF},{label:"Low",value:ss.LOW}',
                      '{label:"Off",value:ss.OFF},{label:"Minimal",value:"minimal"},{label:"Low",value:ss.LOW}',
                      "reasoning level list")
    # token budget (the menu text "Max N tokens" is derived from it)
    js = replace_once(js, "{[ss.HIGH]:8192,[ss.LOW]:512,",
                      "{minimal:%d,[ss.HIGH]:8192,[ss.LOW]:512," % budget, "reasoning budget table")

    open(os.path.join(dist, "_app", "immutable", new_name + ".js"), "w", encoding="utf-8").write(js)
    os.remove(bundle_path)

    index = index.replace(old_name + ".js", new_name + ".js")
    open(index_path, "w", encoding="utf-8").write(index)

    sw = replace_once(sw, old_name + ".js", new_name + ".js", "bundle entry in sw.js")
    md5 = hashlib.md5(index.encode("utf-8")).hexdigest()
    sw, n = re.subn(r'\{url:"\./",revision:"[0-9a-f]+"\}', '{url:"./",revision:"%s"}' % md5, sw)
    if n != 1:
        sys.exit("patch-ui: index entry not found in sw.js")
    open(sw_path, "w", encoding="utf-8").write(sw)
    print(f"patch-ui: added Minimal ({budget} tokens), bundle {old_name}.js -> {new_name}.js")


if __name__ == "__main__":
    main()
