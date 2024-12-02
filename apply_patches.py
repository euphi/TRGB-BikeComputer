from os.path import join, isfile

Import("env")

lib_name = "BMI160-Arduino"
lib_dir = join(env.subst("$PROJECT_LIBDEPS_DIR"), env["PIOENV"], lib_name)

patchflag_path = join(lib_dir, ".patching-done")

def _touch(path):
        with open(path, "w") as fp:
            fp.write("")


# patch file only if we didn't do it before
if not isfile(join(lib_dir, ".patching-done")):
    #original_file = join(FRAMEWORK_DIR, "variants", "standard", "pins_arduino.h")
    #patch_file = join("patches", "1-framework-arduinoavr-add-pin-a8.patch")
    del_file = join(lib_dir, "internal/ss_spi_101.cpp")

    #assert isfile(original_file) and isfile(patched_file)

    #env.Execute("patch %s %s" % (original_file, patched_file))
    env.Execute("rm %s" % (del_file))
    # env.Execute("touch " + patchflag_path)


    env.Execute(lambda *args, **kwargs: _touch(patchflag_path))


lib_name = "SparkFun BME280"

lib_dir = join(env.subst("$PROJECT_LIBDEPS_DIR"), env["PIOENV"], lib_name)
patch_file = "patches/SparkFunBME280.patch"
original_file = join(lib_dir,"src/SparkFunBME280.h")
patchflag_path = join(lib_dir, ".patching-done")
if not isfile(join(lib_dir, ".patching-done")):
    env.Execute("patch \"%s\" < %s" % (original_file, patch_file))

