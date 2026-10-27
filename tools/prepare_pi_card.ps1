<#
Post-flash preparation and verification of a Raspberry Pi OS card, for win11
skyhorizon or aquamarine, PowerShell 5.1. Run it once after Raspberry Pi
Imager finishes and before the card is ejected.

It does not flash and it does not write any credential. Imager owns the user,
the password and the wireless key, all of which live in the cloud-init files
it writes; this script only checks they are there and applies the two boot
settings Imager has no field for.

WHY IT EXISTS. Doing this by hand on Monday 26 October 2026 produced a
duplicated console= parameter, because Raspberry Pi OS already ships
"console=serial0,115200 console=tty1" in cmdline.txt and the hand-written
prepend did not look first. Every change below is therefore idempotent: run
it twice and the second run reports "already set" and writes nothing.

    .\tools\prepare_pi_card.ps1
    .\tools\prepare_pi_card.ps1 -DriveLetter D -ExpectHostname rpi3BV1p2 -Ssid 3TubeA1B7Z9

Exit status is 0 when every required check passed and 1 when any failed, so
it can gate a flash in a longer script.
#>

[CmdletBinding()]
param(
    # Boot partition drive letter. Omitted, the script finds it by volume label.
    [string] $DriveLetter = "",
    # A directory to treat as the boot partition. For testing this script
    # against a synthetic card, which is how its checks were first proved to
    # fire. Takes precedence over -DriveLetter.
    [string] $Path = "",
    # Assert the hostname Imager wrote, when given.
    [string] $ExpectHostname = "",
    # Assert the account name Imager wrote. Chapters of this volume use bing.
    [string] $ExpectUser = "",
    # Assert this wireless network name appears in network-config, by name only.
    [string] $Ssid = "",
    # Add nothing, only report. Useful on a card already in a board's reader.
    [switch] $CheckOnly
)

$script:Failures = 0
$script:Changes = 0

function Say-Pass([string] $m) { Write-Host ("  pass    " + $m) }
function Say-Info([string] $m) { Write-Host ("  note    " + $m) }
function Say-Edit([string] $m) {
    Write-Host ("  WROTE   " + $m)
    $script:Changes = $script:Changes + 1
}
function Say-Fail([string] $m) {
    Write-Host ("  FAIL    " + $m)
    $script:Failures = $script:Failures + 1
}

# ---------------------------------------------------------------- the partition

