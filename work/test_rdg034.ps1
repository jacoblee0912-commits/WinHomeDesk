param([Parameter(Mandatory=$true)][string]$Bundle,[Parameter(Mandatory=$true)][string]$RdgFile)
$ErrorActionPreference='Stop'
$framework=Get-ChildItem 'C:/Program Files/dotnet/shared/Microsoft.WindowsDesktop.App' -Directory | Where-Object Name -Match '^10\.0\.\d+$' | Sort-Object {[version]$_.Name} -Descending | Select-Object -First 1
foreach($name in @('System.Windows.Forms.Primitives.dll','System.Windows.Forms.dll','System.Drawing.Common.dll')){[Reflection.Assembly]::LoadFrom((Join-Path $framework.FullName $name))|Out-Null}
foreach($name in @('WinRT.Runtime.dll','Microsoft.Windows.SDK.NET.dll','MSTSCLib.dll','AxMSTSCLib.dll')){[Reflection.Assembly]::LoadFrom((Join-Path $Bundle $name))|Out-Null}
$assembly=[Reflection.Assembly]::LoadFrom((Join-Path $Bundle 'RDCMan.dll'))
$type=$assembly.GetType('RDCMan.Settings.LocalResourcesSettings')
if(!$type){$type=$assembly.GetType('RdcMan.LocalResourcesSettings')}
if(!$type){throw 'Local resources type missing'}
$doc=[Xml.XmlDocument]::new();$doc.Load($RdgFile)
$errors=[Collections.Generic.List[string]]::new()
foreach($node in $doc.SelectNodes('//server/localResources')) {
 $settings=[Activator]::CreateInstance($type,$true)
 $type.GetMethod('ReadXml',[Reflection.BindingFlags]'Instance,Public,NonPublic').Invoke($settings,@($node,$null,$errors))|Out-Null
 if($errors.Count -ne 0 -or !$settings.RedirectClipboard.Value){throw "Clipboard settings rejected: $errors"}
}
"PASS RDCMan $($assembly.GetName().Version): generated RDG enables Clipboard; parser only, no application settings changed."
