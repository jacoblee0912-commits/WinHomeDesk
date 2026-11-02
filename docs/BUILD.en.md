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

Historical scripts may require old build directories. Native code was not rebuilt for this localization-only release; packaged native files are verified against the prior release. Task Scheduler COM validation can be skipped when the environment denies access.
