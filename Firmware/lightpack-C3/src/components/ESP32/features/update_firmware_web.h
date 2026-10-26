#if !defined(UPDATE_FIRMWARE_WEB)
#define UPDATE_FIRMWARE_WEB

#include "shared/dependencies.h"

#include <Update.h>
//? esp_ota_get_next_update_partition() and esp_ota_get_running_partition().
//! Update.h only pulls in esp_partition.h, so the OTA ops come from here.
#include <esp_ota_ops.h>

//? ---------------------- WEB FIRMWARE UPDATE ----------------------
//? The device page accepts a firmware.bin and writes it to the INACTIVE OTA
//? slot, then reboots into it. This is the same operation as ArduinoOTA, driven
//? from the browser with nothing installed on the machine: fetch the
//? firmware.bin that PlatformIO leaves in .pio/build/<env>/, hand it to the
//? page, and the board comes back running it.
//?
//? Layout of the endpoint:
//?   GET  /api/update  -> what the target slot is, and how big an image fits
//?   POST /api/update  -> multipart/form-data body, one file part; the reply is
//?                        {"ok":true,...} before the board reboots
//?
//? Why multipart and not a raw POST body: the ESP32 WebServer only runs its
//? upload callback from _parseForm(), i.e. for multipart/form-data. A raw
//? application/octet-stream body would be handed to the arg parser instead and
//? the handler would never see a byte. The page therefore sends FormData.
//?
//? Safety, in the order the code applies it:
//?   1. Update.begin() targets esp_ota_get_next_update_partition() - the slot
//!      the bootloader is NOT running. The running image is never written.
//?   2. The image magic byte is checked as the first sector is buffered, and the
//!      first 16 bytes are withheld until the end, so an aborted or bogus upload
//?      cannot leave a bootable-looking partial image behind.
//?   3. Only Update.end() sets the boot partition, and it verifies the image
//!      (header, length, checksum) first. A failure there leaves the device
//!      running exactly what it was running before.
//?   4. NVS is untouched by all of this, so the mode, palette and presets
//?      survive the update.
//!
//! Two things this does NOT do, deliberately:
//!   * it is unauthenticated, exactly like the rest of this page (see the note
//!     at the top of services/webpage.h). Anyone who can reach port 80 can
//!     install firmware - on a trusted LAN that is the design, off one it is
//!     remote code execution;
//!   * it does not roll back. Update::end() marks the new image bootable and the
//!     Arduino core never marks an app valid, so canRollBack()/rollBack() stay
//!     inert without CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE. A new image that
//!     boots but cannot join WiFi has to be recovered over USB, so keep the
//!     last-known-good firmware.bin around.

//* Browser-facing behaviour. The reboot is what the page waits for, so the
//? response is sent first and the restart is deferred to webpage_update_poll(),
//? which runs from webpage_loop() after the reply has been flushed.
#define WEBPAGE_UPDATE_REBOOT_DELAY_MS 1500

//* Upload state, written by the upload callback and read by the two handlers.
//! The whole multipart body is consumed inside a single web_server.handleClient()
//! call, so loop() - the strip, the gyro, the button - is frozen for the length
//! of the transfer. Nothing else in the firmware runs during an upload, which is
//! also why no lock is needed between this and ArduinoOTA.handle().
bool webpage_update_active = false;
bool webpage_update_failed = false;
bool webpage_update_offered = false;
String webpage_update_error = "";
size_t webpage_update_bytes = 0;
uint32_t webpage_update_reboot_at = 0;