if ($Path -ne "") {
    $root = $Path
    if (-not $root.EndsWith("\")) { $root = $root + "\" }
} elseif ($DriveLetter -eq "") {
    $vol = Get-Volume | Where-Object { $_.FileSystemLabel -like "boot*" }
    if (-not $vol) {
        Write-Host "No volume labelled boot* is mounted. Is the card in the reader?"
        exit 1
    }
    if (@($vol).Count -gt 1) {
        Write-Host "More than one boot* volume is mounted. Name one with -DriveLetter:"
        $vol | Select-Object DriveLetter, FileSystemLabel | Format-Table -AutoSize
        exit 1
    }
    $DriveLetter = $vol.DriveLetter
}
if ($Path -eq "") { $root = $DriveLetter + ":\" }
if (-not (Test-Path $root)) {
    Write-Host ("" + $root + " is not there.")
    exit 1
}
Write-Host ""
Write-Host ("Raspberry Pi card at " + $root)
Write-Host ""

# -------------------------------------------------------- did the image write

Write-Host "The image itself"
foreach ($f in @("config.txt", "cmdline.txt", "kernel8.img", "overlays")) {
    if (Test-Path (Join-Path $root $f)) { Say-Pass $f }
    else { Say-Fail ($f + " is missing, so the write did not complete") }
}
$dtb = @(Get-ChildItem $root -Filter "*.dtb" -ErrorAction SilentlyContinue)
if ($dtb.Count -ge 1) { Say-Pass ([string] $dtb.Count + " device tree blobs") }
else { Say-Fail "no .dtb files, so the write did not complete" }

# ------------------------------------------------------- did Imager customise

Write-Host ""
Write-Host "First boot customisation, which is what gives you a user and a network"
$cloud = @("user-data", "meta-data", "network-config")
$cloudFound = @($cloud | Where-Object { Test-Path (Join-Path $root $_) })
$legacyToml = Test-Path (Join-Path $root "custom.toml")
$legacyRun = Test-Path (Join-Path $root "firstrun.sh")

if ($cloudFound.Count -eq 3) {
    Say-Pass "cloud-init: user-data, meta-data and network-config, the Trixie format"
} elseif ($legacyToml) {
    Say-Pass "custom.toml, the older format"
} elseif ($legacyRun) {
    Say-Pass "firstrun.sh, the oldest format"
} else {
    Say-Fail "NO customisation of any format. The board would boot with no user,"
    Say-Fail "no SSH and no wireless. Rewrite the card with Imager's settings filled in."
}

$ud = Join-Path $root "user-data"
if (Test-Path $ud) {
    $host0 = Select-String -Path $ud -Pattern "^hostname:" | ForEach-Object { $_.Line }
    $user0 = Select-String -Path $ud -Pattern "^\s*-?\s*name:" | ForEach-Object { $_.Line }
    $ssh0 = Select-String -Path $ud -Pattern "enable_ssh|ssh_pwauth|ssh_authorized_keys"
    if ($host0) { Say-Info ("hostname line: " + ($host0 -join " ").Trim()) }
    if ($user0) { Say-Info ("account line:  " + ($user0 -join " ").Trim()) }
    if ($ssh0) { Say-Pass "SSH is enabled in user-data" }
    else { Say-Fail "no SSH line in user-data, so the board would be unreachable" }

    if ($ExpectHostname -ne "") {
        if (($host0 -join " ") -match [regex]::Escape($ExpectHostname)) {
            Say-Pass ("hostname is " + $ExpectHostname)
        } else {
            Say-Fail ("hostname is NOT " + $ExpectHostname)
        }
    }
    if ($ExpectUser -ne "") {
        if (($user0 -join " ") -match [regex]::Escape($ExpectUser)) {
            Say-Pass ("account is " + $ExpectUser)
        } else {
            Say-Fail ("account is NOT " + $ExpectUser + ". Every chapter uses")
            Say-Info ("/home/" + $ExpectUser + ", so reflash or create it on first boot.")
        }
    }
}

# network-config holds the wireless passphrase in clear text, so this only ever
# counts matches and never prints a line from it.
$nc = Join-Path $root "network-config"
if ((Test-Path $nc) -and ($Ssid -ne "")) {
    $n = @(Select-String -Path $nc -Pattern ([regex]::Escape($Ssid))).Count
    if ($n -ge 1) { Say-Pass ("the network name " + $Ssid + " is in network-config") }
    else { Say-Fail ("the network name " + $Ssid + " is NOT in network-config") }
}

# ------------------------------------------------- the two settings Imager lacks

Write-Host ""
Write-Host "Boot settings Imager has no field for"
$cfg = Join-Path $root "config.txt"
if (Test-Path $cfg) {
    $n = @(Select-String -Path $cfg -Pattern "^enable_uart=1").Count
    if ($n -eq 1) {
        Say-Pass "enable_uart=1 already set once"
    } elseif ($n -gt 1) {
        Say-Fail ("enable_uart=1 appears " + $n + " times in config.txt; remove the extras")
    } elseif ($CheckOnly) {
        Say-Info "enable_uart=1 absent, not added because -CheckOnly"
    } else {
        Add-Content -Path $cfg -Value "enable_uart=1" -Encoding ASCII
        Say-Edit "enable_uart=1 appended to config.txt"
    }
}

# cmdline.txt must stay ONE line. Appending a line stops the board booting, and
# Raspberry Pi OS already carries console=serial0,115200, so this checks before
# it writes rather than prepending blindly.
$cmd = Join-Path $root "cmdline.txt"
if (Test-Path $cmd) {
    $lines = @(Get-Content $cmd).Count
    if ($lines -ne 1) {
        Say-Fail ("cmdline.txt has " + $lines + " lines and must have exactly 1")
    } else {
        Say-Pass "cmdline.txt is one line"
    }
    $text = (Get-Content $cmd -Raw).Trim()
    $n = @([regex]::Matches($text, "console=serial0")).Count
    if ($n -eq 1) {
        Say-Pass "one console=serial0 parameter"
    } elseif ($n -gt 1) {
        if ($CheckOnly) {
            Say-Fail ("console=serial0 appears " + $n + " times, not fixed because -CheckOnly")
        } else {
            $fixed = $text -replace "console=serial0,115200 ", ""
            $fixed = "console=serial0,115200 " + $fixed
            Set-Content -Path $cmd -Value $fixed -NoNewline -Encoding ASCII
            Say-Edit ("collapsed " + $n + " console=serial0 parameters to one")
        }
    } elseif ($CheckOnly) {
        Say-Info "no console=serial0, not added because -CheckOnly"
    } else {
        $new = "console=serial0,115200 " + $text
        Set-Content -Path $cmd -Value $new -NoNewline -Encoding ASCII
        Say-Edit "console=serial0,115200 prepended to cmdline.txt"
    }
    $after = @(Get-Content $cmd).Count
    if ($after -ne 1) {
        Say-Fail ("cmdline.txt now has " + $after + " lines. Do NOT boot this card.")
    }
}

# ------------------------------------------------------------------- the verdict

Write-Host ""
if ($script:Failures -eq 0) {
    Write-Host ("Ready. " + $script:Changes + " change(s) written.")
    Write-Host "Eject through Safely Remove Hardware, not by pulling the card: the"
    Write-Host "customisation files are written last and sit in the write cache."
    exit 0
} else {
    Write-Host ($script:Failures.ToString() + " check(s) failed. Read them above before booting.")
    exit 1
}
