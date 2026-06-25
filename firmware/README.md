# Published firmware (work in progress)

This folder holds only the build **switch** for the over-the-air (OTA) pipeline.
The built image a device pulls in `proxy` and `periodic` modes is **not** stored
here, or anywhere in the repo tree - it is published as assets on the rolling
`firmware-latest` GitHub Release by the `Build Firmware` GitHub Actions workflow.

> **WIP:** the pull-based OTA pipeline is still being finalized. The push (window)
> OTA path is the supported way to update today; see
> [docs/installation.md](../docs/installation.md).

## The build switch

A workflow run builds firmware only when a switch file says so, so ordinary
commits never rebuild or overwrite the published image:

1. The run checks for `firmware/on` containing `1`.
2. If set, it builds `firmware.bin`, uploads `firmware.bin`, `manifest.json`, and
   `checksums.txt` as assets on the `firmware-latest` release, then deletes
   `firmware/on` and commits **only that switch removal** back. No build artifact
   is committed.
3. Otherwise the run does nothing.

Trigger a build:

```bash
echo 1 > firmware/on
git add firmware/on
git commit -m "build firmware"
git push
```

## What a device pulls

Point `ota.proxyUrl` in `config.json` at the release `manifest.json` download URL
for your fork, for example:

```text
https://github.com/<owner>/<repo>/releases/download/firmware-latest/manifest.json
```

`manifest.json` looks like:

```json
{
  "version": "2026-01-01T00:00:00Z",
  "url": "https://github.com/<owner>/<repo>/releases/download/firmware-latest/firmware.bin"
}
```

The device compares `version` (an ISO-8601 UTC timestamp) against the last
installed one and updates only when it is newer. Release download URLs answer with
a redirect to the asset CDN; the firmware follows redirects, so the same URL works
on-device.

## Why publishing this is safe

`firmware.bin` carries no WiFi credentials. The image only contains the public
simulator default network; real credentials are provisioned to the device's NVS
on the first USB flash and are never compiled into the binary.
