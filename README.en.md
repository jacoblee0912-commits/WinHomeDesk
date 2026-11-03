# WinHomeDesk

Development source **0.7.0-local** adds a macOS 27 / Apple Silicon server and client, system audio and video options, Windows server multi-monitor capture, and separate monitor windows on Mac. See [build and usage](macos/README.md), [validation status](macos/VALIDATION.md), and the [Windows continuation guide](docs/WINDOWS_MULTIMONITOR_DEVELOPMENT.md). The Windows GUI remains at 0.3.0; its monitor selector and native monitor windows are follow-up work. No binary release accompanies this source snapshot.

[한국어](README.md) | **English**

WinHomeDesk is an experimental, standalone FreeRDP-based server for sharing the currently signed-in Windows desktop. It targets **Windows 11 Home x64**, including Surface Pro 8 and x64 Surface Pro 9 devices. The current release is **0.3.0**.

It does not use RDPWrap or patch Windows Remote Desktop Services. It shares an existing desktop; it does not create separate Windows user sessions. Access before sign-in, the lock screen, UAC secure desktop and sleeping PCs is not supported.

## What's new in 0.3.0

- Select **한국어 / English** from the **Language** menu at the top right of the main window.
- The selected language is saved and restored on the next launch.
- Switch menus, settings, RDG management, app messages and performance captions without restarting the server.
- User-entered passwords, addresses and device/group names are preserved. Existing log lines and native engine diagnostic identifiers are not rewritten.
- English and Korean guides and project descriptions are provided separately.

## Features

- DirectX (DXGI) capture with GDI fallback
- H.264 AVC420, AVC444, GFX Planar and compatible bitmap modes
- GPU-first AVC420 encoding with CPU fallback
- 20 / 30 / 45 FPS targets, resolution scaling and stage timing metrics
- System audio, mouse/keyboard input and optional right-Alt Korean toggle
- RDP connection files, grouped RDCMan RDG files and RDP import
- Integrated launcher for the RDCMan 3.22 Korean input fix
- System tray operation and optional elevated startup at Windows sign-in

## Quick start

Download the Windows x64 ZIP from [Releases](https://github.com/jacoblee0912-commits/WinHomeDesk/releases) and extract the entire archive into a fixed folder. Keep the `engine` directory beside `WinHomeDesk.exe`.

On the **PC being shared**, choose its reachable IPv4 address and check the app password. Select the screen-sharing consent checkbox, then **Start server**. The username is `surface`, the default TCP port is `3390`, and Domain is blank. Use the app password, not your Windows PIN. `127.0.0.1` is for same-PC testing only.

On the **connecting PC**, open a saved RDP file or load an RDG in RDCMan. For the RDCMan 3.22 Korean input fix, run WinHomeDesk on the connecting PC and use **RDCMan (Korean input fix)**. You do not need to start a server on that PC. RDCMan must be installed separately; its binaries are not included.

## Elevated sign-in startup

In **Settings**, select **Start in the tray when I sign in to Windows** and **Run as administrator at sign-in**. Save and approve UAC using the currently signed-in administrator account. This registers an interactive Task Scheduler logon task. It takes effect on the next sign-in, not in the already-running app. Starting the remote server automatically is a separate option.

**Validation limit:** startup migration, rollback and UI tests passed, but actual elevated task registration, UAC approval and execution after sign-in could not be tested in the restricted build environment. This limitation remains in 0.3.0; the native RDP engine is unchanged from 0.2.9.

## Documentation and source

- [English installation and usage guide](docs/GUIDE.en.md)
- [한국어 설치·사용 안내](docs/GUIDE.ko.md)
- [Build and test instructions](docs/BUILD.en.md)
- [Development history](CHANGELOG.md)
- [Korean PDF manual for 0.2.8](docs/WinHomeDesk-0.2.8-설치-사용설명서.pdf): use the current guides for language selection and elevated startup.

`work/*.cs` contains the Windows GUI. `work/Translations.xml` contains the Korean/English catalog. `work/FreeRDP` contains the modified native server source. `work/ime-fix` contains our own Korean input module and its source. Historical scripts remain as development records and may require their original toolchain/output directories.

## License

Custom WinHomeDesk code and the original icon are Apache-2.0 licensed. FreeRDP and bundled dependencies retain their own copyright and license terms. See [LICENSE](LICENSE) and [NOTICE](work/licenses/NOTICE.txt). No Microsoft RDCMan or Windows system DLLs are redistributed.
