# WinHomeDesk 0.3.3-local — separate monitor connections / H.264

This local Windows build adds separate monitor connections to the 0.3.1 combined-desktop mode. It does not depend on the Mac engine. This 0.3.3 source snapshot is published on GitHub; binary release publication is separate.


## 0.3.3 fixes

- Settings: handle HRESULT `0x80070002` when COM interop maps an absent task to `FileNotFoundException`, not just `COMException`. Only task lookup treats this as absent; access-denied and COM initialization/dependency errors remain visible.
- Server cursor flicker mitigation: move `EnumDisplaySettingsEx` out of the per-frame and input paths. Runtime checks use USER32 geometry and an invisible, non-activating window receiving `WM_DISPLAYCHANGE`. Layout changes still stop the server.
- GDI uses `SRCCOPY` without `CAPTUREBLT`. Some layered/transparent windows may render differently with GDI; use automatic DirectX capture if such content is omitted. H.264 remains enabled.
- Regression tests inject the exact settings error and open the dialog, cover startup rollback, perform 200 topology checks without mode queries, and stop on a display-change message. No real display reconfiguration or mouse movement is performed by these tests.
- Whether cursor flicker is fully resolved on the user's hardware still requires confirmation after installing this build; display-driver variations have not all been tested.


## Install and open separate windows

1. Exit the old app from its tray menu. Closing its window only hides it.
2. Extract the entire ZIP into a **new directory**, then run `WinHomeDesk.exe`. Do not mix DLLs or executables from different versions.
3. Set Windows displays to **Extend**.
4. Choose **Share displays → Separate monitor connections (RDCMan)**. Select the server address and starting port.
5. Select Auto/prefer H.264 AVC420 or H.264 AVC420, check sharing consent, and start the server.
6. Use **Save monitor RDG**, or **RDCMan groups** to save the automatically populated monitor entries.
7. Open the RDG in RDCMan **2.93 or 3.22**. Entries are named `PC — DISPLAY1`, `PC — DISPLAY2`, etc. Use username `surface`, an empty Domain, and the WinHomeDesk app password. Passwords are not included in the RDG. Shift+Enter (Connect As) can be used to enter credentials.
8. Connect each entry, then choose **Undock** from its context menu. Arrange the separate windows as needed. The exported RDG scales each remote screen to its window.

For three displays starting at port **3390**, the endpoints use **3390, 3391, 3392**. Allow the assigned TCP ports in the server firewall and any VPN/forwarding configuration. The app does not automatically modify firewall rules.

These windows share **one logged-in Windows desktop**, including one pointer and keyboard focus; they are not independent user sessions. Duplicate display mode may show identical content. DISPLAY device names are engine identifiers and need not match the numbers in Windows Settings. Check the actual screen and logged device coordinates.

## Behavior

- Primary-only, combined-all, and separate monitor modes remain available. Scope cannot change while running.
- Separate mode launches one engine per display, using consecutive ports and one authenticated connection per port. A range exceeding 65535 is rejected before launch.
- Capture is pinned to a device name. A missing target fails rather than exposing the entire desktop. Portrait and negative-origin screens have their own coordinate mapping.
- Only the **first connection** sends audio. Connect that entry to hear sound. Performance/codec summaries also describe the first connection; other engines have device/port prefixes in their detailed logs.
- Certificate fingerprint shows each active endpoint's certificate. Per-port state directories prevent certificate generation races.
- Closing the window keeps all engines running in the tray. Stop, explicit exit, or an engine failure stops all sibling engines. If not all ports report ready within 30 seconds, the app stops them.
- Restart after display attachment/removal, layout/rotation/resolution changes, lock, or secure-desktop transitions. Re-export the RDG if device or port assignments change.
- Scope persists and applies to automatic server startup. Save startup options from the new executable to update its registered path. Existing settings, DPAPI password protection, and elevated startup behavior are retained.
- Save RDP becomes **Save monitor RDG** in separate mode; primary/combined modes retain ordinary RDP export.

## Performance

Each monitor has its own H.264 encoder. DXGI can fall back to GDI, and GPU encoding can fall back to CPU H.264. Concurrent connections increase encoding and network load. Reduce quality to 1440 or lower FPS if necessary.

Separate mode applies quality limits per monitor. Combined mode scales the shared layout uniformly. Each encoded canvas is capped at 8192 per dimension and 9,437,184 pixels after 16-pixel padding, including native-preferred quality.

## Validation

The Windows engine, DLLs, and GUI build successfully. Existing primary/combined-mode regressions passed. Three physical displays (two landscape, one portrait with negative coordinates) received **three simultaneous connections** with both the legacy Microsoft ActiveX engine and RDCMan 3.22 RDCore. NLA authentication and AVC420 frames passed for all six connections. At 1440 quality the separate desktop sizes were 1440×808, 1440×808, and 808×1440. RDCore screenshots showed distinct screens and the portrait orientation.

Tests cover device selection, missing-device rejection, coordinate boundaries, port overflow, RDG roundtrip, multiple ports for the same PC, saved scope, bilingual UI, and audio-only-on-first configuration. Fake local listeners verify concurrent launch, close-to-tray, stopping all children, bind failure, unexpected exit, and explicit application shutdown. Existing settings/encryption/startup mock tests passed; actual Task Scheduler COM access was denied.

The live video tests used GDI + CPU H.264. GPU performance, long-running remote sessions, audio synchronization, physical display removal, and actual pointer positioning still need hardware validation. No input was injected during this task because the user was using the mouse. Connection tests used the two Microsoft engines rather than long-running sessions in each complete RDCMan UI.

Build with `work/Build.ps1`. Tests: `test029.py`, `test030.py`, `test031.py`, `test032.py`, `test_rdg031.ps1`. Supply `--engine` and `--compiler` to test031, and `--compiler` to test032. Package with `work/package032.py --base <0.3.0 runtime ZIP> --output <empty output directory>`.

To roll back, exit the new app from the tray and run the previous installation. This task did not replace that installation or change real startup registration.
