# Build and test

[한국어](BUILD.md) | **English**

Run commands from the repository root on Windows x64. Python 3 and the Windows .NET Framework 4.x C# compiler are required for the GUI.

```powershell
python work/build_gui030.py
python work/test029.py
python work/test030.py
```

The result is `work/WinHomeDesk.exe`. It embeds `Translations.xml` and the included custom IME hook. It still needs the native engine directory from a release or a native build. `test029.py` creates the lifecycle test helper used by `test030.py`; run them in the order shown. Tests do not register a real startup task or start a real RDP server.

For the native engine and GUI, use `work/Build.ps1` with LLVM-MinGW UCRT x64, CMake, Ninja and the dependencies listed in `work/BUILD-INFO.txt`. See the Korean build guide for the command. Do not distribute Windows system or RDCMan DLLs.

`Translations.xml` stores exact Korean source strings and their English equivalents. User content is not passed through the translation catalog. Translation coverage, live switching, persisted preferences, unchanged credentials/protocol options and Korean/English UI snapshots are checked by `test030.py`.

Historical scripts may require old build directories. The historical 0.3.0 localization release retained the previous native engine; 0.3.3 requires a native rebuild. Task Scheduler COM validation can be skipped when the environment denies access.


## 0.3.3 native engine and regressions

Rebuild both the GUI and native engine/DLLs for 0.3.3; do not mix 0.3.0 or Mac engine binaries.

```powershell
python work/test031.py --engine <complete-engine-directory/WinHomeDesk-engine.exe> --compiler <LLVM-MinGW-clang.exe>
python work/test032.py --compiler <LLVM-MinGW-clang.exe>
python work/test033.py --compiler <LLVM-MinGW-clang.exe>
```

Test032 uses temporary loopback ports 23730–23731 to test child-process lifecycle. It does not inject user input. Test033 sends display-change messages only to its hidden test window and does not reconfigure real displays. See the [Windows guide](WINDOWS_MULTIMONITOR.en.md) for validation limits.

Package with `python work/package033.py --base <0.3.0-Windows-x64.zip> --output <empty-directory>`. Python `pefile` is required. The base ZIP supplies licenses and runtimes; current builds replace the engine EXE and FreeRDP/WinPR DLLs.


## 0.3.4 clipboard regression

Build the native engine and GUI first. The clipboard bridge uses C++17 and statically linked LLVM C++ runtime code. Keep `CHANNEL_CLIPRDR_SERVER` enabled.

```powershell
python work/test034.py --compiler C:/toolchain/bin/x86_64-w64-mingw32-clang++.exe --runtime C:/staged-package/engine
python work/test032.py --compiler C:/toolchain/bin/x86_64-w64-mingw32-clang.exe
python work/package034.py --base C:/packages/WinHomeDesk-0.3.0-Windows-x64.zip --output C:/packages/release034
```

The OS clipboard test requires an isolated window station. If creation is denied, the test reports SKIP and still tests protocol callbacks, IDataObject/IStream, file snapshots, cancellation, the inactive listener lifecycle and GUI settings. It never falls back to changing the user's clipboard. See the CLIPBOARD guide for remaining two-PC validation.


## 0.3.5 inbound regression / 수신 회귀 시험

```powershell
python work/test035.py --compiler C:/toolchain/bin/x86_64-w64-mingw32-clang++.exe --runtime C:/staged-package/engine
python work/package035.py --base C:/packages/WinHomeDesk-0.3.0-Windows-x64.zip --output C:/packages/release035
```

`test035.py` tests cached format rendering, Explorer's format-only query, timeout isolation, late-reply draining and incoming publication guards. It also calls the real cliprdr DLL through an in-memory WTS transport to verify wire headers and fragmented responses. It does not touch the user's clipboard or initiate a network connection. 실제 두 PC 전송 재검증과는 별도입니다.

File size-query regression (no OS clipboard changes):

```powershell
python work/test036.py --compiler <clang++.exe> --runtime <packaged-engine-directory>
```

Clipboard file locking and COM wait regression (no OS clipboard changes):

```powershell
python work/test037.py --compiler <clang++.exe> --runtime <packaged-engine-directory>
```
