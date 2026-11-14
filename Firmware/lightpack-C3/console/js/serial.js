//? Web Serial transport for the console.
//?
//? The C3's USB Serial/JTAG enumerates as a standard CDC-ACM port, so opening
//? it is the whole driver story on Windows and macOS. Linux needs the usual
//? dialout/udev access.
//!
//? What this module owns: opening the port, the read loop, ordered writes and
//? the line decoder. What it does NOT own: the handshake, request matching and
//? reconnecting - those live in app.js, because they are policy, not transport.

import { LineDecoder } from './protocol.js';

export class SerialLink {
  constructor({ onLine, onLog, onStatus }) {
    this.onLine = onLine;
    this.onLog = onLog || (() => {});
    this.onStatus = onStatus || (() => {});

    this.port = null;
    this.reader = null;
    this.writer = null;
    this.decoder = new LineDecoder();
    this.txChain = Promise.resolve();
    this.open = false;
    this.closing = false;

    this._boundDisconnect = (event) => {
      // SerialConnectionEvent carries the port in `port`; event.target is the
      // Serial object. Checking target would silently never match.
      const port = event.port || event.target;
      if (port === this.port && this.open) {
        this.onStatus({ state: 'disconnected', reason: 'The board was unplugged or reset.' });
      }
    };
    if ('serial' in navigator) {
      navigator.serial.addEventListener('disconnect', this._boundDisconnect);
    }
  }

  static get supported() {
    return 'serial' in navigator;
  }

  /**
   * Open an already-granted port. Throws with a readable message when the port
   * is gone or another program owns it (PlatformIO's monitor is the usual one).
   */
  async openPort(port) {
    if (!port) throw new Error('No serial port was chosen.');

    // Reopening the same SerialPort after a reset is allowed once the previous
    // reader/writer locks are released; drop them defensively.
    await this._releaseLocks().catch(() => {});

    this.port = port;
    this.closing = false;

    try {
      await port.open({
        baudRate: 115200,
        dataBits: 8,
        stopBits: 1,
        parity: 'none',
        flowControl: 'none',
      });
    } catch (error) {
      this.port = null;
      throw new Error(describeOpenError(error));
    }

    //! Do NOT call port.setSignals() here. On the ESP32-C3 USB Serial/JTAG,
    //! changing DTR/RTS resets the chip: measured on the bench board, DTR high
    //! at open resets it, a later low/low transition resets it again, and
    //! low/high puts it into download mode. Chrome's open may already assert
    //! the lines, so one reset is unavoidable; a second one on top of it is
    //! not. The handshake retries until the board has finished booting.
    this.writer = port.writable.getWriter();
    this.reader = port.readable.getReader();
    this.decoder.reset();
    this.open = true;

    this.onStatus({ state: 'connected', port });
    this._readLoop();
  }

  async _readLoop() {
    try {
      while (this.open) {
        const { value, done } = await this.reader.read();
        if (done) break;
        if (!value || value.length === 0) continue;
        for (const line of this.decoder.push(value)) this.onLine(line);
      }
    } catch (error) {
      if (this.open && !this.closing) {
        this.onStatus({ state: 'disconnected', reason: describeReadError(error) });
      }
    } finally {
      if (this.open && !this.closing) {
        // The reader ended without close() being called: the device went away.
        this.open = false;
        this.onStatus({ state: 'disconnected', reason: 'The serial stream ended.' });
      }
    }
  }

  /** Write one command line. Writes are chained so two edits cannot reorder. */
  async send(line) {
    if (!this.open || !this.writer) throw new Error('Not connected.');
    const bytes = new TextEncoder().encode(line);

    this.txChain = this.txChain.then(async () => {
      if (!this.open || !this.writer) throw new Error('Not connected.');
      await this.writer.write(bytes);
    });
    return this.txChain;
  }

  async _releaseLocks() {
    if (this.reader) {
      try {
        await this.reader.cancel();
      } catch {}
      try {
        this.reader.releaseLock();
      } catch {}
      this.reader = null;
    }
    if (this.writer) {
      try {
        await this.writer.close();
      } catch {}
      try {
        this.writer.releaseLock();
      } catch {}
      this.writer = null;
    }
  }

  async close() {
    if (!this.port) return;
    this.closing = true;
    this.open = false;

    await this._releaseLocks().catch(() => {});
    try {
      await this.port.close();
    } catch {}
    this.port = null;
    this.closing = false;
    this.onStatus({ state: 'closed' });
  }

  dispose() {
    if ('serial' in navigator) {
      navigator.serial.removeEventListener('disconnect', this._boundDisconnect);
    }
    return this.close();
  }
}

function describeOpenError(error) {
  const message = String(error?.message || error || 'unknown error');
  if (/access denied|permission|busy|failed to open/i.test(message)) {
    return 'The port is busy or access was denied. Close PlatformIO\'s monitor, ' +
      'the Arduino IDE or any other terminal that owns it, then try again.';
  }
  if (/no device|not found|disconnected/i.test(message)) {
    return 'The board is not on that port any more. Plug it in and choose the port again.';
  }
  return message;
}

function describeReadError(error) {
  const message = String(error?.message || error || 'unknown error');
  if (/device has been lost|disconnected|network/i.test(message)) {
    return 'The board went away. It may have reset or been unplugged.';
  }
  return message;
}
