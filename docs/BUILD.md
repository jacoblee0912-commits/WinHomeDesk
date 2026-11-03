# 빌드와 테스트

**한국어** | [English](BUILD.en.md)

저장소 루트에서 명령을 실행합니다. Windows x64가 대상입니다.

## GUI만 빌드

Windows의 .NET Framework 4.x C# 컴파일러와 Python 3가 필요합니다. 자체 제작 IME 모듈은 이미 포함되어 있습니다.

```powershell
python work/build_gui030.py
python work/test029.py
python work/test030.py
```

결과는 `work/WinHomeDesk.exe`입니다. 이 파일만으로 서버가 동작하지는 않으며 배포본의 engine 폴더 또는 직접 빌드한 엔진이 필요합니다. `test029.py`는 설정과 트레이·자동 시작 전환을 시험하며 실제 시작 등록을 바꾸지 않습니다. Task Scheduler COM을 사용할 수 없는 환경에서는 해당 검증이 SKIP됩니다.

## 네이티브 엔진과 GUI

제작 시 사용한 도구와 의존성 버전은 `work/BUILD-INFO.txt` 및 `work/licenses/dependency-manifest.json`에 기록되어 있습니다. 도구 및 사전 빌드 의존성은 저장소에 포함하지 않습니다.

```powershell
& ./work/Build.ps1 -CompilerRoot 'C:/Toolchains/llvm-mingw' -DependencyRoot 'C:/Dependencies/ucrt64' -CMake 'C:/Tools/cmake/bin/cmake.exe' -Ninja 'C:/Tools/ninja.exe'
```

컴파일러는 LLVM-MinGW UCRT x64, 의존성은 OpenSSL·OpenH264·zlib-ng-compat를 사용합니다. `SURFACE_AUDIO_TEST_SOURCE=OFF`로 제작해야 합니다. `work/build/server/shadow/cli/freerdp-shadow-cli.exe`를 배포할 때는 `engine/WinHomeDesk-engine.exe`로 이름을 바꾸고 필요한 런타임 DLL과 OpenSSL legacy provider, 라이선스를 함께 둡니다. Windows 시스템 DLL이나 RDCMan DLL은 복사하지 않습니다.

## RDCMan 관련 테스트

별도 설치한 RDCMan 실행 파일을 환경 변수로 지정합니다. 개인 PC 경로를 소스에 기록하지 않습니다.

```powershell
$env:RDCMAN_EXE = 'C:/Tools/RDCMan/RDCMan.exe'
python work/test_client028.py
```

실제 RDCMan 3.22를 시험 프로세스로 실행하는 테스트입니다. RDCore 프로토콜 테스트는 별도의 SDK 런타임 준비가 필요하므로 `work/BUILD-INFO.txt`를 먼저 확인하세요. 루프백 테스트 인증서 허용 코드를 원격 서버에 사용하지 마세요.

## IME 모듈 재빌드

`work/ime-fix/StartupHook.cs`가 내장 모듈 소스입니다. PowerShell 7/.NET 10, Roslyn, WindowsDesktop 참조가 필요합니다. `work/ime-fix/build.ps1`의 설치 경로를 환경에 맞게 수정한 후 실행합니다. 메인 GUI는 계속 .NET Framework 4.x를 사용합니다.

## 이 저장소를 준비하며 확인한 범위

최신 GUI 빌드와 test029를 저장소 사본에서 확인합니다. 새 환경에서 네이티브 전체를 다시 빌드했거나 실제 서피스 간 접속을 다시 시험한 것은 아닙니다. 네이티브 원본은 0.2.9 소스 배포본에서 가져왔습니다.


## 0.3.3 네이티브 엔진과 회귀 시험

0.3.3은 GUI와 네이티브 엔진/DLL을 함께 빌드해야 합니다. 0.3.0 또는 Mac 엔진 DLL과 섞지 마세요.

```powershell
python work/test031.py --engine <완전한-엔진-폴더/WinHomeDesk-engine.exe> --compiler <LLVM-MinGW-clang.exe>
python work/test032.py --compiler <LLVM-MinGW-clang.exe>
python work/test033.py --compiler <LLVM-MinGW-clang.exe>
```

`test032.py`는 임시 루프백 TCP 포트 23730~23731을 사용해 다중 자식 프로세스의 생명주기를 시험합니다. 실제 입력을 주입하지 않습니다. `test033.py`는 실제 화면을 변경하지 않고 숨겨진 테스트 창에 변경 메시지를 보내 중지 처리를 검사합니다. 최신 검증 범위와 남은 장치 시험은 [Windows 안내](WINDOWS_MULTIMONITOR.ko.md)를 참고하세요.

패키징은 `python work/package033.py --base <0.3.0-Windows-x64.zip> --output <빈-폴더>`를 사용합니다. Python `pefile`이 필요하며, 라이선스와 런타임은 기존 ZIP에서 가져오고 FreeRDP/WinPR DLL과 EXE는 현재 빌드 결과로 교체합니다.


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
