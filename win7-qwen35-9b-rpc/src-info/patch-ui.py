#!/usr/bin/env python3
# Adds a small "Minimal" thinking level to the official llama.cpp web UI (prebuilt dist).
#
# The Reasoning effort menu offers Off / Low (512) / Medium (2048) / High (8192) / Max.
# 512 tokens is long at ~2.5 t/s, so this inserts "Minimal" (default 256 tokens) after Off.
# It also pre-approves the browser-only tools (get_datetime, get_info, run_javascript).
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
    budget = int(sys.argv[2]) if len(sys.argv) > 2 else 256

    index_path = os.path.join(dist, "index.html")
    sw_path = os.path.join(dist, "sw.js")
    index = open(index_path, encoding="utf-8").read()
    sw = open(sw_path, encoding="utf-8").read()

    m = re.search(r"_app/immutable/(bundle\.[A-Za-z0-9_-]+)\.js", index)
    if not m:
        sys.exit("patch-ui: bundle not found in index.html")
    old_name = m.group(1)
    if "-q" in old_name or old_name.endswith("-min"):
        sys.exit("patch-ui: already patched")
    bundle_path = os.path.join(dist, "_app", "immutable", old_name + ".js")
    js = open(bundle_path, encoding="utf-8").read()

    # menu entry, after "Off"
    js = replace_once(js, '{label:"Off",value:ss.OFF},{label:"Low",value:ss.LOW}',
                      '{label:"Off",value:ss.OFF},{label:"Minimal",value:"minimal"},{label:"Low",value:ss.LOW}',
                      "reasoning level list")
    # token budget (the menu text "Max N tokens" is derived from it)
    js = replace_once(js, "{[ss.HIGH]:8192,[ss.LOW]:512,",
                      "{minimal:%d,[ss.HIGH]:8192,[ss.LOW]:512," % budget, "reasoning budget table")

    # pre-approve the tools that run only inside the browser (clock, runtime info, sandboxed JavaScript),
    # so users are not asked "Allow use of ...?" on every call; server / MCP tools still ask
    js = replace_once(js, "{const e=localStorage.getItem(r9);if(e)for",
                      '{["browser:get_datetime","browser:get_info","browser:run_javascript"].forEach(k=>this._tools.add(k));'
                      "const e=localStorage.getItem(r9);if(e)for", "permission store initializer")

    # name the bundle after its content: a cached older patched bundle can never be mistaken for this one
    new_name = old_name + "-q" + hashlib.md5(js.encode("utf-8")).hexdigest()[:8]
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
    print(f"patch-ui: added Minimal ({budget} tokens) and pre-approved browser tools, bundle {old_name}.js -> {new_name}.js")


if __name__ == "__main__":
    main()