void webpage_handle_update_upload()
{
    HTTPUpload &upload = web_server.upload();

    if (upload.status == UPLOAD_FILE_START)
    {
        webpage_update_active = false;
        webpage_update_failed = false;
        webpage_update_offered = true;
        webpage_update_error = "";
        webpage_update_bytes = 0;

        Serial.printf("[WEB] Firmware upload: '%s' (%s)\n",
                      upload.filename.c_str(), upload.type.c_str());

        //! First of the refusals, and for a request nobody authenticated the only
        //? one that matters: this callback runs WHILE the body is still arriving,
        //! so it is the last point at which an image can be refused before a byte
        //? of it reaches the flash. Silent on purpose - a 401 written from inside
        //! the body parser would go out over a socket the client is mid-upload on,
        //* and the gated completion handler sends it once the body is consumed.
        if (!webpage_auth_allows())
        {
            webpage_update_failed = true;
            webpage_update_error = F("authentication required");
            Serial.println("[WEB] Refused: not authenticated");
            return;
        }

        //! A restart is already pending from a previous upload. Refuse rather
        //! than start a second one: the completed upload has just switched the
        //! boot partition, so the slot that is "next" now is the one this board
        //! is RUNNING from, and an interrupted write there is a brick.
        //! The page disables its own button, so this only catches a second tab.
        if (webpage_update_reboot_at != 0)
        {
            webpage_update_failed = true;
            webpage_update_error = F("installed firmware is waiting for a restart");
            Serial.println("[WEB] Refused: a restart into new firmware is pending");
            return;
        }

        //! Do not touch the Update object while another transfer owns it. In
        //! practice ArduinoOTA.handle() cannot be mid-flight here (loop() is
        //! blocked in this very call), but a second browser tab uploading at the
        //! same time would otherwise have the two writes interleave into one slot.
        if (Update.isRunning())
        {
            webpage_update_failed = true;
            webpage_update_error = F("another update is already in progress");
            Serial.println("[WEB] Refused: Update is already running");
            return;
        }

        const esp_partition_t *target = esp_ota_get_next_update_partition(NULL);
        if (target == nullptr)
        {
            //! Reached when the partition table has no second app slot. The
            //! shipped default.csv table does, so this means the table was
            //! replaced - and no OTA path of any kind would work either.
            webpage_update_failed = true;
            webpage_update_error = F("no inactive OTA partition to write");
            Serial.println("[WEB] Refused: no OTA partition");
            return;
        }

        //? UPDATE_SIZE_UNKNOWN, not the Content-Length: the header is not the
        //? image size in a multipart body, and Update caps _size at the target
        //? partition anyway. An image larger than the slot fails at write time,
        //? which the page pre-empts by asking GET /api/update first.
        //! Update allocates its 4 KB sector buffer on the heap here.
        if (!Update.begin(UPDATE_SIZE_UNKNOWN))
        {
            webpage_update_failed = true;
            webpage_update_error = Update.errorString();
            Serial.printf("[WEB] Refused: Update.begin failed - %s\n", Update.errorString());
            return;
        }

        webpage_update_active = true;
        Serial.printf("[WEB] Writing to '%s' at 0x%08X (%u KB)\n", target->label,
                      (unsigned)target->address, (unsigned)(target->size / 1024));

        //* Physical progress. The strip is frozen for the duration of the
        //? upload, so the onboard LED is the only indication that anything is
        //? happening; update_onboard_LED() repaints it once loop() resumes.
        onboard_led.setPixelColor(0, strip.Color(0, 0, 60));
        onboard_led.show();
    }
    else if (!webpage_update_active)
    {
        //* Every other status is meaningless once the start was refused: the
        //* bytes still arrive, and they have nowhere to go.
        return;
    }
    else if (upload.status == UPLOAD_FILE_WRITE)
    {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
        {
            webpage_update_failed = true;
            webpage_update_error = Update.errorString();
            webpage_update_active = false;
            Serial.printf("[WEB] Write failed at %u bytes: %s\n",
                          (unsigned)webpage_update_bytes, webpage_update_error.c_str());
            //! Release the partition before the rest of the body is thrown away,
            //! or the next upload would be refused by the isRunning() guard.
            Update.abort();
            return;
        }
        webpage_update_bytes += upload.currentSize;
    }
    else if (upload.status == UPLOAD_FILE_END)
    {
        //* totalSize is what the parser counted, and is authoritative: it
        //* includes any byte the final partial buffer picked up.
        webpage_update_bytes = upload.totalSize;
        webpage_update_active = false;

        //? end(true) accepts the short final buffer (the image is not a multiple
        //? of the 4 KB sector), verifies the image and switches the boot
        //? partition. On failure the error stays set and Update has already
        //? aborted itself, so the running image is untouched.
        if (!Update.end(true))
        {
            webpage_update_failed = true;
            webpage_update_error = Update.errorString();
            Serial.printf("[WEB] Rejected after %u bytes: %s\n",
                          (unsigned)webpage_update_bytes, webpage_update_error.c_str());
            return;
        }

        Serial.printf("[WEB] Firmware accepted: %u bytes, md5 %s - rebooting into it\n",
                      (unsigned)webpage_update_bytes, Update.md5String().c_str());
    }
    else if (upload.status == UPLOAD_FILE_ABORTED)
    {
        Update.abort();
        webpage_update_active = false;
        webpage_update_failed = true;
        webpage_update_error = F("upload was interrupted before it finished");
        Serial.println("[WEB] Firmware upload aborted by the client");
    }
}

