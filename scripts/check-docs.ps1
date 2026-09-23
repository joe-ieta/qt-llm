[CmdletBinding()]
param(
    [string]$RepoRoot = ''
)

$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($RepoRoot)) {
    $RepoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
} else {
    $RepoRoot = [System.IO.Path]::GetFullPath($RepoRoot)
}

$strictUtf8 = New-Object System.Text.UTF8Encoding($false, $true)
$errors = New-Object System.Collections.Generic.List[string]
$requiredDocuments = @(
    'README.md',
    'AI_RULES.md',
    'docs\README.md',
    'docs\00-project\positioning.md',
    'docs\20-integration\public-api-guide.md',
    'docs\30-development\build-and-test.md',
    'docs\30-development\release-validation.md',
    'docs\50-reference\api-index.md'
)

foreach ($requiredDocument in $requiredDocuments) {
    if (-not (Test-Path -LiteralPath (Join-Path $RepoRoot $requiredDocument))) {
        $errors.Add("Required document is missing: $requiredDocument")
    }
}

$documents = @(
    Get-Item -LiteralPath (Join-Path $RepoRoot 'README.md'), (Join-Path $RepoRoot 'AI_RULES.md')
    Get-ChildItem -LiteralPath (Join-Path $RepoRoot 'docs') -Recurse -File -Filter '*.md' |
        Where-Object { $_.FullName -notmatch '[\\/]docs[\\/]archive[\\/]' }
)

foreach ($document in $documents) {
    $relativeDocument = $document.FullName.Substring($RepoRoot.Length).TrimStart('\', '/')
    try {
        $text = [System.IO.File]::ReadAllText($document.FullName, $strictUtf8)
    } catch {
        $errors.Add("Document is not valid UTF-8: $relativeDocument")
        continue
    }

    if ($text -notmatch '(?m)^#\s+\S') {
        $errors.Add("Document has no level-one title: $relativeDocument")
    }

    $insideFence = $false
    foreach ($line in ($text -split '\r?\n')) {
        $trimmed = $line.TrimStart()
        if ($trimmed.StartsWith('```') -or $trimmed.StartsWith('~~~')) {
            $insideFence = -not $insideFence
            continue
        }
        if ($insideFence) {
            continue
        }

        foreach ($match in [regex]::Matches($line, '\[[^\]]+\]\(([^)]+)\)')) {
            $target = $match.Groups[1].Value.Trim().Trim('<', '>')
            if ($target -match '^(https?://|mailto:|#)') {
                continue
            }
            $pathPart = ($target -split '#', 2)[0]
            if ([string]::IsNullOrWhiteSpace($pathPart)) {
                continue
            }
            try {
                $pathPart = [System.Uri]::UnescapeDataString($pathPart)
                $resolved = [System.IO.Path]::GetFullPath((Join-Path $document.DirectoryName $pathPart))
                if (-not (Test-Path -LiteralPath $resolved)) {
                    $errors.Add("Broken relative link: $relativeDocument -> $target")
                }
            } catch {
                $errors.Add("Invalid relative link: $relativeDocument -> $target")
            }
        }
    }
}

$staleAssertions = @(
    @{ File = 'docs\README.md'; Pattern = '尚未实施' },
    @{ File = 'docs\00-project\llm-foundation-optimization-plan.md'; Pattern = 'QTL-09 进行中' },
    @{ File = 'docs\00-project\qtllm-public-contract-baseline.md'; Pattern = '当前主机未提供本轮构建证据' },
    @{ File = 'docs\30-development\cmake-consumption.md'; Pattern = '有 Qt5/STL 弃用警告' },
    @{ File = 'docs\30-development\request-lifecycle.md'; Pattern = '由 QTL-04 继续处理' }
)
foreach ($assertion in $staleAssertions) {
    $path = Join-Path $RepoRoot $assertion.File
    if (Test-Path -LiteralPath $path) {
        $text = [System.IO.File]::ReadAllText($path, $strictUtf8)
        if ($text.Contains($assertion.Pattern)) {
            $errors.Add("Stale statement in $($assertion.File): $($assertion.Pattern)")
        }
    }
}

$apiIndexPath = Join-Path $RepoRoot 'docs\50-reference\api-index.md'
if (Test-Path -LiteralPath $apiIndexPath) {
    $apiIndex = [System.IO.File]::ReadAllText($apiIndexPath, $strictUtf8)
    foreach ($targetName in @('QtLlm::Core', 'QtLlm::Diagnostics', 'QtLlm::Tools', 'QtLlm::LocalRuntime', 'QtLlm::Conversation', 'QtLlm::QtLlm')) {
        if (-not $apiIndex.Contains($targetName)) {
            $errors.Add("API index does not name public CMake target: $targetName")
        }
    }
}

if ($errors.Count -gt 0) {
    foreach ($errorMessage in $errors) {
        Write-Error $errorMessage
    }
    throw "Documentation check failed with $($errors.Count) error(s)."
}

Write-Host "Documentation check passed: $($documents.Count) active documents, UTF-8, titles, links, status markers, and CMake target index."
