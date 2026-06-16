$ErrorActionPreference = "Stop"
$ErrorView = "ConciseView"

trap {
    Write-Host ""
    Write-Host "Error: $($_.Exception.Message)"
    exit 1
}

$Root = Resolve-Path (Join-Path $PSScriptRoot "..")
$Out = Join-Path $Root "dist\windows"
New-Item -ItemType Directory -Force -Path $Out | Out-Null
$env:GOPROXY = "https://proxy.golang.org,direct"

$TotalSteps = 4
$script:Step = 0
$script:LastLine = 0

function Progress-Line {
    param([int]$Percent, [string]$Text)
    if ($Percent -lt 0) { $Percent = 0 }
    if ($Percent -gt 100) { $Percent = 100 }
    $Width = 32
    $Filled = [math]::Floor($Width * $Percent / 100)
    $Bar = ("#" * $Filled).PadRight($Width, "-")
    if ($Text.Length -gt 76) { $Text = $Text.Substring(0, 73) + "..." }
    $Line = "`r[$Bar] {0,3}%  {1}" -f $Percent, $Text
    $Pad = ""
    if ($script:LastLine -gt $Line.Length) { $Pad = " " * ($script:LastLine - $Line.Length) }
    Write-Host -NoNewline ($Line + $Pad)
    $script:LastLine = $Line.Length
}

function Ask-YesNo {
    param([string]$Question)
    if ($script:LastLine -gt 0) {
        Write-Host ""
        $script:LastLine = 0
    }
    $Answer = Read-Host "$Question [y/N]"
    return $Answer -match "^(y|yes)$"
}

function Has-Command {
    param([string]$Name)
    return [bool](Get-Command $Name -ErrorAction SilentlyContinue)
}

function Step {
    param([string]$Text, [scriptblock]$Body)
    $script:Step += 1
    $Base = [math]::Round((($script:Step - 1) / $TotalSteps) * 100)
    Progress-Line $Base ("[$script:Step/$TotalSteps] $Text")
    $global:LASTEXITCODE = 0
    & $Body
    if ($LASTEXITCODE -ne 0) {
        throw "$Text failed"
    }
    Progress-Line ([math]::Round(($script:Step / $TotalSteps) * 100)) "Done: $Text"
}

function Install-Scoop {
    if (Has-Command "scoop") { return }
    if (-not (Ask-YesNo "Scoop was not found. Install Scoop now?")) {
        throw "Scoop is required to install missing Windows dependencies."
    }
    Progress-Line 5 "Installing Scoop"
    Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser -Force -ErrorAction Stop
    Invoke-RestMethod -Uri https://get.scoop.sh -ErrorAction Stop | Invoke-Expression
    $env:PATH = "$env:USERPROFILE\scoop\shims;$env:PATH"
}

function Ensure-ScoopPackage {
    param([string]$Command, [string]$Package)
    if (Has-Command $Command) { return }
    Install-Scoop
    if (-not (Ask-YesNo "$Command was not found. Install $Package with Scoop now?")) {
        throw "$Command is required."
    }
    Progress-Line 10 "scoop install $Package"
    scoop install $Package | Out-Null
    $env:PATH = "$env:USERPROFILE\scoop\shims;$env:PATH"
}

Step "Checking Go" {
    Ensure-ScoopPackage "go" "go"
}

Step "Checking C compiler" {
    Ensure-ScoopPackage "gcc" "gcc"
}

Step "Downloading Go modules" {
    Push-Location $Root
    go mod download
    Pop-Location
}

Step "Building executables" {
    Push-Location $Root
    go build -trimpath -ldflags "-s -w" -o "$Out\sharenet_server.exe" .\cmd\server
    go build -trimpath -ldflags "-s -w" -o "$Out\sharenet_client.exe" .\cmd\client
    Pop-Location
}

Progress-Line 100 "Done: $Out"
Write-Host ""
