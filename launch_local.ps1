param([switch]$Mute, [switch]$TouchControls, [int]$Frames = 0)
$ErrorActionPreference = 'Stop'
$gameRoot = $PSScriptRoot
$saveRoot = Join-Path $gameRoot 'SaveData'
$logRoot = Join-Path $gameRoot 'Logs'
New-Item -ItemType Directory -Force $saveRoot, $logRoot | Out-Null
$runLock = $null
$outputStream = $null
$errorStream = $null
$process = $null
try {
    try { $runLock = [IO.File]::Open((Join-Path $saveRoot 'running.lock'), 'OpenOrCreate', 'ReadWrite', 'None') }
    catch { throw 'BSM2 Mobile is already running with this save folder.' }
    $stamp = Get-Date -Format 'yyyyMMdd_HHmmss_fff'
    if (Test-Path -LiteralPath (Join-Path $saveRoot 'Profile.save')) {
        $backupRoot = Join-Path $saveRoot ('Backups/' + $stamp)
        New-Item -ItemType Directory -Force $backupRoot | Out-Null
        foreach ($name in 'Profile.save', 'OldProfile.save', 'local_bsm2.settings') {
            $path = Join-Path $saveRoot $name
            if (Test-Path -LiteralPath $path) { Copy-Item -LiteralPath $path -Destination $backupRoot }
        }
    }
    foreach ($name in 'BSM2_DIAGNOSTICS', 'BSM2_LEGACY_RANDOM', 'BSM2_TAPS', 'BSM2_INPUT_FILE', 'BSM2_CAPTURE_EVERY', 'BSM2_BASELINE_PACING', 'BSM2_TRACE_CURSOR', 'ARC_TRACE_JNI', 'ARC_TRACE_ASSETS', 'ARC_TRACE_FILES', 'ARC_TRACE_THREADS', 'ARC_TRACE_GL') {
        Remove-Item ('Env:' + $name) -ErrorAction SilentlyContinue
    }
    $env:BSM2_BOOT = '1'
    $env:BSM2_PROFILE_ROOT = $saveRoot
    if ($Mute) { $env:SDL_AUDIODRIVER = 'dummy' }
    if ($TouchControls) { $env:BSM2_TOUCH_CONTROLS = '1' }
    else { Remove-Item Env:BSM2_TOUCH_CONTROLS -ErrorAction SilentlyContinue }
    $log = Join-Path $logRoot ($stamp + '.log')
    $gameArgs = @('--window', ('"--assets=' + (Join-Path $gameRoot 'Assets') + '"'), ('"' + (Join-Path $gameRoot 'libnative.so') + '"'))
    if ($Frames -gt 0) { $gameArgs += ('--frames=' + $Frames) }
    # Suppress the host console without supplying SW_HIDE to the SDL window.
    $start = New-Object Diagnostics.ProcessStartInfo
    $start.FileName = Join-Path $gameRoot 'BSM2Mobile.exe'
    $start.Arguments = $gameArgs -join ' '
    $start.WorkingDirectory = $saveRoot
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    # Disable FileStream buffering as well as the game's own stdio buffering.
    # Reports should remain complete even if this console is closed at a prompt.
    $outputStream = [IO.FileStream]::new($log, [IO.FileMode]::Create, [IO.FileAccess]::Write, [IO.FileShare]::Read, 1, [IO.FileOptions]::WriteThrough)
    $errorStream = [IO.FileStream]::new((Join-Path $logRoot ($stamp + '.errors.log')), [IO.FileMode]::Create, [IO.FileAccess]::Write, [IO.FileShare]::Read, 1, [IO.FileOptions]::WriteThrough)
    $process = [Diagnostics.Process]::Start($start)
    $outputCopy = $process.StandardOutput.BaseStream.CopyToAsync($outputStream)
    $errorCopy = $process.StandardError.BaseStream.CopyToAsync($errorStream)
    $process.WaitForExit()
    $outputCopy.Wait()
    $errorCopy.Wait()
    # Keep the complete failure report on disk before the error prompt waits.
    $outputStream.Flush()
    $errorStream.Flush()
    if ($process.ExitCode) { throw "Game stopped with code $($process.ExitCode). Details: $log" }
} catch {
    Write-Host $_.Exception.Message
    Read-Host 'Press Enter to close'
    exit 1
} finally {
    if ($outputStream) { $outputStream.Dispose() }
    if ($errorStream) { $errorStream.Dispose() }
    if ($process) { $process.Dispose() }
    if ($runLock) { $runLock.Dispose() }
}
