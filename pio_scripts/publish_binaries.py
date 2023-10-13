import shutil
import os
import json
Import('env')

PIOENV = env.get("PIOENV")
VERSION = os.getenv("VERSION", "")
BOARD_CONFIG = env.BoardConfig()
APP_BIN = "{}{}{}.bin".format(
    env.get("BUILD_DIR"), os.path.sep, env.get("PROGNAME"))
PUBLISH_DIR_BASE = "publish{}".format(os.path.sep)
PUBLISH_DIR_FULL = "{}{}{}".format(PUBLISH_DIR_BASE, PIOENV, os.path.sep)
PUBLISH_BIN = "{}ATEM_tally_light_{}_{}.bin".format(
    PUBLISH_DIR_FULL, VERSION, PIOENV)

def _get_cpp_define_value(env, define: str):
    define_list = [(item[-1] if item[0] == define else "")
                   for item in env["CPPDEFINES"] if item == define or item[0] == define]
    if define_list:
        return define_list[0]

    return None


def _create_publish_dir():
    if not os.path.isdir(PUBLISH_DIR_BASE):
        os.mkdir(PUBLISH_DIR_BASE)

    if not os.path.isdir(PUBLISH_DIR_FULL):
        os.mkdir(PUBLISH_DIR_FULL)


def _esp_webtools_manifest(env, chip_family: str, flash_parts):
    manifest = {
        "name": env.GetProjectOption("custom_web_flasher_name"),
        "version": VERSION,
        "new_install_prompt_erase": True,
        "builds": [{
            "chipFamily": chip_family.replace('\\"', ''),
            "parts": [
                {
                    "path": "{}/{}".format(PIOENV, os.path.basename(image[1])),
                    "offset": int(image[0], 16)
                } for image in flash_parts]
        }]
    }

    with open("{}manifest.json".format(PUBLISH_DIR_FULL), "w") as outfile:
        json.dump(manifest, outfile)


def bin_rename_copy(source, target, env):
    print(os.getenv("PUBLISH"))
    _create_publish_dir()

    chip_family = _get_cpp_define_value(env, "CHIP_FAMILY")
    flash_images = env.get("FLASH_EXTRA_IMAGES", [])

    # else:  # esp8266
    for image in flash_images:
        shutil.copy(image[1], PUBLISH_DIR_FULL)
    shutil.copy(str(target[0]), PUBLISH_BIN)

    _esp_webtools_manifest(env, chip_family, flash_images +
                           [(env.get("ESP32_APP_OFFSET", "0x00"), PUBLISH_BIN)])

if (os.getenv("PUBLISH") is not None):
    env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", [bin_rename_copy])

