# COM Port Locked on Windows (XIAO ESP32S3 Sense)

If `west flash` or `west espressif monitor` fails with **Access is denied** on COM16:

```text
* Connecting to COM16 ... - Opening COM16: Access denied
PermissionError(13, 'Access is denied.', None, 5)
```

something on **your PC** still has the serial port open. On Windows, **only one program can use a COM port at a time**.

---

## Important: unplugging the XIAO does not fix this

If you unplug the board for 10 seconds and plug it back in but **still** get Access denied, the lock is **not** on the device — it is a **leftover process on Windows** that never released COM16.

Unplug/replug only helps after you kill that process (or reboot).

---

## Primary cause in this project: leftover `idf_monitor`

`west espressif monitor` starts Espressif's **`idf_monitor`** (a Python process). It opens COM16 exclusively.

When **Cursor agent / automated scripts** run monitor for ~15–25 seconds and the session ends abruptly:

- The terminal or agent task stops
- **`idf_monitor` / `python` keeps running in the background**
- COM16 stays locked until you kill that process

This matches the pattern: port works fine until the agent flashes/monitors, then stays locked for you afterward.

**Not caused by:** the XIAO hardware, bad cable, or needing a longer unplug — unless a PC process is still holding the handle.

---

## Fix: kill the leftover monitor process (do this first)

### Option A — project script (recommended)

From the repo root, in a **new** PowerShell window:

```powershell
cd C:\Users\Brian\GitHub\xiao_esp32s3_sense_zephyr
.\scripts\kill-serial-monitor.ps1 -Port COM16
```

Then retry flash or monitor.

### Option B — manual PowerShell

Run in a **new** PowerShell window (not a stuck Cursor terminal):

```powershell
# Show what is locking the port (look for idf_monitor, esptool, COM16)
Get-CimInstance Win32_Process |
  Where-Object { $_.CommandLine -match 'idf_monitor|espressif|esptool|COM16' } |
  Select-Object ProcessId, Name, CommandLine | Format-List

# Kill all python processes running idf_monitor / esptool (Zephyr monitor)
Get-CimInstance Win32_Process |
  Where-Object {
    $_.Name -match '^python(\.exe)?$' -and
    $_.CommandLine -match 'idf_monitor|esptool'
  } |
  ForEach-Object {
    Write-Host "Killing PID $($_.ProcessId)"
    Write-Host $_.CommandLine
    Stop-Process -Id $_.ProcessId -Force
  }

Start-Sleep -Seconds 2
```

### Option C — Task Manager

1. Open **Task Manager** → **Details**
2. Find **python.exe** (may be several)
3. Right-click → **Properties** or add **Command line** column
4. End task on any row whose command line contains:
   - `idf_monitor`
   - `espressif`
   - `COM16`
   - `zephyrproject\modules\hal\espressif\tools\idf_monitor`

### Option D — Resource Monitor (when unsure which PID)

1. Run `resmon.exe`
2. **CPU** tab → **Associated Handles** → search `COM16`
3. Note the **Process** name and PID
4. End that process in Task Manager

---

## After killing the process

```powershell
$env:PATHEXT = ".PY;" + $env:PATHEXT
$env:ZEPHYR_SDK_INSTALL_DIR = "C:\Users\Brian\zephyr-sdk-0.17.2"
cd C:\zephyrproject\zephyr

west flash -- --esp-device COM16

# Only after flash finishes:
west espressif monitor -p COM16
```

Exit monitor properly with **`Ctrl+]`** before closing the terminal.

---

## Other causes (less common here)

| Cause | Notes |
|---|---|
| Cursor / VS Code serial panel | Close serial monitor extensions |
| Arduino IDE Serial Monitor | Close before Zephyr work |
| Two terminals both running monitor | Only one client at a time |
| You running monitor while agent also runs | Coordinate — kill agent leftovers first |

---

## Preventing lockups

### For you (manual work)

1. **One serial client at a time**
2. Exit monitor with **`Ctrl+]`** — do not only close the tab
3. Run `.\scripts\kill-serial-monitor.ps1` before flash if the agent was just used

### For Cursor agent / automated runs

Agent sessions that run `west espressif monitor` **must** clean up afterward:

1. Prefer a **time-limited** monitor with explicit kill, or capture output and exit
2. After any automated monitor, run **`kill-serial-monitor.ps1`** before handing control back
3. Do not leave background `python`/`idf_monitor` running after the task completes

---

## If still locked after killing python/idf_monitor

| Step | Action |
|---|---|
| Kill script again | `.\scripts\kill-serial-monitor.ps1 -Port COM16` |
| Check Resource Monitor | Search handles for `COM16` — kill whatever remains |
| Reboot PC | Clears all COM handles (last resort) |
| Wrong COM number | Device Manager — port may not still be COM16 |

---

## Find which COM port is the XIAO

**Device Manager** → **Ports (COM & LPT)** → **USB Serial Device (COMxx)**

```powershell
[System.IO.Ports.SerialPort]::getportnames()
```

Unplug the board and run again — the port that disappears is the XIAO.

---

## Related docs

- [scripts/kill-serial-monitor.ps1](../scripts/kill-serial-monitor.ps1) — kill helper
- [docs/phase0-setup.md](phase0-setup.md) — flash and monitor commands
- [LESSONS_LEARNED.md](../LESSONS_LEARNED.md) — COM port entries

---

## Log a new issue

Add a row to [LESSONS_LEARNED.md](../LESSONS_LEARNED.md) with the **process name and command line** from Resource Monitor or Task Manager.
