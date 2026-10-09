"""Exercise the portable profile and overlays with RetroArch's real config parser."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
COMMON = ROOT / "libretro-common"
SOURCES = [
    "compat/fopen_utf8.c", "compat/compat_strl.c",
    "compat/compat_strcasestr.c", "compat/compat_posix_string.c",
    "encodings/encoding_utf.c", "file/file_path.c", "file/file_path_io.c",
    "file/config_file.c", "lists/string_list.c", "string/stdstring.c",
    "streams/file_stream.c", "vfs/vfs_implementation.c", "time/rtime.c",
]
PROBE = r'''
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <file/config_file.h>

static void expect(config_file_t *cfg, const char *key, const char *value)
{
   char *actual = NULL;
   if (!config_get_string(cfg, key, &actual) || !actual || strcmp(actual, value))
   {
      fprintf(stderr, "%s: expected %s, got %s\n", key, value,
            actual ? actual : "<missing>");
      exit(1);
   }
   free(actual);
}

int main(int argc, char **argv)
{
   config_file_t *cfg;
   const char *disabled[] = {"video_threaded", "pause_nonactive",
      "config_save_on_exit", "bundle_assets_extract_enable",
      "auto_overrides_enable", "auto_remaps_enable"};
   unsigned i;
   if (argc != 4 || !(cfg = config_file_new_from_path_to_string(argv[1])))
      return 2;
   expect(cfg, "audio_driver", "coreaudio3");
   expect(cfg, "input_driver", "cocoa");
   expect(cfg, "video_driver", "vulkan");
   expect(cfg, "video_use_metal_arg_buffers", "true");
   expect(cfg, "video_vsync", "true");
   expect(cfg, "stdin_cmd_enable", "true");
   for (i = 0; i < sizeof(disabled) / sizeof(*disabled); i++)
      expect(cfg, disabled[i], "false");
   if (!config_append_file(cfg, argv[2]))
      return 2;
   expect(cfg, "video_vsync", "false");
   expect(cfg, "video_driver", "vulkan");
   if (!config_append_file(cfg, argv[3]))
      return 2;
   expect(cfg, "video_vsync", "true");
   expect(cfg, "video_use_metal_arg_buffers", "false");
   expect(cfg, "input_driver", "cocoa");
   config_file_free(cfg);
   puts("PASS: macOS base, per-session overlay, explicit extra and duplicate-key precedence");
   return 0;
}
'''


def main():
    profile = ROOT / "profiles/agent-control-macos.cfg"
    with tempfile.TemporaryDirectory(prefix="retroarch-profile-") as directory:
        scratch = Path(directory)
        probe = scratch / "probe.c"
        probe.write_text(PROBE)
        session = scratch / "session.cfg"
        session.write_text('video_vsync = "false"\n')
        extra = scratch / "extra.cfg"
        extra.write_text('video_vsync = "true"\nvideo_vsync = "false"\n'
                         'video_use_metal_arg_buffers = "false"\n')
        originals = {path: path.read_bytes() for path in (profile, session, extra)}
        binary = scratch / "probe"
        subprocess.run([*shlex.split(os.environ.get("CC", "cc")), "-std=gnu99",
                        "-I", str(COMMON / "include"), str(probe),
                        *[str(COMMON / source) for source in SOURCES],
                        "-o", str(binary)], check=True)
        subprocess.run([str(binary), str(profile), str(session), str(extra)], check=True)
        assert all(path.read_bytes() == before for path, before in originals.items())


if __name__ == "__main__":
    main()
