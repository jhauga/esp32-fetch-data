# Published firmware (work in progress)

This folder holds the over-the-air (OTA) build that a device pulls in `proxy` and
`periodic` modes. It is produced by the `Build Firmware` GitHub Actions workflow
and is **not** edited by hand.

> **WIP:** the pull-based OTA pipeline is still being finalized. The push (window)
> OTA path is the supported way to update today; see
> [docs/installation.md](../docs/installation.md).

## The build switch

A workflow run builds firmware only when a switch file says so, so ordinary
commits never rebuild or overwrite the published image:

1. The run checks for `firmware/on` containing `1`.
2. If set, it builds `firmware.bin`, writes `manifest.json` and `checksums.txt`
   here, deletes `firmware/on`, and commits the result.
3. Otherwise the run does nothing.

Trigger a build:

```bash
echo 1 > firmware/on
git add firmware/on
git commit -m "build firmware"
git push
```

## What a device pulls

Point `ota.proxyUrl` in `config.json` at the raw `manifest.json` URL for your
fork, for example:

```text
https://raw.githubusercontent.com/<owner>/<repo>/main/firmware/manifest.json
```

`manifest.json` looks like:

```json
{
  "version": "2026-01-01T00:00:00Z",
  "url": "https://raw.githubusercontent.com/<owner>/<repo>/main/firmware/firmware.bin"
}
```

The device compares `version` (an ISO-8601 UTC timestamp) against the last
installed one and updates only when it is newer.

## Why publishing this is safe

`firmware.bin` carries no WiFi credentials. The image only contains the public
simulator default network; real credentials are provisioned to the device's NVS
on the first USB flash and are never compiled into the binary.
