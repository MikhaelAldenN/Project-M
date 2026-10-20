<# : batch bootstrap
@echo off
setlocal DisableDelayedExpansion
set "PACK_SELF=%~f0"
set PACK_ARGS=%*
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -Command "Invoke-Expression ([System.IO.File]::ReadAllText($env:PACK_SELF))"
exit /b %ERRORLEVEL%
#>

# ============================================================================
#  pack_code - packs source files into one text bundle for AI assistants
#
#  Everything below is PowerShell (Windows PowerShell 5.1 compatible).
#  Keep this file ASCII-only and saved with CRLF line endings.
#  Run "pack_code.bat -Help" for usage.
# ============================================================================

$ErrorActionPreference = 'Stop'

$PackVersion = '2.1'
$ConfigName  = '.packcode'
$DefaultOut  = 'context_bundle.txt'
$BundleMark  = '<bundle_info>'

# Extensions collected when a FOLDER is packed. Files given explicitly are
# always packed, whatever their extension.
$DefaultExt = ('c cc cpp cxx h hh hpp hxx inl ipp ' +
               'hlsl hlsli fx fxh glsl vert frag geom comp tesc tese shader cginc compute usf ush ' +
               'cs py lua gd js jsx ts tsx java kt rs go html css cmake').Split(' ')
$DefaultNames = @('CMakeLists.txt')

# Header-like extensions are placed right before the matching source file.
$HeaderExt = 'h hh hpp hxx inl ipp hlsli fxh cginc ush'.Split(' ')

# Folder names that are never entered while walking a folder.
$DefaultExclude = ('.git .svn .hg .vs .vscode .idea ' +
                   'bin obj x64 x86 Win32 ARM64 Debug Release build out dist ' +
                   'Intermediate Binaries Saved DerivedDataCache Library Temp Logs ' +
                   'node_modules packages __pycache__ .venv venv ' +
                   'ThirdParty third_party 3rdparty External Externals vendor').Split(' ')

$Sep        = [IO.Path]::DirectorySeparatorChar
$NL         = "`r`n"
$Utf8NoBom  = New-Object System.Text.UTF8Encoding($false)
$Utf8Strict = New-Object System.Text.UTF8Encoding($false, $true)
$IgnoreCase = [System.StringComparison]::OrdinalIgnoreCase

$Ctx = @{
    Base     = $null      # project root guessed before any input is known
    BaseOpt  = $null
    Inputs   = New-Object System.Collections.Generic.List[object]
    InputSet = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
    Bundles  = New-Object System.Collections.Generic.List[string]
    NotFound = 0
    Cli      = @{}     # option overrides: linenumbers, ext, maxkb, ...
    Flags    = @{ ClearHistory = $false; Repack = $false; Out = $null; SavePreset = $null; Clip = $false; Help = $false; Opt = $false }
    NoPause  = [bool]$env:PACK_NOPAUSE
    ExitCode = 0
}

# ----------------------------------------------------------------------------
#  Small helpers
# ----------------------------------------------------------------------------
function Split-Args([string]$s) {
    # Splits a command line into tokens. A closing quote always ends a token,
    # so "a""b" (Windows 11 multi-file paste) becomes two tokens.
    $out = New-Object System.Collections.Generic.List[string]
    if ([string]::IsNullOrWhiteSpace($s)) { return ,$out }
    $sb = New-Object System.Text.StringBuilder
    $inQuote = $false
    foreach ($ch in $s.ToCharArray()) {
        if ($ch -eq [char]'"') {
            if ($sb.Length -gt 0) { $out.Add($sb.ToString()); [void]$sb.Clear() }
            $inQuote = -not $inQuote
        } elseif ((-not $inQuote) -and [char]::IsWhiteSpace($ch)) {
            if ($sb.Length -gt 0) { $out.Add($sb.ToString()); [void]$sb.Clear() }
        } else {
            [void]$sb.Append($ch)
        }
    }
    if ($sb.Length -gt 0) { $out.Add($sb.ToString()) }
    return ,$out
}

function Split-List([string]$v, [string]$pattern) {
    $r = New-Object System.Collections.Generic.List[string]
    if ($v) {
        foreach ($p in ($v -split $pattern)) {
            $p = $p.Trim()
            if ($p) { $r.Add($p) }
        }
    }
    return ,$r
}

function Split-ExtList([string]$v) {
    $r = New-Object System.Collections.Generic.List[string]
    foreach ($p in (Split-List $v '[,;\s]+')) {
        $e = $p.TrimStart('*').TrimStart('.').ToLower()
        if ($e) { $r.Add($e) }
    }
    return ,$r
}

function Test-On([string]$v) { return ($v -match '^(on|true|yes|1)$') }

function Test-Under([string]$path, [string]$dir) {
    $p = $path.TrimEnd($Sep)
    $d = $dir.TrimEnd($Sep)
    if ($p.Length -eq $d.Length) { return [string]::Equals($p, $d, $IgnoreCase) }
    return $p.StartsWith($d + $Sep, $IgnoreCase)
}

