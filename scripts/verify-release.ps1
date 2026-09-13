[CmdletBinding()]
param(
    [string]$CMakePath = 'E:\Qt\Tools\CMake_64\bin\cmake.exe',
    [string]$Qt5Root = 'E:\Qt\5.15.2\msvc2019_64',
    [string]$Qt6Root = 'E:\Qt\6.10.3\msvc2022_64',
    [string]$BuildRoot = '',
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo', 'MinSizeRel')]
    [string]$Configuration = 'Release',
    [string]$Generator = 'Visual Studio 17 2022',
    [string]$Architecture = 'x64',
    [string]$ExpectedVersion = ''
)

$ErrorActionPreference = 'Stop'

$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if ([string]::IsNullOrWhiteSpace($BuildRoot)) {
    $BuildRoot = Join-Path $repoRoot 'build-release-verification'
} elseif (-not [System.IO.Path]::IsPathRooted($BuildRoot)) {
    $BuildRoot = Join-Path $repoRoot $BuildRoot
}
$BuildRoot = [System.IO.Path]::GetFullPath($BuildRoot)
$ctestPath = Join-Path (Split-Path -Parent $CMakePath) 'ctest.exe'
$logRoot = Join-Path $BuildRoot 'logs'
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)

function Assert-PathExists([string]$Path, [string]$Description) {
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "$Description not found: $Path"
    }
}

function Convert-ToCMakePath([string]$Path) {
    return ([System.IO.Path]::GetFullPath($Path)).Replace('\', '/')
}

function Invoke-Checked(
    [string]$Label,
    [string]$File,
    [string[]]$Arguments,
    [switch]$CheckCompilerWarnings
) {
    Write-Host "[RUN ] $Label"
    $previousPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $nativeOutput = & $File @Arguments 2>&1
        $exitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousPreference
    }

    $lines = [string[]]@($nativeOutput | ForEach-Object { $_.ToString() })
    $safeLabel = [regex]::Replace($Label, '[^A-Za-z0-9_.-]+', '_')
    [System.IO.File]::WriteAllLines(
        (Join-Path $logRoot "$safeLabel.log"),
        $lines,
        $utf8NoBom
    )

    if ($exitCode -ne 0) {
        $tail = ($lines | Select-Object -Last 60) -join [Environment]::NewLine
        throw "$Label failed with exit code $exitCode.$([Environment]::NewLine)$tail"
    }

    if ($CheckCompilerWarnings) {
        $warnings = @($lines | Where-Object {
            $_ -match '(?i)\bwarning (?:C\d{4}|LNK\d{4})\b'
        })
        if ($warnings.Count -gt 0) {
            throw "$Label emitted compiler/linker warnings:$([Environment]::NewLine)$($warnings -join [Environment]::NewLine)"
        }
    }

    Write-Host "[PASS] $Label"
    return $lines
}

function Invoke-SourceBuild([string]$MatrixName, [string]$BuildDirectory) {
    $buildArguments = @('--build', $BuildDirectory, '--config', $Configuration)
    try {
        $null = Invoke-Checked "$MatrixName-source-build" $CMakePath $buildArguments -CheckCompilerWarnings
    } catch {
        $message = $_.Exception.Message
        if ($message -notmatch 'LNK1104' -or $message -notmatch 'qtllm_tests\.exe') {
            throw
        }

        Write-Host "[INFO] $MatrixName-source-build hit the known qtllm_tests.exe lock; retrying that target once"
        Start-Sleep -Milliseconds 500
        $null = Invoke-Checked "$MatrixName-source-build-retry" $CMakePath @(
            '--build', $BuildDirectory,
            '--config', $Configuration,
            '--target', 'qtllm_tests'
        ) -CheckCompilerWarnings
        $null = Invoke-Checked "$MatrixName-source-build-confirm" $CMakePath $buildArguments -CheckCompilerWarnings
    }
}

function Assert-InstalledPackage([string]$InstallRoot) {
    $packageRoot = Join-Path $InstallRoot 'lib\cmake\QtLlm'
    $requiredFiles = @(
        'QtLlmConfig.cmake',
        'QtLlmConfigVersion.cmake',
        'QtLlmCoreTargets.cmake',
        'QtLlmDiagnosticsTargets.cmake',
        'QtLlmToolsTargets.cmake',
        'QtLlmLocalRuntimeTargets.cmake',
        'QtLlmConversationTargets.cmake',
        'QtLlmTargets.cmake'
    )
    foreach ($requiredFile in $requiredFiles) {
        Assert-PathExists (Join-Path $packageRoot $requiredFile) 'Installed package metadata'
    }
    Assert-PathExists (Join-Path $InstallRoot 'include\qtllm_global.h') 'Installed export header'
    return $packageRoot
}

