# Firmware installer — how to run it

`install/index.html` is a browser flasher for a **fresh, blank Lux et Portabilis V2 board**
(ESP32-C3 SuperMini). It uses [ESP Web Tools](https://esphome.github.io/esp-web-tools/) over the
Web Serial API, so there is nothing to install on the computer: no Python, no esptool, no
PlatformIO, and no USB-serial driver (the C3's USB is native CDC).

It reads the binaries that are already committed under `Firmware/lightpack-C3/releases/` and
writes them through the ESP32-C3 ROM bootloader.

---

## For the person flashing a board

You only need a URL, a cable, and the right browser.

| Requirement | Detail |
|---|---|
| Browser | **Chrome or Edge, desktop.** Firefox and Safari have no Web Serial; no browser on iOS or Android does. |
| Cable | A **data** USB-C cable. Charge-only cables enumerate nothing and are the most common cause of "no port appears". |
| Page origin | **https:// or localhost.** Both count as a secure context; a handed-out `file://` copy does not. |
| Internet | Only to load the ~40 KB installer component from the CDN. The firmware itself is served from the same origin as the page. |

Steps:

1. Open the installer page (see *Where it lives* below).
2. Plug the board in. It does **not** need any button pressed — a blank C3 already sits in its
   ROM bootloader. If no port appears, hold **BOOT**, tap **RESET**, release **BOOT**, and try
   again.
3. Click **Install V0.4-pre** → pick the port in the browser dialog → confirm.
4. When it finishes, the board reboots. First boot: join the Wi-Fi hotspot **`Lightpack-Devkit`**
   and open <http://lightpack.local> to configure it.

Full flashing detail, offsets and checksums live in
[`Firmware/lightpack-C3/releases/V0.4-pre/FLASHING.md`](../Firmware/lightpack-C3/releases/V0.4-pre/FLASHING.md).

## Where it lives

**Published (normal case):** if GitHub Pages is enabled for this repository, the page is at

```
https://<user>.github.io/lux-et-tenebris/install/
```

To enable it: repository **Settings → Pages → Source: Deploy from a branch → Branch: `main`,
Folder: `/ (root)`**. Nothing else is needed — the page and the binaries it fetches are both in
the repository, which keeps them on one origin (see *Why the binaries are committed*).

**Running it locally**, which is the answer for a workshop with no network or when you do not want
to enable Pages. Serve the **repository root**, not the `install/` folder, because the manifest
paths reach into `Firmware/`:

```bash
git clone https://github.com/machadoleonardo94/lux-et-tenebris.git
cd lux-et-tenebris
python -m http.server 8000
```

then open <http://localhost:8000/install/>.

Any static server works (`npx serve .`, `php -S localhost:8000`); `localhost` is a secure context,
which is why Web Serial is permitted there even without https. Opening `install/index.html`
straight off disk will not work — the page detects `file://` and says so.

**On Linux** the user needs access to the USB device: be in the `dialout` group (or install an
`esp32` udev rule), otherwise the port appears but opening it fails.

---

## For maintainers

### Adding a release

1. Build and drop the binaries in a new folder, e.g. `Firmware/lightpack-C3/releases/V0.5/`
   (the merged factory image is what the browser flasher wants — one part at offset `0`).
2. Copy `V0.4-pre/manifest.json` into it and change `version` plus the `path`:

   ```json
   {
     "name": "lightpack-C3",
     "version": "V0.5",
     "builds": [
       { "chipFamily": "ESP32-C3",
         "parts": [ { "path": "lightpack-C3-V0.5-factory.bin", "offset": 0 } ] }
     ]
   }
   ```

3. Add a second button in `install/index.html` pointing at the new manifest, and label the
   existing one as the previous release. Offsets are **decimal** in the manifest, and `path` is
   relative to the manifest file.
4. Keep the old manifest in place: users on an older revision may need to reflash what they had.

`chipFamily` must be exactly `ESP32-C3`; the component matches it against the detected chip and
refuses a mismatch, which is what stops someone from writing this image into an ESP32-WROOM board.

### Why the binaries are committed

ESP Web Tools fetches the `.bin` files with `fetch()`, so they must be **same-origin** with the
page. GitHub *Release assets* are a different origin and are blocked by CORS — which is why the
release folders are version-controlled rather than attached to a GitHub Release only. Attaching
them to a Release as well is still useful for plain downloads; it just cannot be the flasher's
source.

### Version pinning

The page loads the component from a pinned CDN URL:

```html
<script type="module"
        src="https://unpkg.com/esp-web-tools@10.4.0/dist/install-button.js?module"></script>
```

Pinned so an upstream release cannot change flashing behaviour under an existing tag. If unpkg is
ever unreachable, the page shows its own "the installer component did not load" message instead of
an inert button. To make the page work with no network at all, download `dist/install-button.js`
and its `?module` dependencies once and point the `src` at a local copy — the component is
Apache-2.0, so vendoring is allowed; note that it pulls `esptool-js`, `lit` and `@material/web`, so
the vendored set is more than one file.

**If you do vendor it, Apache-2.0 then applies to the copy:** ship its `LICENSE` and `NOTICE`
alongside, state any modifications you made, and add the component to
[`THIRD-PARTY-NOTICES.md`](../THIRD-PARTY-NOTICES.md). Loading it from the CDN — what the page does
today — keeps the project free of that obligation entirely, which is the main reason it is not
vendored.
