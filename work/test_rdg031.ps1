param([Parameter(Mandatory=$true)][string]$RdcManAssembly,[string]$Bundle,[string]$RdgFile)
$ErrorActionPreference='Stop'
if($Bundle) {
    $framework=Get-ChildItem 'C:/Program Files/dotnet/shared/Microsoft.WindowsDesktop.App' -Directory | Where-Object Name -Match '^10\.0\.\d+$' | Sort-Object {[version]$_.Name} -Descending | Select-Object -First 1
    foreach($name in @('System.Windows.Forms.Primitives.dll','System.Windows.Forms.dll','System.Drawing.Common.dll')){[Reflection.Assembly]::LoadFrom((Join-Path $framework.FullName $name))|Out-Null}
    foreach($name in @('WinRT.Runtime.dll','Microsoft.Windows.SDK.NET.dll','MSTSCLib.dll','AxMSTSCLib.dll')){[Reflection.Assembly]::LoadFrom((Join-Path $Bundle $name))|Out-Null}
}
$assembly=[Reflection.Assembly]::LoadFrom($RdcManAssembly)
$type=$assembly.GetType('RdcMan.ServerDisplaySettings')
if(!$type){$type=$assembly.GetType('RDCMan.Settings.ServerDisplaySettings')}
$doc=[Xml.XmlDocument]::new()
if(!$RdgFile){$RdgFile=Join-Path $PSScriptRoot 'RDG-compatibility-test.rdg'}
$doc.Load($RdgFile)
$errors=[Collections.Generic.List[string]]::new()
foreach($node in $doc.SelectNodes('//server/displaySettings')) {
$settings=[Activator]::CreateInstance($type,$true)
$type.GetMethod('ReadXml',[Reflection.BindingFlags]'Instance,Public,NonPublic').Invoke($settings,@($node,$null,$errors))
if($errors.Count -ne 0 -or $settings.SmartSizeDockedWindow.Value.ToString() -ne 'Scale' -or $settings.SmartSizeUndockedWindow.Value.ToString() -ne 'Scale'){throw "RDG scale settings rejected: $errors"}
}
"PASS $($assembly.GetName().Version): generated RDG scales docked and undocked windows; no application settings changed."
