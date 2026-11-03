param([string]$InstallDirectory)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Windows.Forms
try {
    if (-not $InstallDirectory) {
        $picker = New-Object System.Windows.Forms.OpenFileDialog
        $picker.Title = 'Select your existing WinHomeDesk.exe'
        $picker.Filter = 'WinHomeDesk|WinHomeDesk.exe'
        if ($picker.ShowDialog() -ne 'OK') { exit 0 }
        $InstallDirectory = Split-Path -Parent $picker.FileName
        $picker.Dispose()
    }
    $InstallDirectory = (Resolve-Path -LiteralPath $InstallDirectory).Path
    $engine = Join-Path $InstallDirectory 'engine'
    if (-not (Test-Path -LiteralPath (Join-Path $InstallDirectory 'WinHomeDesk.exe')) -or
        -not (Test-Path -LiteralPath (Join-Path $engine 'WinHomeDesk-engine.exe'))) {
        throw 'Select the folder containing the existing WinHomeDesk.exe and engine folder.'
    }
    if (Get-Process -Name WinHomeDesk-engine -ErrorAction SilentlyContinue) {
        throw 'Click Stop Server in WinHomeDesk first, then run this updater again. Keep the main WinHomeDesk app open.'
    }
    $payload = Join-Path $PSScriptRoot 'engine'
    $manifest = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'engine-sha256.json') -Raw | ConvertFrom-Json
    foreach ($file in $manifest) {
        $source = [IO.Path]::GetFullPath((Join-Path $payload $file.path))
        if (-not $source.StartsWith($payload + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Invalid package path.' }
        if ((Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash -ne $file.sha256) { throw ('Package hash mismatch: ' + $file.path) }
    }
    # This enumerates display geometry only; it never starts a listener or captures pixels.
    $start = New-Object Diagnostics.ProcessStartInfo
    $start.FileName = Join-Path $payload 'WinHomeDesk-engine.exe'
    $start.Arguments = '/monitors'
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    $probe = [Diagnostics.Process]::Start($start)
    $stdout = $probe.StandardOutput.ReadToEndAsync()
    $stderr = $probe.StandardError.ReadToEndAsync()
    if (-not $probe.WaitForExit(15000)) { $probe.Kill(); throw 'Monitor preflight timed out.' }
    $report = $stdout.Result + $stderr.Result
    $report | Set-Content -LiteralPath (Join-Path $PSScriptRoot 'monitor-preflight.txt') -Encoding UTF8
    if ($report -notmatch 'MONITOR_LIST count=(\d+)') { throw 'Engine preflight failed. See monitor-preflight.txt; existing files were not changed.' }
    $count = [int]$Matches[1]
    if ($count -lt 2) { throw 'Only one extended display was detected. Set Windows displays to Extend, then retry. Existing files were not changed.' }
    $backup = Join-Path $InstallDirectory ('engine.pre-multimonitor-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
    Copy-Item -LiteralPath $engine -Destination $backup -Recurse
    $changed = New-Object 'System.Collections.Generic.List[string]'
    try {
        foreach ($file in $manifest) {
            $target = Join-Path $engine $file.path
            [IO.Directory]::CreateDirectory((Split-Path -Parent $target)) | Out-Null
            $changed.Add($file.path)
            Copy-Item -LiteralPath (Join-Path $payload $file.path) -Destination $target -Force
            if ((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ne $file.sha256) { throw ('Copy verification failed: ' + $file.path) }
        }
    } catch {
        foreach ($relative in $changed) {
            $old = Join-Path $backup $relative
            $target = Join-Path $engine $relative
            if (Test-Path -LiteralPath $old) { Copy-Item -LiteralPath $old -Destination $target -Force }
            elseif (Test-Path -LiteralPath $target) { Remove-Item -LiteralPath $target -Force }
        }
        throw
    }
    [Windows.Forms.MessageBox]::Show("Update complete: $count monitors detected.`nBackup: $backup`n`nClick Start Server in the existing WinHomeDesk window. Then connect from Mac and use New Monitor Window.",'WinHomeDesk multi-monitor update') | Out-Null
} catch {
    [Windows.Forms.MessageBox]::Show($_.Exception.Message,'WinHomeDesk update failed') | Out-Null
    exit 1
}
