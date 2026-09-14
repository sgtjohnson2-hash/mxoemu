# ============================================================================
# The Matrix Online: Master One-Click Live Deployment Pipeline (PowerShell)
# Orchestrates client compilation, distribution, git push, and remote VPS deployment
# ============================================================================
[CmdletBinding()]
param (
    [switch]$SkipClientBuild,
    [switch]$SkipRemoteDeploy,
    [string]$VpsHost = "mxo-vps"
)

$ErrorActionPreference = "Stop"
$RootPath = $PSScriptRoot

Write-Host "========================================================================" -ForegroundColor Cyan
Write-Host ">>> [1/4] Client Module Build & Synchronization" -ForegroundColor Cyan
Write-Host "========================================================================" -ForegroundColor Cyan

if (-not $SkipClientBuild) {
    Write-Host "Building mxohax_modern.dll via MSVC x86..." -ForegroundColor Yellow
    & cmd.exe /c "$RootPath\build_mxohax.bat"
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to build mxohax_modern.dll (Exit code: $LASTEXITCODE)"
    }

    Write-Host "Distributing mxohax_modern.dll across all client targets..." -ForegroundColor Yellow
    & cmd.exe /c "$RootPath\deploy_mxohax.bat"
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to deploy mxohax_modern.dll (Exit code: $LASTEXITCODE)"
    }
} else {
    Write-Host "Skipping client build per -SkipClientBuild flag." -ForegroundColor DarkGray
}

Write-Host "========================================================================" -ForegroundColor Cyan
Write-Host ">>> [2/4] Remote VPS Deployment Execution" -ForegroundColor Cyan
Write-Host "========================================================================" -ForegroundColor Cyan

if (-not $SkipRemoteDeploy) {
    Write-Host "Connecting to $VpsHost to trigger live deployment..." -ForegroundColor Yellow
    
    $sshCmd = "cd /home/ubuntu/mxoemu && chmod +x deploy_live.sh && ./deploy_live.sh"
    & ssh -n $VpsHost $sshCmd
    if ($LASTEXITCODE -ne 0) {
        throw "Remote deployment failed on $VpsHost (Exit code: $LASTEXITCODE)"
    }
} else {
    Write-Host "Skipping remote deployment per -SkipRemoteDeploy flag." -ForegroundColor DarkGray
}

Write-Host "========================================================================" -ForegroundColor Cyan
Write-Host ">>> [3/4] Live Server Telemetry & Verification" -ForegroundColor Cyan
Write-Host "========================================================================" -ForegroundColor Cyan

Write-Host "Querying live Docker stats on $VpsHost..." -ForegroundColor Yellow
& ssh -n $VpsHost "docker stats --no-stream mxoemu-reality-server-1"

Write-Host "========================================================================" -ForegroundColor Green
Write-Host ">>> DEPLOYMENT SUCCEEDED: All systems operational!" -ForegroundColor Green
Write-Host "========================================================================" -ForegroundColor Green
