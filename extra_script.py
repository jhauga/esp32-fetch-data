# extra_script.py
# Build the LittleFS image from the repo root instead of a data/ folder.
#
# PlatformIO builds the filesystem image from PROJECT_DATA_DIR (default: data/).
# This script stages the root-level config.json into a build-time folder and
# points PROJECT_DATA_DIR at it, so config.json can live at the repo root while
# `pio run -t uploadfs` still writes it to the device filesystem as /config.json.
Import("env")  # noqa: F821  (provided by PlatformIO)

import os
import shutil

project_dir = env["PROJECT_DIR"]
staging_dir = os.path.join(env.subst("$BUILD_DIR"), "littlefs_image")

# Recreate a clean staging folder containing only filesystem files.
if os.path.isdir(staging_dir):
    shutil.rmtree(staging_dir)
os.makedirs(staging_dir)

config_src = os.path.join(project_dir, "config.json")
if os.path.isfile(config_src):
    shutil.copy(config_src, os.path.join(staging_dir, "config.json"))

env.Replace(PROJECT_DATA_DIR=staging_dir)
