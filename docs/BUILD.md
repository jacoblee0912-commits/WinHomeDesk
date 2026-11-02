# 빌드와 테스트

저장소 루트에서 명령을 실행합니다. Windows x64가 대상입니다.

## GUI만 빌드

Windows의 .NET Framework 4.x C# 컴파일러와 Python 3가 필요합니다. 자체 제작 IME 모듈은 이미 포함되어 있습니다.

```powershell
python work/build_gui029.py
python work/test029.py
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
