param([string]$Report)
$ErrorActionPreference='Stop'
$desktop='C:/Program Files/dotnet/shared/Microsoft.WindowsDesktop.App/10.0.11'
foreach($name in @('System.Windows.Forms.Primitives.dll','System.Windows.Forms.dll','System.Drawing.Common.dll')){[Reflection.Assembly]::LoadFrom((Join-Path $desktop $name)) | Out-Null}
foreach($name in @('WinRT.Runtime.dll','Microsoft.Windows.SDK.NET.dll','MSTSCLib.dll','AxMSTSCLib.dll','RDCMan.dll')){[Reflection.Assembly]::LoadFrom((Join-Path "$PSScriptRoot/rdc322-bundle" $name)) | Out-Null}
$refs=@([AppContext]::GetData('TRUSTED_PLATFORM_ASSEMBLIES').Split([IO.Path]::PathSeparator))
$refs+=@('System.Windows.Forms.Primitives.dll','System.Windows.Forms.dll','System.Drawing.Common.dll') | ForEach-Object {Join-Path $desktop $_}
$refs+=@('WinRT.Runtime.dll','Microsoft.Windows.SDK.NET.dll','RDCMan.dll') | ForEach-Object {Join-Path "$PSScriptRoot/rdc322-bundle" $_}
Add-Type -CompilerOptions /nowarn:1701 -Path "$PSScriptRoot/rdcoreprobe027.cs" -ReferencedAssemblies ($refs | Select-Object -Unique)
[RDCoreProbe027]::Run("$PSScriptRoot/rdc322-bundle",$env:WINHOME_TEST_PASSWORD,$Report)

