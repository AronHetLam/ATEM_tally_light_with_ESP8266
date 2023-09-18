import subprocess
import shutil
import os
Import('env')

PIOENV = env.get("PIOENV")
VERSION = env.get("VERSION", "")
BOARD_CONFIG = env.BoardConfig()
APP_BIN = "{}{}{}.bin".format(
    env.get("BUILD_DIR"), os.pathsep, env.get("PROGNAME"))
PUBLISH_DIR = "publish{}".format(os.path.sep)
PUBLISH_BIN = "{}ATEM_tally_light_{}_{}.bin".format(
    PUBLISH_DIR, VERSION, PIOENV)


def _get_cpp_define_value(env, define: str):
    define_list = [(item[-1] if item[0] == define else "")
                   for item in env["CPPDEFINES"] if item == define or item[0] == define]
    if define_list:
        return define_list[0]

    return None


def bin_rename_copy(source, target, env):
    if not os.path.isdir(PUBLISH_DIR):
        os.mkdir(PUBLISH_DIR)

    chip_family = _get_cpp_define_value(env, "CHIP_FAMILY") or ""

    if (chip_family.startswith("ESP32")):
        flash_images = env.Flatten(
            env.get("FLASH_EXTRA_IMAGES", [])) + [env.get("ESP32_APP_OFFSET"), str(target[0])]

        # Run esptool to merge images into a single binary
        env.Execute(" ".join(
            [
                    "esptool",
                    "--chip", BOARD_CONFIG.get("build.mcu", "esp32"),
                    "merge_bin",
                    "-o", PUBLISH_BIN,
                    *flash_images
                    ]
        ))

    else:  # esp8266
        shutil.copy(str(target[0]), PUBLISH_BIN)

if (_get_cpp_define_value(env, "PUBLISH") is not None):
    env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", [bin_rename_copy])

    env.Replace(
        UPLOADERFLAGS=[
                f
                for f in env.get("UPLOADERFLAGS")
                if f not in env.Flatten(env.get("FLASH_EXTRA_IMAGES"))
            ]
            + ["0x0", PUBLISH_BIN],
        UPLOADCMD='"$PYTHONEXE" "$UPLOADER" $UPLOADERFLAGS',
    )
