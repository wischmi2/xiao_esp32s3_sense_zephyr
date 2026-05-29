# Serial capture logs

Automated `west espressif monitor` output and agent capture files go here (not repo root).

Use:

```powershell
.\scripts\capture-serial.ps1 -Port COM16 -OutFile cam_jpeg.txt -Seconds 45
```

Or pass `-Monitor` to build scripts that support it.
