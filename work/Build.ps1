param(
    [Parameter(Mandatory=$true)][string]$CompilerRoot,
    [Parameter(Mandatory=$true)][string]$DependencyRoot,
    [Parameter(Mandatory=$true)][string]$CMake,
    [Parameter(Mandatory=$true)][string]$Ninja
)
$ErrorActionPreference='Stop'
$base=$PSScriptRoot
$env:PATH=(Join-Path $CompilerRoot 'bin')+';'+$env:PATH
$compiler=(Join-Path $CompilerRoot 'bin/x86_64-w64-mingw32-clang.exe').Replace('\','/')
$compilerCxx=(Join-Path $CompilerRoot 'bin/x86_64-w64-mingw32-clang++.exe').Replace('\','/')
$resource=(Join-Path $CompilerRoot 'bin/llvm-windres.exe').Replace('\','/')
$dep=(Resolve-Path -LiteralPath $DependencyRoot).Path.Replace('\','/')
$build=Join-Path $base 'build'
$argsList=@('-S',(Join-Path $base 'FreeRDP'),'-B',$build,'-G','Ninja',"-DCMAKE_MAKE_PROGRAM=$($Ninja.Replace('\','/'))","-DCMAKE_C_COMPILER=$compiler","-DCMAKE_CXX_COMPILER=$compilerCxx","-DCMAKE_RC_COMPILER=$resource","-DCMAKE_PREFIX_PATH=$dep","-DOPENSSL_ROOT_DIR=$dep","-DOPENH264_LIBRARY=$dep/lib/libopenh264.dll.a",'-DCMAKE_BUILD_TYPE=Release','-DUSE_UNWIND=OFF','-DCMAKE_EXPORT_NO_PACKAGE_REGISTRY=ON')
$enabled=@('WITH_SERVER','WITH_SHADOW','WITH_WINPR_TOOLS','BUILD_SHARED_LIBS','WITH_OPENH264','WITH_CLIENT_COMMON','WITH_CLIENT_CHANNELS','CHANNEL_RDPSND_CLIENT')
$disabled=@('SURFACE_AUDIO_TEST_SOURCE','WITH_CLIENT','CHANNEL_URBDRC','CHANNEL_AUDIN_CLIENT','WITH_PLATFORM_SERVER','WITH_PROXY','WITH_SAMPLE','WITH_FFMPEG','WITH_MEDIA_FOUNDATION','WITH_SWSCALE','WITH_JPEG','WITH_CUPS','WITH_PULSE','WITH_ALSA','WITH_PCSC','WITH_SMARTCARD_EMULATE','WITH_MANPAGES','WITH_RDTK','WITH_WEBVIEW','WITH_KRB5','WITH_FUSE','WITH_VERBOSE_WINPR_ASSERT','WITH_PKCS11','WITH_SSE2','WITH_AVX2','WITH_WINPR_DEPRECATED','WITH_FREERDP_DEPRECATED')
foreach($item in $enabled){$argsList+="-D$item=ON"}
foreach($item in $disabled){$argsList+="-D$item=OFF"}
& $CMake @argsList
if($LASTEXITCODE -ne 0){throw 'Configure failed'}
& $CMake --build $build --target freerdp-shadow-cli surface-probe --parallel 6
if($LASTEXITCODE -ne 0){throw 'Engine build failed'}
$csc=Join-Path $env:WINDIR 'Microsoft.NET/Framework64/v4.0.30319/csc.exe'
& $csc /nologo /target:winexe /platform:x64 "/win32icon:$(Join-Path $base 'SurfaceDesk.ico')" /reference:System.Security.dll /reference:Microsoft.CSharp.dll "/out:$(Join-Path $base 'WinHomeDesk.exe')" /reference:System.Windows.Forms.dll /reference:System.Drawing.dll /reference:System.Xml.dll (Join-Path $base 'SurfaceDesk.cs') (Join-Path $base 'RdgManager.cs') (Join-Path $base 'PerformanceMetrics.cs') (Join-Path $base 'DesktopOptions.cs') (Join-Path $base 'StartupRegistration.cs') (Join-Path $base 'Localization.cs') "/resource:$(Join-Path $base 'Translations.xml'),WinHomeDesk.Translations.xml" (Join-Path $base 'SurfaceDesk.Lifecycle.cs') (Join-Path $base 'GraphicsMode.cs') (Join-Path $base 'AudioMode.cs') (Join-Path $base 'EncoderMode.cs') (Join-Path $base 'AppCredentials.cs') (Join-Path $base 'RdcManClient.cs') "/resource:$(Join-Path $base 'ime-fix/WinHomeDesk.ImeHook.dll'),WinHomeDesk.ImeHook.dll"
if($LASTEXITCODE -ne 0){throw 'GUI build failed'}
Write-Output 'Build complete. Engine: build/server/shadow/cli/freerdp-shadow-cli.exe. Copy runtime DLLs and OpenSSL legacy provider when packaging.'
