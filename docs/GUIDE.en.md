# WinHomeDesk 0.3.0: Installation and usage

For 0.3.3 multi-monitor connections and fixes, see the [current Windows guide](WINDOWS_MULTIMONITOR.en.md).

[한국어](GUIDE.ko.md) | **English**

## 1. Install or update

1. Exit the old version using **Exit (also stops server)** in its tray menu. The window's X button only hides it.
2. Extract the entire Windows x64 ZIP into a new, fixed folder. Do not run inside the ZIP or copy only the EXE.
3. Run `WinHomeDesk.exe` and check the version in the title bar.
4. If sign-in startup is enabled, save Settings from the new folder to update its executable path.

This build targets Windows 11 x64; it is not a native ARM64 distribution. The PC being shared must be signed in, awake and unlocked.

## 2. Choose the language

Use **Language → English** or **한국어** at the top right of the main window. The selection is saved immediately and restored on the next launch. No server restart is required. Settings, buttons, group management and new app messages use the selected language.

Changing the UI language does not change the Windows keyboard layout, Korean IME, right-Alt mapping, passwords, addresses or user-entered device names. Previously recorded logs are kept as recorded. Native engine identifiers and Windows-provided error messages may remain in their original language.

## 3. Start the server on the PC being shared

1. Choose a reachable IPv4 address assigned to this PC. Do not select `127.0.0.1` for connections from another PC.
2. Use TCP port `3390` unless you have deliberately changed it.
3. Check or set the app-specific password, at least 12 characters long. Username: `surface`; Domain: blank.
4. Select the screen-sharing consent checkbox and click **Start server**.
5. Confirm **Listening** and a `LISTENING` log entry, then use **Save RDP file** or **RDCMan groups**.

Use the password shown in the app, not a Windows PIN. If automatic server start is off, a fresh random password is generated after completely exiting and reopening the app. Automatic server start stores the password encrypted for the current Windows account.

The real local desktop is shared, so people at the PC see the same screen. Locking, entering the UAC secure desktop or changing resolution can stop the server. Access before sign-in and separate Windows sessions are not supported.

## 4. Connect from another PC

For one PC, transfer the saved `.rdp` file and open it in Windows Remote Desktop Connection. Enter `surface` and the current app password. Verify the first-connection certificate against **Certificate fingerprint** on the server.

For RDCMan, open an `.rdg` file using **File → Open**. If entering details manually, put only the IP/hostname in Server name and set the port separately in Connection Settings. Use 32-bit color. Set Local Resources to play remote audio on the connecting computer.

Select a server and use **Shift+Enter (Connect As)** to enter credentials, or set its Logon Credentials without inheriting unrelated parent credentials. An RDG file does not guarantee an automatic password prompt on every connection.

## 5. Manage groups and import RDP files

Open **RDCMan groups**. Set the list name and fill in Group, Device name, IP/hostname, Port, Username and Domain. The initial row represents the current PC; delete it if it is only your management PC.

To import, choose an **Import group**, click **Add RDP files**, and select one or more files. Address/port duplicates are skipped. Addresses, ports and accounts are imported; passwords, gateways and automatic password-prompt options are not. Files without a port default to standard RDP port 3389, so check WinHomeDesk targets for port 3390.

**Load RDG** replaces the table with a simplified device list; it is not a lossless editor for all RDCMan settings. Save under a different name to preserve the original. Keep separate copies for RDCMan 2.93 and 3.22 because saving in a newer RDCMan can affect older-version compatibility.

## 6. Korean input in RDCMan 3.22

On the connecting PC, close any running RDCMan instance. Open WinHomeDesk and click **RDCMan (Korean input fix)**. Select the installed RDCMan 3.22 executable once, then open your RDG. Use this launcher or its tray menu every time you want the fix applied.

The launcher retains RDCMan 3.22's new engine and does not modify the original executable. It does not affect RDCMan 2.93 or double-clicked RDP files. The connecting PC does not need to run a server. Changing WinHomeDesk to English does not disable this feature.

For right-Alt Korean/English switching, enable **Use right Alt to toggle Korean/English input** on the server PC and restart the server. Disable it when you need regular AltGr. The shared Windows desktop needs a Korean input method configured.

## 7. Tray and automatic startup

X hides the app. Double-click its tray icon to restore it. **Exit (also stops server)** closes the app and its server. Closing WinHomeDesk does not forcibly close RDCMan it launched.

In Settings, **Start in the tray when I sign in to Windows** starts the app after sign-in. **Also start the server** separately stores the current server settings and password. If the saved IP does not become available within about 60 seconds, automatic server start is deferred; open the app, check the address and start manually.

For elevated startup, also select **Run as administrator at sign-in**, save, and approve UAC with the same currently signed-in administrator account. A different administrator account is not supported. UAC secure desktop activation can interrupt an active remote session, so stop the server before configuring this. The next sign-in runs the task at highest available privileges; the title bar displays **Administrator**. The current process is not elevated by saving settings.

Turning off elevated startup switches back to ordinary startup; turning off sign-in startup removes the app's task and Run entries. Changing or removing an elevated task can require UAC again. This is an interactive logon task, not a SYSTEM service, and it does not store your Windows account password.

## 8. Performance and audio

Start comparisons with **Balanced 1920px / 30 FPS / Auto DirectX / Auto AVC420 / Prefer GPU**. Stop the server before changing these settings, then restart and reconnect. 20/30/45 FPS values are targets, not guarantees. Check the active graphics and encoder captions; GPU-first can fall back to CPU, and AVC444 uses the CPU in this implementation.

Enable **Stream system audio** and client-side audio playback. Check both PCs' output devices, mute states and volumes. If audio lags or stretches, compare audio-only playback with video/scrolling, try 1440px and 30 FPS, and record whether the delay is constant or accumulates.

Metrics show capture, encode/prepare and send-call costs. They do not directly measure end-to-end display or speaker latency. Low FPS on a static screen can be normal. Use **Save log** immediately after reproducing a problem and record the client version and settings.

## 9. Connection, VPN and input troubleshooting

- **Cannot connect:** check LISTENING, address/port, sleep/lock state, firewall and network routing. The listener uses TCP. Allow only the required trusted LAN/VPN peers rather than disabling the firewall.
- **Password rejected:** use `surface`, blank Domain and the current app password. Remove stale saved credentials where necessary.
- **VPN:** a fixed outbound public IP is not necessarily a reachable inbound server address. Verify peer routing/inbound support. Select an address actually assigned to the shared PC and update RDP/RDG files after address changes.
- **Only some apps accept clicks:** compare with a normal Notepad window. Windows may block injected input into higher-privilege applications. Running the server as administrator can help with that case but does not enable UAC secure-desktop control or guarantee every app can be controlled.

## 10. Data and validation limits

Server settings: `%LOCALAPPDATA%\SurfaceDesk\preferences.xml`; certificates: `%LOCALAPPDATA%\SurfaceDesk\shadow`; client input-fix cache/logs: `%LOCALAPPDATA%\WinHomeDesk\Client`.

Disable sign-in startup and save before deleting the app folder. User data remains separate. Encrypted passwords are tied to the Windows user and may not transfer to another PC/account.

0.3.0 adds localization; the native RDP engine and Korean input-fix module are unchanged. UI/resource, preference preservation and startup regression tests were run. Actual elevated task registration/UAC/re-login and a new Surface-to-Surface network test were not performed in the restricted build environment.