function Get-Rel([string]$path, [string]$root) {
    if (Test-Under $path $root) {
        $r = $path.Substring($root.TrimEnd($Sep).Length).TrimStart($Sep)
        if ($r -eq '') { return '.' }
        return $r.Replace('\', '/')
    }
    return $path.Replace('\', '/')
}

function Get-CommonDir($dirs) {
    $c = $dirs[0]
    while ($c) {
        $all = $true
        foreach ($d in $dirs) {
            if (-not (Test-Under $d $c)) { $all = $false; break }
        }
        if ($all) { return $c }
        $c = [IO.Path]::GetDirectoryName($c)
    }
    return $null
}

function Find-Root([string]$start) {
    # 1) nearest folder (going up) that has a .packcode file
    $d = $start
    while ($d) {
        if ([IO.File]::Exists([IO.Path]::Combine($d, $ConfigName))) { return $d }
        $d = [IO.Path]::GetDirectoryName($d)
    }
    # 2) nearest folder that looks like a project root
    $d = $start
    while ($d) {
        try {
            if ([IO.Directory]::Exists([IO.Path]::Combine($d, '.git')) -or
                [IO.File]::Exists([IO.Path]::Combine($d, '.git'))) { return $d }
            if ([IO.Directory]::GetFiles($d, '*.sln').Length -gt 0) { return $d }
            if ([IO.Directory]::GetFiles($d, '*.uproject').Length -gt 0) { return $d }
        } catch { }
        $d = [IO.Path]::GetDirectoryName($d)
    }
    return $null
}

function Read-Head([string]$path, [int]$max) {
    $fs = [IO.File]::OpenRead($path)
    try {
        $buf = New-Object byte[] $max
        $n = $fs.Read($buf, 0, $max)
        return $Utf8NoBom.GetString($buf, 0, $n)
    } finally {
        $fs.Dispose()
    }
}

function Test-IsBundle([string]$path) {
    try { return (Read-Head $path 64).StartsWith($BundleMark) } catch { return $false }
}

function ConvertTo-Cli([string]$s) {
    $cli = @{}
    foreach ($kv in (Split-List $s '\|')) {
        $eq = $kv.IndexOf('=')
        if ($eq -gt 0) { $cli[$kv.Substring(0, $eq).Trim()] = $kv.Substring($eq + 1).Trim() }
    }
    return $cli
}

function Read-BundleHeader([string]$path) {
    $res = @{ Inputs = @(); Cli = @{} }
    $head = Read-Head $path 65536
    $m = [regex]::Match($head, '(?m)^\[pack\] inputs:(.*)$')
    if ($m.Success) {
        $res.Inputs = Split-List $m.Groups[1].Value '\|'
    }
    $m = [regex]::Match($head, '(?m)^\[pack\] options:(.*)$')
    if ($m.Success) {
        $res.Cli = ConvertTo-Cli $m.Groups[1].Value
    }
    return $res
}

function Read-TextFile([string]$path, [int]$codePage) {
    # Returns @{Text; Enc} or $null when the file looks binary.
    $bytes = [IO.File]::ReadAllBytes($path)
    $len = $bytes.Length
    if ($len -eq 0) { return @{ Text = ''; Enc = 'utf-8' } }
    if ($len -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF) {
        return @{ Text = $Utf8NoBom.GetString($bytes, 3, $len - 3); Enc = 'utf-8' }
    }
    if ($len -ge 2 -and $bytes[0] -eq 0xFF -and $bytes[1] -eq 0xFE) {
        return @{ Text = [Text.Encoding]::Unicode.GetString($bytes, 2, $len - 2); Enc = 'utf-16' }
    }
    if ($len -ge 2 -and $bytes[0] -eq 0xFE -and $bytes[1] -eq 0xFF) {
        return @{ Text = [Text.Encoding]::BigEndianUnicode.GetString($bytes, 2, $len - 2); Enc = 'utf-16' }
    }
    $probe = [Math]::Min($len, 8192)
    if ([Array]::IndexOf($bytes, [byte]0, 0, $probe) -ge 0) { return $null }
    try {
        return @{ Text = $Utf8Strict.GetString($bytes); Enc = 'utf-8' }
    } catch { }
    # Not valid UTF-8: fall back to a legacy code page (system default, e.g. 932 = Shift-JIS)
    $enc = [Text.Encoding]::Default
    if ($codePage -gt 0) { $enc = [Text.Encoding]::GetEncoding($codePage) }
    return @{ Text = $enc.GetString($bytes); Enc = ('cp' + $enc.CodePage) }
}

# ----------------------------------------------------------------------------
#  Options: defaults <- .packcode in the project root <- command line
# ----------------------------------------------------------------------------
function Get-Options([string]$root, $cli, [bool]$quiet) {
    $ext = New-Object System.Collections.Generic.List[string]
    $ext.AddRange([string[]]$DefaultExt)
    $exc = New-Object System.Collections.Generic.List[string]
    $exc.AddRange([string[]]$DefaultExclude)
    $o = @{
        LineNumbers = $true
        KeepBlank   = $false
        MaxKB       = 512
        CodePage    = 0
        Out         = $DefaultOut
        Presets     = @{}
        ConfigPath  = $null
    }

    $pairs = New-Object System.Collections.Generic.List[object]
    if ($root) {
        $cfg = [IO.Path]::Combine($root, $ConfigName)
        if ([IO.File]::Exists($cfg)) {
            $o.ConfigPath = $cfg
            $r = Read-TextFile $cfg 0
            if ($r) {
                foreach ($line in [regex]::Split($r.Text, "\r\n|\n|\r")) {
                    $l = $line.Trim()
                    if ((-not $l) -or $l.StartsWith('#')) { continue }
                    $eq = $l.IndexOf('=')
                    if ($eq -lt 1) { continue }
                    $pairs.Add(@($l.Substring(0, $eq).Trim().ToLower(), $l.Substring($eq + 1).Trim(), $true))
                }
            }
        }
    }
    if ($cli) {
        foreach ($k in @('ext', 'ext+', 'exclude+', 'linenumbers', 'keepblank', 'maxkb', 'codepage')) {
            if ($cli.ContainsKey($k)) { $pairs.Add(@($k, [string]$cli[$k], $false)) }
        }
    }

    foreach ($pair in $pairs) {
        $k = $pair[0]; $v = $pair[1]; $fromFile = $pair[2]
        try {
            if     ($k -eq 'ext')         { $ext = Split-ExtList $v }
            elseif ($k -eq 'ext+')        { $ext.AddRange([string[]](Split-ExtList $v)) }
            elseif ($k -eq 'exclude')     { $exc = Split-List $v '[,;]' }
            elseif ($k -eq 'exclude+')    { $exc.AddRange([string[]](Split-List $v '[,;]')) }
            elseif ($k -eq 'linenumbers') { $o.LineNumbers = Test-On $v }
            elseif ($k -eq 'keepblank')   { $o.KeepBlank = Test-On $v }
            elseif ($k -eq 'maxkb')       { $o.MaxKB = [int]$v }
            elseif ($k -eq 'codepage')    { $o.CodePage = [int]$v }
            elseif ($k -eq 'out' -and $fromFile -and $v) { $o.Out = [IO.Path]::GetFileName($v) }
            elseif ($k.StartsWith('preset.') -and $k.Length -gt 7) { $o.Presets[$k.Substring(7)] = $v }
            elseif ($fromFile -and (-not $quiet)) { Write-Host "[Warning] Unknown key in ${ConfigName}: $k" -ForegroundColor Yellow }
        } catch {
            if (-not $quiet) { Write-Host "[Warning] Bad value for '$k': $v" -ForegroundColor Yellow }
        }
    }

    $o.ExtSet     = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
    $o.ExcludeSet = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
    $o.NameSet    = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
    foreach ($e in $ext)          { [void]$o.ExtSet.Add($e) }
    foreach ($e in $exc)          { [void]$o.ExcludeSet.Add($e) }
    foreach ($e in $DefaultNames) { [void]$o.NameSet.Add($e) }
    return $o
}

function Format-Cli($cli) {
    $parts = New-Object System.Collections.Generic.List[string]
    foreach ($k in @('linenumbers', 'keepblank', 'maxkb', 'codepage', 'ext', 'ext+', 'exclude+')) {
        if ($cli.ContainsKey($k)) { $parts.Add($k + '=' + $cli[$k]) }
    }
    return ($parts -join ' | ')
}

# ----------------------------------------------------------------------------
#  History of recent packs (per Windows user, never stored in the project).
#  One line per pack: root <TAB> bundle path <TAB> inputs <TAB> options
# ----------------------------------------------------------------------------
function Get-HistoryPath {
    $base = $env:LOCALAPPDATA
    if (-not $base) { $base = [IO.Path]::GetTempPath() }
    return [IO.Path]::Combine($base, 'pack_code', 'recent.txt')
}

function Get-History {
    $list = New-Object System.Collections.Generic.List[object]
    try {
        $hp = Get-HistoryPath
        if ([IO.File]::Exists($hp)) {
            foreach ($l in [IO.File]::ReadAllLines($hp, $Utf8NoBom)) {
                $f = $l.Split("`t")
                if ($f.Length -ne 4 -or $list.Count -ge 5) { continue }
                if (-not [IO.Directory]::Exists($f[0])) { continue }
                $list.Add([pscustomobject]@{ Root = $f[0]; Out = $f[1]; Inputs = $f[2]; Options = $f[3] })
            }
        }
    } catch { }
    return ,$list
}

function Save-History([string]$root, [string]$out, [string]$inputs, [string]$options) {
    try {
        $lines = New-Object System.Collections.Generic.List[string]
        $lines.Add(($root, $out, $inputs, $options) -join "`t")
        foreach ($h in (Get-History)) {
            $same = [string]::Equals($h.Root, $root, $IgnoreCase) -and [string]::Equals($h.Inputs, $inputs, $IgnoreCase)
            if ((-not $same) -and $lines.Count -lt 5) { $lines.Add(($h.Root, $h.Out, $h.Inputs, $h.Options) -join "`t") }
        }
        $hp = Get-HistoryPath
        [void][IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($hp))
        [IO.File]::WriteAllLines($hp, [string[]]$lines, $Utf8NoBom)
    } catch { }
}

function Remove-PackHistory {
    try {
        $hp = Get-HistoryPath
        if ([IO.File]::Exists($hp)) { [IO.File]::Delete($hp) }
        Write-Host 'History cleared.' -ForegroundColor Green
    } catch {
        Write-Host "[Error]  Could not clear the history: $($_.Exception.Message)" -ForegroundColor Red
    }
}

function ConvertTo-Recipe($rec) {
    return @{ Root = $rec.Root; Inputs = (Split-List $rec.Inputs '\|'); Cli = (ConvertTo-Cli $rec.Options) }
}

# ----------------------------------------------------------------------------
#  Turning what the user gave us into a list of inputs
# ----------------------------------------------------------------------------
function Get-ExistingPath([string]$t) {
    $cands = New-Object System.Collections.Generic.List[string]
    try {
        if ([IO.Path]::IsPathRooted($t)) {
            $cands.Add([IO.Path]::GetFullPath($t))
        } else {
            if ($Ctx.Base) { $cands.Add([IO.Path]::GetFullPath([IO.Path]::Combine($Ctx.Base, $t))) }
            $cands.Add([IO.Path]::GetFullPath([IO.Path]::Combine((Get-Location).Path, $t)))
        }
    } catch { }
    foreach ($p in $cands) {
        if ([IO.File]::Exists($p) -or [IO.Directory]::Exists($p)) { return $p }
    }
    return $null
}

function Add-Input([string]$full) {
    if ($full.Length -gt 3) { $full = $full.TrimEnd('\', '/') }
    $isDir = [IO.Directory]::Exists($full)
    if ((-not $isDir) -and (Test-IsBundle $full)) {
        if (-not $Ctx.Bundles.Contains($full)) { $Ctx.Bundles.Add($full) }
        return
    }
    if (-not $Ctx.InputSet.Add($full)) { return }
    $Ctx.Inputs.Add([pscustomobject]@{ Path = $full; IsDir = $isDir })
    if ($isDir) { Write-Host "[Folder] $full" } else { Write-Host "[File]   $full" }
}

function Find-ByName([string]$pattern, [string]$base, $opt) {
    # Looks for files or folders whose NAME matches (wildcards allowed).
    $hits = New-Object System.Collections.Generic.List[string]
    $stack = New-Object 'System.Collections.Generic.Stack[string]'
    $stack.Push($base)
    while ($stack.Count -gt 0) {
        $d = $stack.Pop()
        try {
            $files = [IO.Directory]::GetFiles($d)
            $subs  = [IO.Directory]::GetDirectories($d)
        } catch { continue }
        foreach ($f in $files) {
            if ([IO.Path]::GetFileName($f) -like $pattern) { $hits.Add($f) }
        }
        foreach ($s in $subs) {
            $n = [IO.Path]::GetFileName($s)
            if ($opt.ExcludeSet.Contains($n)) { continue }
            if ($n -like $pattern) { $hits.Add($s); continue }
            try {
                if (([IO.File]::GetAttributes($s) -band [IO.FileAttributes]::ReparsePoint) -ne 0) { continue }
            } catch { continue }
            $stack.Push($s)
        }
    }
    return ,$hits
}

function Resolve-Preset([string]$name) {
    if ((-not $Ctx.Base) -or (-not $Ctx.BaseOpt) -or (-not $Ctx.BaseOpt.Presets.ContainsKey($name))) {
        $known = ''
        if ($Ctx.BaseOpt -and $Ctx.BaseOpt.Presets.Count -gt 0) { $known = ' Available: ' + (($Ctx.BaseOpt.Presets.Keys | Sort-Object) -join ', ') }
        Write-Host "[Error]  Preset not found: $name.$known" -ForegroundColor Red
        Write-Host "         Presets are read from $ConfigName in the project root; run the script from inside the project." -ForegroundColor Red
        $Ctx.NotFound++
        return
    }
    Write-Host "[Preset] $name"
    foreach ($item in (Split-List $Ctx.BaseOpt.Presets[$name] '\|')) {
        $p = $Ctx.Base
        if ($item -ne '.') { $p = [IO.Path]::GetFullPath([IO.Path]::Combine($Ctx.Base, $item.Replace('/', $Sep))) }
        if ([IO.File]::Exists($p) -or [IO.Directory]::Exists($p)) { Add-Input $p }
        else { Write-Host "[Missing] $item" -ForegroundColor Yellow; $Ctx.NotFound++ }
    }
}

function Resolve-Token([string]$tok) {
    $t = $tok.Trim().Trim('"')
    if (-not $t) { return }
    $p = Get-ExistingPath $t
    if ($p) { Add-Input $p; return }
    if ($t.StartsWith('@') -and $t.Length -gt 1) { Resolve-Preset $t.Substring(1); return }
    if ($Ctx.Base -and $t.IndexOfAny([char[]]'\/:') -lt 0) {
        Write-Host "[Search] Looking for ""$t"" under $($Ctx.Base)"
        $hits = Find-ByName $t $Ctx.Base $Ctx.BaseOpt
        if ($hits.Count -gt 0) {
            foreach ($h in $hits) { Add-Input $h }
            return
        }
    }
    Write-Host "[Error]  Not found: $t" -ForegroundColor Red
    $Ctx.NotFound++
}

function Get-FileList($opt, $skippedDirs) {
    $seen  = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
    $files = New-Object System.Collections.Generic.List[string]
    foreach ($in in $Ctx.Inputs) {
        if (-not $in.IsDir) {
            if ($seen.Add($in.Path)) { $files.Add($in.Path) }
            continue
        }
        $stack = New-Object 'System.Collections.Generic.Stack[string]'
        $stack.Push($in.Path)
        while ($stack.Count -gt 0) {
            $d = $stack.Pop()
            try {
                $fs = [IO.Directory]::GetFiles($d)
                $ds = [IO.Directory]::GetDirectories($d)
            } catch {
                Write-Host "  [Warning] Cannot read folder: $d" -ForegroundColor Yellow
                continue
            }
            foreach ($f in $fs) {
                $e = [IO.Path]::GetExtension($f).TrimStart('.')
                if ($opt.ExtSet.Contains($e) -or $opt.NameSet.Contains([IO.Path]::GetFileName($f))) {
                    if ($seen.Add($f)) { $files.Add($f) }
                }
            }
            foreach ($s in $ds) {
                if ($opt.ExcludeSet.Contains([IO.Path]::GetFileName($s))) { $skippedDirs.Add($s); continue }
                try {
                    if (([IO.File]::GetAttributes($s) -band [IO.FileAttributes]::ReparsePoint) -ne 0) { continue }
                } catch { continue }
                $stack.Push($s)
            }
        }
    }
    return ,$files
}

# ----------------------------------------------------------------------------
#  Writing the bundle
# ----------------------------------------------------------------------------
function Build-Bundle([string]$root, $opt, [string]$outPath, $cli, [bool]$noClip) {
    $skippedDirs  = New-Object System.Collections.Generic.List[string]
    $skippedFiles = New-Object System.Collections.Generic.List[string]
    $headerSet = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
    foreach ($h in $HeaderExt) { [void]$headerSet.Add($h) }

    $files = Get-FileList $opt $skippedDirs
    $entries = foreach ($f in $files) {
        if ([string]::Equals($f, $outPath, $IgnoreCase)) { continue }
        $rel = Get-Rel $f $root
        $slash = $rel.LastIndexOf('/')
        $dir = ''
        if ($slash -ge 0) { $dir = $rel.Substring(0, $slash) }
        $ext = [IO.Path]::GetExtension($f).TrimStart('.').ToLower()
        $rank = 1
        if ($headerSet.Contains($ext)) { $rank = 0 }
        [pscustomobject]@{ Full = $f; Rel = $rel; Dir = $dir; Base = [IO.Path]::GetFileNameWithoutExtension($f); Ext = $ext; Rank = $rank }
    }
    $entries = @($entries | Sort-Object Dir, Base, Rank, Ext)

    $body     = New-Object System.Text.StringBuilder
    $manifest = New-Object System.Text.StringBuilder
    $count = 0; $totalLines = 0; $legacy = 0; $legacyName = ''

    Write-Host ''
    Write-Host 'Packing:'
    foreach ($e in $entries) {
        $size = (New-Object System.IO.FileInfo($e.Full)).Length
        if ($opt.MaxKB -gt 0 -and $size -gt ($opt.MaxKB * 1024)) {
            $skippedFiles.Add("$($e.Rel)  (larger than $($opt.MaxKB) KB)")
            continue
        }
        $r = $null
        try { $r = Read-TextFile $e.Full $opt.CodePage } catch {
            $skippedFiles.Add("$($e.Rel)  (cannot read: $($_.Exception.Message))")
            continue
        }
        if ($null -eq $r) { $skippedFiles.Add("$($e.Rel)  (binary)"); continue }
        if ($r.Enc -like 'cp*') { $legacy++; $legacyName = $r.Enc }

        $lines = [regex]::Split($r.Text, "\r\n|\n|\r")
        $n = $lines.Length
        if ($n -gt 0 -and $lines[$n - 1] -eq '') { $n-- }

        [void]$body.Append('<file path="').Append($e.Rel).Append('" lines="').Append($n).Append('">').Append($NL)
        for ($i = 0; $i -lt $n; $i++) {
            $ln = $lines[$i].TrimEnd()
            if ($ln.Length -eq 0) {
                if (-not $opt.KeepBlank) { continue }
                if ($opt.LineNumbers) { [void]$body.Append($i + 1).Append(':') }
                [void]$body.Append($NL)
                continue
            }
            if ($opt.LineNumbers) { [void]$body.Append($i + 1).Append(': ') }
            [void]$body.Append($ln).Append($NL)
        }
        [void]$body.Append('</file>').Append($NL).Append($NL)
        [void]$manifest.Append($e.Rel).Append(' | ').Append($n).Append(' lines').Append($NL)

        $count++
        $totalLines += $n
        Write-Host "  - $($e.Rel)"
    }

    if ($count -eq 0) {
        Write-Host '  (nothing)'
        Write-Host ''
        Write-Host '[!] No matching source files were found. Nothing was written.' -ForegroundColor Yellow
        foreach ($s in $skippedFiles) { Write-Host "    skipped: $s" -ForegroundColor Yellow }
        return $false
    }

    $main = '<manifest>' + $NL + $manifest.ToString() + '</manifest>' + $NL + $NL + $body.ToString()

    $kb = [int][Math]::Ceiling($Utf8NoBom.GetByteCount($main) / 1024.0)

    $relInputs = foreach ($in in $Ctx.Inputs) { Get-Rel $in.Path $root }

    $h = New-Object System.Text.StringBuilder
    [void]$h.Append($BundleMark).Append($NL)
    [void]$h.Append("Source code bundle generated by pack_code $PackVersion on $(Get-Date -Format 'yyyy-MM-dd HH:mm').").Append($NL)
    [void]$h.Append("Files: $count | Lines: $totalLines | Size: about $kb KB").Append($NL)
    [void]$h.Append('Layout: a <manifest> listing every file, then each file inside <file path="..."> ... </file>.').Append($NL)
    [void]$h.Append('Paths are relative to the project root and use forward slashes.').Append($NL)
    if ($opt.LineNumbers) {
        [void]$h.Append('Line numbers: every code line starts with its ORIGINAL line number and a colon, e.g. "212: code".').Append($NL)
        [void]$h.Append('  The prefix is not part of the source. Cite these numbers when pointing at code; never include them when writing code.').Append($NL)
        if (-not $opt.KeepBlank) {
            [void]$h.Append('  Blank lines are omitted, so gaps in the numbering are expected.').Append($NL)
        }
    } else {
        [void]$h.Append('Line numbers: NOT included. Do not state line numbers for this bundle; refer to code by file path and function name.').Append($NL)
        if (-not $opt.KeepBlank) {
            [void]$h.Append('  Blank lines are omitted.').Append($NL)
        }
    }
    [void]$h.Append('[pack] inputs: ').Append(($relInputs -join ' | ')).Append($NL)
    [void]$h.Append('[pack] options: ').Append((Format-Cli $cli)).Append($NL)
    [void]$h.Append('</bundle_info>').Append($NL).Append($NL)

    $final = $h.ToString() + $main
    [IO.File]::WriteAllText($outPath, $final, $Utf8NoBom)

    $clip = $false
    if (-not $noClip) {
        try { Set-Clipboard -Value $final; $clip = $true } catch { }
    }

    Write-Host ''
    Write-Host '------------------------------------------------'
    Write-Host "Packed $count file(s), $totalLines lines  (about $kb KB)" -ForegroundColor Green
    Write-Host "Bundle : $outPath"
    Write-Host "Root   : $root"
    if ($opt.ConfigPath) { Write-Host "Config : $($opt.ConfigPath)" }
    if ($clip) { Write-Host 'Copied to clipboard.' }
    elseif (-not $noClip) { Write-Host 'Could not copy to clipboard (the file was still written).' -ForegroundColor Yellow }
    if ($legacy -gt 0) {
        Write-Host "$legacy file(s) were not UTF-8 and were read as $legacyName." -ForegroundColor Yellow
        Write-Host '  If comments look garbled in the bundle, set the right one with -CodePage (932 = Shift-JIS).' -ForegroundColor Yellow
    }
    if ($skippedDirs.Count -gt 0) {
        Write-Host "Skipped $($skippedDirs.Count) excluded folder(s):" -ForegroundColor Yellow
        $shown = 0
        foreach ($s in $skippedDirs) {
            if ($shown -ge 12) { Write-Host "    ... and $($skippedDirs.Count - 12) more" -ForegroundColor Yellow; break }
            Write-Host "    $(Get-Rel $s $root)" -ForegroundColor Yellow
            $shown++
        }
    }
    if ($skippedFiles.Count -gt 0) {
        Write-Host "Skipped $($skippedFiles.Count) file(s):" -ForegroundColor Yellow
        foreach ($s in $skippedFiles) { Write-Host "    $s" -ForegroundColor Yellow }
    }
    Write-Host '------------------------------------------------'
    return $true
}

function Save-Preset([string]$root, [string]$name) {
    $cfg = [IO.Path]::Combine($root, $ConfigName)
    $lines = New-Object System.Collections.Generic.List[string]
    if ([IO.File]::Exists($cfg)) {
        $r = Read-TextFile $cfg 0
        if ($r) {
            foreach ($l in [regex]::Split($r.Text.TrimEnd(), "\r\n|\n|\r")) {
                if ($l -match ('^\s*preset\.' + [regex]::Escape($name) + '\s*=')) { continue }
                $lines.Add($l)
            }
        }
    } else {
        $lines.Add('# pack_code project settings. Run "pack_code.bat -Help" for the list of keys.')
    }
    $rel = foreach ($in in $Ctx.Inputs) { Get-Rel $in.Path $root }
    $lines.Add('preset.' + $name + ' = ' + ($rel -join ' | '))
    [IO.File]::WriteAllLines($cfg, [string[]]$lines, $Utf8NoBom)
    Write-Host "Preset saved: @$name  ->  $cfg"
}

# ----------------------------------------------------------------------------
#  User interface
# ----------------------------------------------------------------------------
function Show-Options([bool]$interactive) {
    if ($interactive) {
        Write-Host ''
        Write-Host 'OPTIONS  (type one or more, then press ENTER; they apply to this pack)'
    } else {
        Write-Host 'OPTIONS'
    }
    $text = @(
        '  -opt                 show this list',
        '  -Repack              repack the most recent bundle',
        '  -ClearHistory        forget the list of recent packs',
        '  -Preset NAME         pack a preset from .packcode (same as typing @NAME)',
        '  -SavePreset NAME     save the current inputs as a preset in .packcode',
        '  -NoLineNumbers       do not prefix lines with their line number',
        '  -LineNumbers         force line numbers on (overrides .packcode)',
        '  -KeepBlank           keep blank lines',
        '  -Ext cpp,h           pack only these extensions from folders',
        '  -AddExt json,md      add extensions to the default list',
        '  -Exclude Tools,Docs  extra folder names to skip',
        '  -MaxKB 512           skip files larger than this (0 = no limit)',
        '  -CodePage 932        code page for files that are not UTF-8 (default: system)',
        '  -Out name.txt        bundle file name (always written next to pack_code.bat)',
        '  -Clip                also copy the bundle to the clipboard',
        '  -NoPause             do not wait for ENTER at the end'
    )
    foreach ($l in $text) { Write-Host $l }
    Write-Host ''
}

function Read-Options($tokens, $raw, [bool]$echo) {
    # Applies every -Option found in $tokens; everything else is added to $raw.
    $cli = $Ctx.Cli
    $flags = $Ctx.Flags
    $needsValue = @('-ext', '-addext', '-exclude', '-maxkb', '-codepage', '-out', '-preset', '-savepreset')
    $i = 0
    while ($i -lt $tokens.Count) {
        $t = $tokens[$i]; $i++
        if ($t -eq '/?') { $flags.Help = $true }
        elseif ($t.StartsWith('-') -and (-not (Get-ExistingPath $t))) {
            $name = $t.ToLower()
            $val = $null
            if ($needsValue -contains $name) {
                if ($i -ge $tokens.Count) { throw "Option $t needs a value, e.g. $t something" }
                $val = $tokens[$i]; $i++
            }
            $quiet = $false
            if     ($name -eq '-help' -or $name -eq '-h' -or $name -eq '-?') { $flags.Help = $true; $quiet = $true }
            elseif ($name -eq '-opt' -or $name -eq '-options') { $flags.Opt = $true; $quiet = $true }
            elseif ($name -eq '-repack')        { $flags.Repack = $true; $quiet = $true }
            elseif ($name -eq '-clearhistory')  { $flags.ClearHistory = $true; $quiet = $true }
            elseif ($name -eq '-nolinenumbers') { $cli['linenumbers'] = 'off' }
            elseif ($name -eq '-linenumbers')   { $cli['linenumbers'] = 'on' }
            elseif ($name -eq '-keepblank')     { $cli['keepblank'] = 'on' }
            elseif ($name -eq '-clip')          { $flags.Clip = $true }
            elseif ($name -eq '-noclip')        { $flags.Clip = $false }
            elseif ($name -eq '-nopause')       { $Ctx.NoPause = $true }
            elseif ($name -eq '-ext')           { $cli['ext'] = ((Split-ExtList $val) -join ',') }
            elseif ($name -eq '-addext')        { $cli['ext+'] = ((Split-ExtList $val) -join ',') }
            elseif ($name -eq '-exclude')       { $cli['exclude+'] = ((Split-List $val '[,;]') -join ',') }
            elseif ($name -eq '-maxkb')         { $cli['maxkb'] = [string][int]$val }
            elseif ($name -eq '-codepage')      { $cli['codepage'] = [string][int]$val }
            elseif ($name -eq '-out')           { $flags.Out = [IO.Path]::GetFileName($val) }
            elseif ($name -eq '-preset')        { $raw.Add('@' + $val); $quiet = $true }
            elseif ($name -eq '-savepreset')    { $flags.SavePreset = $val }
            else { throw "Unknown option: $t  (-opt shows the list)" }
            if ($echo -and (-not $quiet)) { Write-Host ("[Option] $t $val").TrimEnd() }
        }
        else { $raw.Add($t) }
    }
}

function Show-Help {
    $text = @(
        "pack_code $PackVersion - packs source files into one text bundle for AI assistants",
        '',
        'USAGE',
        '  Drag files/folders onto pack_code.bat          pack them',
        '  Double-click pack_code.bat                     interactive mode (ENTER = repack the last bundle)',
        '  Drag the old context_bundle.txt onto it        repack the same inputs again',
        '  pack_code.bat [options] <file|folder|name|@preset> ...',
        '  Options can also be typed in interactive mode.',
        '',
        '@@OPTIONS@@',
        'OUTPUT',
        '  context_bundle.txt is written to the folder that contains pack_code.bat.',
        '',
        'PROJECT ROOT (used for relative paths and for finding .packcode)',
        '  The nearest parent folder containing .packcode, .git, *.sln or *.uproject.',
        '  If there is none, the common parent folder of the inputs is used.',
        '',
        'OPTIONAL .packcode FILE (in the project root)',
        '  ext+ = json, md               add extensions      (ext = ... replaces the list)',
        '  exclude+ = Tools, Docs        add excluded folders (exclude = ... replaces the list)',
        '  linenumbers = on              on / off',
        '  keepblank = off               on / off',
        '  maxkb = 512',
        '  codepage = 932',
        '  out = context_bundle.txt',
        '  preset.renderer = Source/Renderer | Shaders',
        '',
        'Folders are filtered by extension and by the exclude list.',
        'Files you give explicitly are always packed unless they are binary or too large.'
    )
    foreach ($l in $text) {
        if ($l -eq '@@OPTIONS@@') { Show-Options $false } else { Write-Host $l }
    }
}

function Show-Banner {
    Write-Host '================================================='
    Write-Host "  pack_code $PackVersion - source code packer for AI"
    Write-Host '================================================='
}

function Invoke-Interactive($history) {
    Write-Host ''
    if ($Ctx.Base) {
        Write-Host "Project : $($Ctx.Base)"
        if ($Ctx.BaseOpt.Presets.Count -gt 0) {
            Write-Host ('Presets : ' + ((($Ctx.BaseOpt.Presets.Keys | Sort-Object) | ForEach-Object { '@' + $_ }) -join '  '))
        }
        Write-Host ''
    }
    if ($history.Count -gt 0) {
        Write-Host 'Packs history'
        for ($i = 0; $i -lt $history.Count; $i++) {
            Write-Host "  [$($i + 1)] $($history[$i].Root)"
            Write-Host "      inputs: $($history[$i].Inputs)"
        }
        Write-Host ''
    }
    Write-Host 'HOW TO USE'
    Write-Host '  1. Add files         drag file(s)/folder(s) into this window, all on the same line'
    Write-Host '                       or type file/folder names   (* = anything, e.g. Player*.cpp)'
    Write-Host '  2. Pack them         press ENTER'
    Write-Host '  Options              type -opt, then press ENTER'
    if ($history.Count -gt 0) {
        Write-Host ''
        Write-Host 'Or, instead of adding files:'
        Write-Host '  Repack [number]      type number, then press ENTER'
        Write-Host '  Repack latest [1]    press ENTER'
    }
    Write-Host ''
    # One line = one pack: ENTER always packs what is on the line.
    # Lines that only set options (or show -opt) keep the prompt open.
    while ($true) {
        Write-Host '> ' -NoNewline
        $line = $Host.UI.ReadLine()
        if ($null -eq $line) { break }
        $line = $line.Trim()
        if ($line -eq '') {
            if ($history.Count -gt 0) { return $history[0] }
            break
        }
        if ($line -match '^[1-9]$' -and [int]$line -le $history.Count) {
            return $history[[int]$line - 1]
        }
        if ($history.Count -gt 0 -and $line -eq 'clear') {
            Remove-PackHistory
            $history.Clear()
            continue
        }

        $missBefore = $Ctx.NotFound
        $ok = $true
        # A path with spaces typed without quotes: take the whole line if it exists.
        $whole = $line.Trim('"')
        if ($whole.IndexOf('"') -lt 0 -and (Get-ExistingPath $whole)) {
            Resolve-Token $whole
        } else {
            $names = New-Object System.Collections.Generic.List[string]
            try {
                Read-Options (Split-Args $line) $names $true
            } catch {
                Write-Host "[Error]  $($_.Exception.Message)" -ForegroundColor Red
                $ok = $false
            }
            $flags = $Ctx.Flags
            if ($flags.Help -or $flags.Opt) { Show-Options $true; $flags.Help = $false; $flags.Opt = $false }
            if ($flags.ClearHistory) { Remove-PackHistory; $history.Clear(); $flags.ClearHistory = $false }
            if ($flags.Repack) {
                $flags.Repack = $false
                if ($history.Count -gt 0) { return $history[0] }
                Write-Host '[Error]  There is no previous pack to repeat yet.' -ForegroundColor Red
            }
            if ($ok) { foreach ($t in $names) { Resolve-Token $t } }
        }

        if ((-not $ok) -or $Ctx.NotFound -gt $missBefore) {
            # Never pack an incomplete list: drop this line and ask again.
            $Ctx.Inputs.Clear(); $Ctx.InputSet.Clear(); $Ctx.Bundles.Clear()
            $Ctx.NotFound = $missBefore
            Write-Host '         Nothing was packed. Fix the line and press ENTER.' -ForegroundColor Yellow
            continue
        }
        if ($Ctx.Inputs.Count -gt 0 -or $Ctx.Bundles.Count -gt 0) { break }
    }
    return $null
}

function Invoke-Main {
    $tokens = Split-Args $env:PACK_ARGS
    $cli = $Ctx.Cli
    $flags = $Ctx.Flags
    $raw = New-Object System.Collections.Generic.List[string]
    Read-Options $tokens $raw $false

    if ($flags.Help) { $Ctx.NoPause = $true; Show-Help; return }
    if ($flags.Opt)  { $Ctx.NoPause = $true; Show-Options $false; return }

    Show-Banner
    if ($flags.ClearHistory) {
        Write-Host ''
        Remove-PackHistory
        if ($raw.Count -eq 0) { return }
    }
    $history = Get-History

    # Guess the project before any input is known: the current folder, then
    # the script's own folder, then the last project that was packed.
    $Ctx.Base = Find-Root (Get-Location).Path
    if ((-not $Ctx.Base) -and $env:PACK_SELF) { $Ctx.Base = Find-Root ([IO.Path]::GetDirectoryName($env:PACK_SELF)) }
    if ((-not $Ctx.Base) -and $history.Count -gt 0) { $Ctx.Base = $history[0].Root }
    $Ctx.BaseOpt = Get-Options $Ctx.Base $cli $true

    # The bundle is always written next to this script.
    $outDir = (Get-Location).Path
    if ($env:PACK_SELF) { $outDir = [IO.Path]::GetDirectoryName($env:PACK_SELF) }

    $recipe = $null
    if ($flags.Repack) {
        if ($history.Count -eq 0) { throw 'There is no previous pack to repeat yet.' }
        $recipe = ConvertTo-Recipe $history[0]
    } elseif ($raw.Count -gt 0) {
        Write-Host ''
        foreach ($t in $raw) { Resolve-Token $t }
    } else {
        $rec = Invoke-Interactive $history
        if ($rec) { $recipe = ConvertTo-Recipe $rec }
    }

    if ((-not $recipe) -and $Ctx.Inputs.Count -eq 0 -and $Ctx.Bundles.Count -gt 0) {
        # An old bundle was given: repeat the pack that produced it.
        $b = $Ctx.Bundles[0]
        foreach ($h in $history) {
            if ([string]::Equals($h.Out, $b, $IgnoreCase)) { $recipe = ConvertTo-Recipe $h; break }
        }
        if (-not $recipe) {
            $hdr = Read-BundleHeader $b
            if ($hdr.Inputs.Count -eq 0) { throw 'This bundle has no input record. Drag the files again once.' }
            $recipe = @{ Root = [IO.Path]::GetDirectoryName($b); Inputs = $hdr.Inputs; Cli = $hdr.Cli }
        }
    } elseif ((-not $recipe) -and $Ctx.Bundles.Count -gt 0) {
        Write-Host '[Note]   Bundle files among the inputs were ignored.' -ForegroundColor Yellow
    }

    $root = $null; $cliUse = $cli
    if ($recipe) {
        Write-Host ''
        Write-Host "[Repack] $($recipe.Root)"
        $root = $recipe.Root
        $cliUse = $recipe.Cli
        foreach ($k in @($cli.Keys)) { $cliUse[$k] = $cli[$k] }
        $Ctx.Inputs.Clear(); $Ctx.InputSet.Clear()
        foreach ($rel in $recipe.Inputs) {
            $p = $root
            if ($rel -ne '.') { $p = [IO.Path]::GetFullPath([IO.Path]::Combine($root, $rel.Replace('/', $Sep))) }
            if ([IO.File]::Exists($p) -or [IO.Directory]::Exists($p)) { Add-Input $p }
            else { Write-Host "[Missing] $rel" -ForegroundColor Yellow }
        }
        if ($Ctx.Inputs.Count -eq 0) { throw 'None of the recorded inputs exist anymore. Drag the files again once.' }
    } else {
        if ($Ctx.Inputs.Count -eq 0) {
            Write-Host ''
            Write-Host '[!] No files were packed.' -ForegroundColor Yellow
            if ($Ctx.NotFound -gt 0) { $Ctx.ExitCode = 1 }
            return
        }
        $dirs = foreach ($in in $Ctx.Inputs) {
            if ($in.IsDir) { $in.Path } else { [IO.Path]::GetDirectoryName($in.Path) }
        }
        $common = Get-CommonDir @($dirs)
        if (-not $common) { throw 'The inputs have no common parent folder (different drives?). Pack them separately.' }
        $root = Find-Root $common
        if (-not $root) { $root = $common }
    }

    $opt = Get-Options $root $cliUse $false
    $name = $opt.Out
    if ($flags.Out) { $name = $flags.Out }
    $outPath = [IO.Path]::Combine($outDir, $name)

    $ok = Build-Bundle $root $opt $outPath $cliUse (-not $flags.Clip)
    if (-not $ok) { $Ctx.ExitCode = 1; return }
    $relInputs = foreach ($in in $Ctx.Inputs) { Get-Rel $in.Path $root }
    Save-History $root $outPath ($relInputs -join ' | ') (Format-Cli $cliUse)
    if ($flags.SavePreset) { Save-Preset $root $flags.SavePreset }
}

try {
    Invoke-Main | Out-Host
} catch {
    Write-Host ''
    Write-Host "[Error] $($_.Exception.Message)" -ForegroundColor Red
    $Ctx.ExitCode = 1
}
if (-not $Ctx.NoPause) {
    Write-Host ''
    Write-Host 'Press ENTER to close...' -NoNewline
    [void]$Host.UI.ReadLine()
}
exit $Ctx.ExitCode