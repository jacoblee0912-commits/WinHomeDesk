# Run on the Windows client. Prompts record human observations, not automated passes.
[CmdletBinding()]
param(
  [Parameter(Mandatory=$true)][string]$MacAddress,
  [ValidateRange(1,65535)][int]$Port=3390,
  [Parameter(Mandatory=$true)][string]$RdcMan293,
  [Parameter(Mandatory=$true)][string]$RdcMan322,
  [string]$OutputDirectory=(Join-Path $PWD 'WinHomeDesk-Mac-validation')
)
$ErrorActionPreference='Stop'
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$results=@()
foreach($case in @(@{Version='2.93';Path=$RdcMan293},@{Version='3.22';Path=$RdcMan322})) {
  $exe=(Resolve-Path -LiteralPath $case.Path).Path
  $info=[Diagnostics.FileVersionInfo]::GetVersionInfo($exe)
  $actual='{0}.{1}' -f $info.FileMajorPart,$info.FileMinorPart
  if($actual -ne $case.Version) { throw "Expected $($case.Version), got $actual at $exe" }
  $escaped=[Security.SecurityElement]::Escape($MacAddress)
  $rdg=Join-Path $OutputDirectory "Mac-$actual.rdg"
  @"
<?xml version="1.0" encoding="utf-8"?>
<RDCMan programVersion="$actual" schemaVersion="3">
 <file><credentialsProfiles/><properties><expanded>True</expanded><name>WinHomeDesk Mac validation</name></properties>
 <server><properties><name>$escaped</name><displayName>Mac $actual</displayName></properties>
 <logonCredentials inherit="None"><profileName scope="Local">Custom</profileName><userName>surface</userName><domain/></logonCredentials>
 <connectionSettings inherit="None"><connectToConsole>False</connectToConsole><port>$Port</port></connectionSettings>
 <securitySettings inherit="None"><authentication>Warn</authentication></securitySettings>
 <remoteDesktop inherit="None"><colorDepth>32</colorDepth></remoteDesktop>
 </server></file><connected/><favorites/><recentlyUsed/>
</RDCMan>
"@ | Set-Content -LiteralPath $rdg -Encoding UTF8
  $network=Test-NetConnection -ComputerName $MacAddress -Port $Port -WarningAction SilentlyContinue
  $process=Start-Process -FilePath $exe -ArgumentList @('/noopen',('"'+$rdg+'"')) -PassThru
  Write-Host "RDCMan $actual. Connect to Mac, enter the application password in RDCMan (not here)."
  Write-Host 'Use a disposable TextEdit document on the Mac. Close this RDCMan session before the next version.'
  $checks=[ordered]@{}
  foreach($item in @('screen_updates','ascii_typing','korean_typing','left_and_right_modifiers',
      'click_double_click_drag','vertical_horizontal_wheel','wrong_password_rejected',
      'disconnect_releases_keys','reconnect','lock_stops_sharing')) {
    $answer=Read-Host "$item : pass / fail / skip"
    if($answer -notin @('pass','fail','skip')) { $answer='skip' }
    $checks[$item]=$answer
  }
  $results += [ordered]@{version=$actual;fileVersion=$info.FileVersion;sha256=(Get-FileHash $exe -Algorithm SHA256).Hash;
    mac=$MacAddress;port=$Port;tcpReachable=$network.TcpTestSucceeded;observation='manual';checks=$checks;time=(Get-Date).ToString('o')}
  $results | ConvertTo-Json -Depth 8 | Set-Content (Join-Path $OutputDirectory 'results.json') -Encoding UTF8
  Read-Host 'Close the tested RDCMan connection, then press Enter to continue' | Out-Null
}
Write-Host "Saved manual observations to $OutputDirectory/results.json. No credentials were collected."
