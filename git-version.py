import subprocess

Import("env")

def get_firmware_specifier_build_flag():    
    #ret = subprocess.run(["git", "describe"], stdout=subprocess.PIPE, text=True) #Uses only annotated tags
    ret = subprocess.run(["git", "describe", "--tags"], stdout=subprocess.PIPE, text=True) #Uses any tags
    #ret = subprocess.run(["git", "describe", "--all"], stdout=subprocess.PIPE, text=True) #Uses any tags
    build_version = ret.stdout.strip().split('-')
    print ("Firmware Revision: " + str(build_version) + " Len:" + str(len(build_version)))
    if (len(build_version) > 1):
        build_flag = "-D GIT_VERSION=\\\"" + build_version[0] + "\\\"" + " -D COMMIT_VERSION=\\\"" + build_version[1] + "\\\"" + " -D GIT_HASH=\\\"" + build_version[2] + "\\\""
    else:
        build_flag = ""
    return (build_flag)

env.Append(
    BUILD_FLAGS=[get_firmware_specifier_build_flag()]
)