function Read-ConfiguredVersion([string]$BuildDirectory) {
    $cachePath = Join-Path $BuildDirectory 'CMakeCache.txt'
    Assert-PathExists $cachePath 'CMake cache'
    $cache = [System.IO.File]::ReadAllText($cachePath, [System.Text.Encoding]::UTF8)
    $match = [regex]::Match($cache, '(?m)^CMAKE_PROJECT_VERSION:STATIC=([^\r\n]+)\r?$')
    if (-not $match.Success) {
        throw "CMAKE_PROJECT_VERSION is missing from $cachePath"
    }
    return $match.Groups[1].Value.Trim()
}

function Find-Executable([string]$BuildDirectory, [string]$Name) {
    $executable = Get-ChildItem -LiteralPath $BuildDirectory -Recurse -File -Filter $Name |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1
    if (-not $executable) {
        throw "Executable not found after build: $Name"
    }
    return $executable.FullName
}

Assert-PathExists $repoRoot 'Repository root'
Assert-PathExists $CMakePath 'CMake executable'
Assert-PathExists $ctestPath 'CTest executable'
Assert-PathExists (Join-Path $Qt5Root 'bin\Qt5Core.dll') 'Qt5 runtime'
Assert-PathExists (Join-Path $Qt6Root 'bin\Qt6Core.dll') 'Qt6 runtime'
[System.IO.Directory]::CreateDirectory($BuildRoot) | Out-Null
[System.IO.Directory]::CreateDirectory($logRoot) | Out-Null

$qtVariants = @(
    @{ Name = 'qt6'; Root = [System.IO.Path]::GetFullPath($Qt6Root) },
    @{ Name = 'qt5'; Root = [System.IO.Path]::GetFullPath($Qt5Root) }
)
$libraryTypes = @('STATIC', 'SHARED')
$packageConsumers = @(
    @{ Name = 'core'; Source = 'tests\consumers\package-core'; Exe = 'qtllm_core_package_consumer.exe' },
    @{ Name = 'components'; Source = 'tests\consumers\package-components'; Exe = 'qtllm_optional_components_consumer.exe' },
    @{ Name = 'conversation'; Source = 'tests\consumers\package-conversation'; Exe = 'qtllm_conversation_consumer.exe' },
    @{ Name = 'aggregate'; Source = 'tests\consumers\package'; Exe = 'qtllm_package_consumer.exe' }
)

$sourceMatrixCount = 0
$packageConsumerCount = 0
$subdirectoryConsumerCount = 0
$resolvedVersion = ''
$originalPath = $env:PATH

