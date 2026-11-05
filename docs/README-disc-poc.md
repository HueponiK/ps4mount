# PS4Mount retail-disc access probe (Stage 0)

**Status:** experimental, read-only diagnostic. This does **not** run retail PS4 games,
mount encrypted PFS volumes, change the PlayStation installation database, or bypass
license checks. It has only been compiled and tested with synthetic packages on a Linux
host. A PS5 payload has **not** been built or tested.

Upstream: <https://github.com/gasb07/ps4mount> (gasb07, GPL-3.0-or-later).
This experimental branch is based on `HueponiK/ps4mount` and preserves the existing
project's source and license notices.

## The specific hypothesis

PS4Mount already mounts internal directories containing symlinks to packages on exFAT
USB drives over `/user/app/<TITLE_ID>` with `nullfs`. Before touching registration,
we need evidence that the **retail** optical PKG can be opened as a normal file and
that seek-based PFS reads work through the PS5's view of the disc.

The expected optical layout, based on PS4 technical references, is:

    /mnt/disc/app/CUSAxxxxx/app.pkg

**This path is not confirmed on a jailbroken PS5.** If it is different, the probe
will report that `app/` cannot be listed, not try to mount the drive automatically.

## Package contents

- Existing `build.sh`, `src/main.c` and `test/run.sh` integrate the probe.
- `src/disc_probe.[ch]`: bounded header parsing and two direct `pread` tests,
  with strict offsets and error handling.
- `test/disc_probe_test.c`: synthetic valid/invalid PKG fixture tests.
- `tools/disc_probe_cli.c`: portable host diagnostic launcher for the same code.
- `test/run_disc_probe.sh`: compile and test without PS5 SDK.

The branch adds a **disabled-by-default** diagnostic to `main.c`. It runs once at
startup only when `/data/ps4mount/disc_probe.enable` exists. Its output goes to
PS4Mount's usual log. Nothing in the new code modifies game files, mount tables,
licenses, system databases, or system storage. Note that **the original PS4Mount**
continues to have its own registration/mounting behaviour when run normally.

## Building and testing

In a checkout of this branch:

```sh
bash test/run.sh
bash test/run_disc_probe.sh
```

To build the console ELF, install the PS5 Payload SDK in `/opt/ps5-payload-sdk`
(or set `PS5_PAYLOAD_SDK`) and run `./build.sh`. Console compilation and hardware
behaviour have **not** been verified.

### Running the read-only diagnostic on a PS5

1. Back up your save data. Upstream PS4Mount itself warns about database corruption
   and possible destructive rest-mode behaviour on some jailbreak configurations.
2. Disable automatic rest mode before running PS4Mount, as instructed upstream.
3. Create an **empty** `/data/ps4mount/disc_probe.enable` marker on the console.
4. Insert an original PS4 disc, then start your locally built PS4Mount payload with
   your existing ELF loader.
5. Inspect the `[disc-probe]` lines in `/data/ps4mount/log.txt`.
6. Delete the marker to return to normal PS4Mount behaviour.

Do **not** install these experimental changes on a console with irreplaceable data.
The diagnostic itself is read-only, but the host payload and firmware environment
are not risk-free.

## Interpreting logs

- `cannot list /mnt/disc/app`: the assumed path is absent, unreadable or different;
  not evidence that the disc cannot be used.
- `header OK`: a plausible `\x7fCNT` PKG header was read at optical file offset 0.
- `direct offset reads OK`: two separate 4096-byte reads succeeded in the PFS
  region of the original package, without copying it.
- `invalid PKG header`: either the layout differs, the file is unreadable/damaged,
  or this strict proof-of-concept parser is missing a variant.

Success validates **read access**, not executable mounting, decryption, licence
verification, playable performance or no-install launch. Sony's retail license
handling remains a separate blocker in PS4Mount's fake-PKG-specific metadata path.

## Local testing

```sh
bash test/run_disc_probe.sh
# Or (after it builds):
build/test/disc_probe_cli /path/to/mock_disc_root
```

The test uses a small fake header and sparse file created in `/tmp`, not actual game
data or cryptographic keys. `disc_probe_scan()` never writes to its input path.

## Next engineering gates (not implemented)

1. Confirm optical package path and read permissions on real PS5 hardware.
2. Compare retail-disc `param.sfo` / signed metadata paths with PS4Mount's fake-PKG
   install metadata generator; avoid fake-key paths when using retail media.
3. Check whether `nullfs` + a symlink can expose the optical package at a test-only
   mountpoint. **Do not** mount over `/user/app` during initial experimentation.
4. Identify a console-native registration mechanism that preserves normal disc
   authentication, without generating fake licenses or bypassing retail ownership checks.
5. Only then test cold launch, read latency, updates, hot-unplug/error handling,
   and cleanup. A playable no-install solution is not yet demonstrated.

## License

New C source files and integration changes are under GPL-3.0-or-later to match
PS4Mount. Retain the upstream repository's license and notices when distributing
modified copies.
