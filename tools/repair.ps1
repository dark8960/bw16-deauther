# Pure-ASCII repair driver. Reads repair_data.txt (UTF-8) and rewrites the
# given line numbers in the sketch. All file I/O uses explicit UTF-8 APIs so
# the Windows PowerShell 5.1 ANSI defaults never touch the text.
$ErrorActionPreference = 'Stop'
$sketch = 'G:\wifideauth\firmware\wifideauth\wifideauth.ino'
$data   = 'G:\wifideauth\tools\repair_data.txt'

$utf8 = New-Object System.Text.UTF8Encoding($false)
$raw  = [System.IO.File]::ReadAllText($data, $utf8)
$lines = [System.IO.File]::ReadAllText($sketch, $utf8) -split "`n"

$count = 0
foreach ($row in ($raw -split "`r?`n")) {
    if ($row.Trim().Length -eq 0) { continue }
    $tab = $row.IndexOf([char]9)
    if ($tab -lt 1) { throw "bad row: $row" }
    $n = [int]$row.Substring(0, $tab)
    $text = $row.Substring($tab + 1)
    if ($n -lt 1 -or $n -gt $lines.Count) { throw "line $n out of range ($($lines.Count))" }
    $lines[$n - 1] = $text
    $count++
}
[System.IO.File]::WriteAllText($sketch, ($lines -join "`n"), $utf8)
"repaired $count lines; total $($lines.Count)"
