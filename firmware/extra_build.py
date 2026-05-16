"""
extra_build.py — PlatformIO extra script

Explicitly compiles the mquickjs library C sources from lib/mquickjs/src/.
PlatformIO's library finder detects the dependency but does not always
generate compile commands for local-only C libraries; this script ensures
the four mquickjs translation units are always built and linked.
"""
Import("env")

from os.path import join

MQUICKJS_SRC = join(env.subst("$PROJECT_DIR"), "lib", "mquickjs", "src")

env.BuildSources(
    join(env.subst("$BUILD_DIR"), "lib_mquickjs"),
    MQUICKJS_SRC,
    src_filter=["+<*.c>"],
)