//* Registered as the POST handler for /api/update, and runs once the whole body
//* has been parsed (i.e. after every upload callback above).
void webpage_handle_update_done()
{
    //* A POST that carried no file part at all never ran the upload callback, so
    //* there is nothing to report but the absence: without this the reply would
    //* be a cheerful ok:true with an MD5 of nothing.
    if (!webpage_update_offered)
    {
        webpage_update_failed = true;
        webpage_update_error = F("no firmware file part in the request");
    }

    String json;
    json.reserve(220);
    json += F("{\"ok\":");
    json += webpage_update_failed ? F("false") : F("true");
    json += F(",\"bytes\":");
    json += String((unsigned)webpage_update_bytes);

    if (webpage_update_failed)
    {
        json += F(",\"error\":\"");
        json += webpage_json_escape(webpage_update_error.c_str());
        json += F("\"");
        Serial.printf("[WEB] Firmware update FAILED: %s\n", webpage_update_error.c_str());
    }
    else
    {
        //* The MD5 of the image the device actually accepted, so the page can
        //* show something to compare against the file's own checksum.
        json += F(",\"md5\":\"");
        json += Update.md5String();
        json += F("\"");
        webpage_update_reboot_at = millis() + WEBPAGE_UPDATE_REBOOT_DELAY_MS;
    }
    json += F("}\n");

    //? 200 even on failure: the page reads the `ok` flag, and a 5xx here would
    //? only be reported by the browser as a network error with the body lost.
    web_server.send(200, "application/json", json);

    //* Close the connection while the reply is still hot. Without this the
    //? client keeps the socket open for a keep-alive the restart is about to
    //? take away, and the page shows a network error instead of the result.
    web_server.client().flush();
    web_server.client().stop();
}

//* GET /api/update: what the page needs before it offers a file.
void webpage_handle_update_info()
{
    const esp_partition_t *target = esp_ota_get_next_update_partition(NULL);
    const esp_partition_t *running = esp_ota_get_running_partition();

    String json;
    json.reserve(200);
    json += F("{\"ok\":true,\"max_bytes\":");
    json += String(target ? (unsigned)target->size : 0);
    json += F(",\"partition\":\"");
    json += target ? target->label : "none";
    json += F("\",\"running\":\"");
    json += running ? running->label : "unknown";
    json += F("\",\"running_bytes\":");
    //* What the current image costs, so the page can say how much of a slot a
    //? firmware.bin of the user's size would leave. ESP.getSketchSize() is the
    //? live app's own size, which is the number that grows release to release.
    json += String((unsigned)ESP.getSketchSize());
    json += F(",\"reboot_delay_ms\":");
    json += String(WEBPAGE_UPDATE_REBOOT_DELAY_MS);
    json += F("}\n");

    web_server.send(200, "application/json", json);
}

//* Called from webpage_loop(), every pass. The restart is deferred rather than
//? done inside webpage_handle_update_done() so the HTTP reply is on the wire
//? before the radio goes down - the page relies on reading it.
void webpage_update_poll()
{
    if (webpage_update_reboot_at == 0)
        return;

    if ((int32_t)(millis() - webpage_update_reboot_at) < 0)
        return;

    webpage_update_reboot_at = 0;
    Serial.println("[WEB] Restarting into the new firmware");
    Serial.flush();
    ESP.restart();
}

#endif // UPDATE_FIRMWARE_WEB
