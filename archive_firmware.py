# PlatformIO post-script: after every successful link, keep the firmware.elf of the build in
# firmware-archive/ (local only, git-ignored), named by the ELF id the firmware itself reports
# (first 9 hex digits of the ELF's SHA-256: "Firmware ELF ..." on /debug/coredump, the
# running-ELF id in the boot log). So a core dump can always be resolved against the exact ELF.
#
#   firmware-archive/builds/<id>/   every build: firmware.elf.gz, firmware.bin, meta.json,
#                                   dirty.patch (uncommitted changes, if any)
#   firmware-archive/<date>_<tag>/  additionally for a clean build of exactly a tag: plain
#                                   binaries + README.md, and a row in firmware-archive/INDEX.md
#
# Only untagged builds are pruned (newest KEEP_UNTAGGED stay). Find one: Tools/fwarchive.sh <id>
# Errors never fail the build.
import datetime, gzip, hashlib, json, os, shutil, subprocess

Import("env")

KEEP_UNTAGGED = 30
ROOT = env.subst("$PROJECT_DIR")
ARCH = os.path.join(ROOT, "firmware-archive")


def git(*args):
    r = subprocess.run(["git", "-C", ROOT, *args], stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True)
    return r.stdout.strip() if r.returncode == 0 else ""


def archive(source, target, env):
    try:
        bdir = env.subst("$BUILD_DIR")
        elf = os.path.join(bdir, "firmware.elf")
        if not os.path.exists(elf):
            return
        sha = hashlib.sha256(open(elf, "rb").read()).hexdigest()
        ident = sha[:9]
        dest = os.path.join(ARCH, "builds", ident)
        if os.path.isdir(dest):
            print("firmware-archive: build %s already archived" % ident)
            return
        os.makedirs(dest)
        with open(elf, "rb") as f, gzip.open(os.path.join(dest, "firmware.elf.gz"), "wb", 6) as g:
            shutil.copyfileobj(f, g)
        for name in ("firmware.bin", "firmware.factory.bin"):
            if os.path.exists(os.path.join(bdir, name)):
                shutil.copy2(os.path.join(bdir, name), dest)
        describe = git("describe", "--tags", "--always")
        patch = git("diff", "HEAD")
        exact_tag = git("describe", "--tags", "--exact-match") if not patch else ""
        meta = dict(id=ident, sha256=sha, env=env.subst("$PIOENV"), commit=git("rev-parse", "HEAD"),
                    describe=describe, branch=git("rev-parse", "--abbrev-ref", "HEAD"), dirty=bool(patch),
                    tag=exact_tag, built=datetime.datetime.now().isoformat(timespec="seconds"))
        json.dump(meta, open(os.path.join(dest, "meta.json"), "w"), indent=1)
        if patch:
            open(os.path.join(dest, "dirty.patch"), "w").write(patch + "\n")
        print("firmware-archive: build %s (%s%s)" % (ident, describe, ", dirty" if patch else ""))
        if exact_tag:
            tagged(meta, bdir)
        prune()
    except Exception as e:  # never break a build over the archive
        print("firmware-archive: skipped (%s)" % e)


def tagged(meta, bdir):
    folder = "%s_%s_%s" % (meta["built"][:10], meta["tag"], meta["env"])
    dest = os.path.join(ARCH, folder)
    if os.path.isdir(dest):
        return
    os.makedirs(dest)
    for name in ("firmware.elf", "firmware.bin", "firmware.factory.bin", "bootloader.bin", "partitions.bin"):
        if os.path.exists(os.path.join(bdir, name)):
            shutil.copy2(os.path.join(bdir, name), dest)
    open(os.path.join(dest, "README.md"), "w").write(
        "# TRGB-BikeComputer firmware %s (%s)\n\n"
        "- Git commit: `%s`, tag `%s`, branch `%s`\n- Environment: `%s`\n- ELF id: `%s` (as shown by /debug/coredump)\n"
        "- Built: %s\n\nFlash: `firmware.factory.bin` at offset `0x0` (`esptool --chip esp32s3 write_flash 0x0 firmware.factory.bin`),\n"
        "or `firmware.bin` via the device's `/update` page. Copy also in `builds/%s/`.\n\n"
        "Add what changed in this release here.\n"
        % (meta["tag"], meta["env"], meta["commit"], meta["tag"], meta["branch"], meta["env"], meta["id"], meta["built"], meta["id"]))
    index = os.path.join(ARCH, "INDEX.md")
    row = "| %s | [`%s/`](%s/) | `%s` (commit `%s`) | `%s` | (release notes in README) |\n" % (
        meta["built"][:10], folder, folder, meta["tag"], meta["commit"][:7], meta["env"])
    if os.path.exists(index):
        text = open(index).read()
        marker = "\n## Conventions"
        text = text.replace(marker, row.rstrip("\n") + "\n" + marker, 1) if marker in text else text + row
        # keep the table contiguous: the row must follow the last table line
        open(index, "w").write(text.replace("|\n\n" + row.rstrip("\n"), "|\n" + row.rstrip("\n"), 1))


def prune():
    root = os.path.join(ARCH, "builds")
    old = []
    for d in os.listdir(root):
        try:
            m = json.load(open(os.path.join(root, d, "meta.json")))
        except Exception:
            continue
        if not m.get("tag"):
            old.append((m["built"], d))
    for _, d in sorted(old)[:-KEEP_UNTAGGED]:
        shutil.rmtree(os.path.join(root, d), ignore_errors=True)


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", archive)