try {
    foreach ($qt in $qtVariants) {
        $qtRoot = $qt.Root
        $qtPrefix = Convert-ToCMakePath $qtRoot
        foreach ($libraryType in $libraryTypes) {
            $matrixName = "$($qt.Name)-$($libraryType.ToLowerInvariant())"
            $buildDirectory = Join-Path $BuildRoot $matrixName
            $installRoot = Join-Path $BuildRoot "install-$matrixName"
            $env:PATH = "$(Join-Path $qtRoot 'bin');$originalPath"

            $configureArguments = @(
                '-S', $repoRoot,
                '-B', $buildDirectory,
                '-G', $Generator,
                '-A', $Architecture,
                "-DCMAKE_PREFIX_PATH=$qtPrefix",
                "-DCMAKE_INSTALL_PREFIX=$(Convert-ToCMakePath $installRoot)",
                "-DQTLLM_LIBRARY_TYPE=$libraryType",
                '-DQTLLM_BUILD_APPS=ON',
                '-DQTLLM_BUILD_TESTS=ON',
                '-DQTLLM_ENABLE_INSTALL=ON'
            )
            $null = Invoke-Checked "$matrixName-source-configure" $CMakePath $configureArguments
            Invoke-SourceBuild $matrixName $buildDirectory

            $testOutput = Invoke-Checked "$matrixName-source-test" $ctestPath @(
                '--test-dir', $buildDirectory,
                '-C', $Configuration,
                '--output-on-failure'
            )
            if (-not ($testOutput -match '100% tests passed, 0 tests failed out of 6')) {
                throw "$matrixName did not report the required 6/6 source tests"
            }
            $sourceMatrixCount++

            $null = Invoke-Checked "$matrixName-source-install" $CMakePath @(
                '--install', $buildDirectory, '--config', $Configuration
            )

            $configuredVersion = Read-ConfiguredVersion $buildDirectory
            if ([string]::IsNullOrWhiteSpace($resolvedVersion)) {
                $resolvedVersion = $configuredVersion
            } elseif ($configuredVersion -ne $resolvedVersion) {
                throw "Project version differs across matrix builds: $configuredVersion vs $resolvedVersion"
            }
            if (-not [string]::IsNullOrWhiteSpace($ExpectedVersion) -and
                $configuredVersion -ne $ExpectedVersion) {
                throw "Expected version $ExpectedVersion but configured version is $configuredVersion"
            }

            $packageRoot = Assert-InstalledPackage $installRoot
            $runtimePath = "$(Join-Path $installRoot 'bin');$(Join-Path $qtRoot 'bin');$originalPath"

            foreach ($consumer in $packageConsumers) {
                $consumerBuild = Join-Path $BuildRoot "consumer-$matrixName-$($consumer.Name)"
                $consumerArguments = @(
                    '-S', (Join-Path $repoRoot $consumer.Source),
                    '-B', $consumerBuild,
                    '-G', $Generator,
                    '-A', $Architecture,
                    "-DCMAKE_PREFIX_PATH=$qtPrefix",
                    "-DQtLlm_DIR=$(Convert-ToCMakePath $packageRoot)"
                )
                if ($consumer.Name -eq 'aggregate') {
                    $consumerArguments += "-DQTLLM_EXPECTED_VERSION=$configuredVersion"
                }
                $null = Invoke-Checked "$matrixName-$($consumer.Name)-configure" $CMakePath $consumerArguments
                $null = Invoke-Checked "$matrixName-$($consumer.Name)-build" $CMakePath @(
                    '--build', $consumerBuild, '--config', $Configuration
                ) -CheckCompilerWarnings
                $env:PATH = $runtimePath
                $consumerExecutable = Find-Executable $consumerBuild $consumer.Exe
                $null = Invoke-Checked "$matrixName-$($consumer.Name)-run" $consumerExecutable @()
                $packageConsumerCount++
                $env:PATH = "$(Join-Path $qtRoot 'bin');$originalPath"
            }

            $subdirectoryBuild = Join-Path $BuildRoot "consumer-$matrixName-subdirectory"
            $null = Invoke-Checked "$matrixName-subdirectory-configure" $CMakePath @(
                '-S', (Join-Path $repoRoot 'tests\consumers\subdirectory'),
                '-B', $subdirectoryBuild,
                '-G', $Generator,
                '-A', $Architecture,
                "-DCMAKE_PREFIX_PATH=$qtPrefix",
                "-DQTLLM_SOURCE_DIR=$(Convert-ToCMakePath $repoRoot)",
                "-DQTLLM_LIBRARY_TYPE=$libraryType",
                '-DQTLLM_BUILD_APPS=OFF',
                '-DQTLLM_BUILD_TESTS=OFF',
                '-DQTLLM_ENABLE_INSTALL=OFF'
            )
            $null = Invoke-Checked "$matrixName-subdirectory-build" $CMakePath @(
                '--build', $subdirectoryBuild, '--config', $Configuration
            ) -CheckCompilerWarnings
            $env:PATH = "$(Join-Path $subdirectoryBuild $Configuration);$(Join-Path $qtRoot 'bin');$originalPath"
            $subdirectoryExecutable = Find-Executable $subdirectoryBuild 'qtllm_subdirectory_consumer.exe'
            $null = Invoke-Checked "$matrixName-subdirectory-run" $subdirectoryExecutable @()
            $subdirectoryConsumerCount++
        }
    }
} finally {
    $env:PATH = $originalPath
}

Write-Host "[PASS] release verification completed"
Write-Host "       version: $resolvedVersion"
Write-Host "       source matrices: $sourceMatrixCount/4"
Write-Host "       installed package consumers: $packageConsumerCount/16"
Write-Host "       add_subdirectory consumers: $subdirectoryConsumerCount/4"
Write-Host "       compiler/linker warning gate: passed"
Write-Host "       logs: $logRoot"
