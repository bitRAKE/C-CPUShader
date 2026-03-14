param(
    [string]$VulkanSdk = $env:VULKAN_SDK
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($VulkanSdk)) {
    throw "VULKAN_SDK is not set."
}

$glslang = Join-Path $VulkanSdk "Bin\glslangValidator.exe"
if (-not (Test-Path $glslang)) {
    throw "glslangValidator.exe was not found under $VulkanSdk\Bin."
}

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$vertPath = Join-Path $scriptDir "present_conversion.vert"
$fragPath = Join-Path $scriptDir "present_conversion.frag"
$vertSpvPath = Join-Path $scriptDir "present_conversion.vert.spv"
$fragSpvPath = Join-Path $scriptDir "present_conversion.frag.spv"
$headerPath = Join-Path $scriptDir "present_conversion_spv.h"

& $glslang -V --target-env vulkan1.0 -S vert -o $vertSpvPath $vertPath | Out-Null
& $glslang -V --target-env vulkan1.0 -S frag -o $fragSpvPath $fragPath | Out-Null

function Convert-SpvToArrayText {
    param(
        [string]$BinaryPath,
        [string]$ArrayName
    )

    [byte[]]$bytes = [System.IO.File]::ReadAllBytes($BinaryPath)
    if (($bytes.Length % 4) -ne 0) {
        throw "SPIR-V file '$BinaryPath' does not have a 4-byte aligned size."
    }

    $values = New-Object System.Collections.Generic.List[string]
    for ($index = 0; $index -lt $bytes.Length; $index += 4) {
        $value = [System.BitConverter]::ToUInt32($bytes, $index)
        $values.Add(("0x{0:X8}u" -f $value))
    }

    $joined = ($values -join ", ")
    return "static const uint $ArrayName" + "[] = { $joined };`r`n" +
           "static const uint ${ArrayName}_count = (uint)(sizeof($ArrayName) / sizeof($ArrayName[0]));`r`n"
}

$headerText = @"
#pragma once

#include "../defines.h"

$(Convert-SpvToArrayText -BinaryPath $vertSpvPath -ArrayName "g_vk_present_vert_spv")
$(Convert-SpvToArrayText -BinaryPath $fragSpvPath -ArrayName "g_vk_present_frag_spv")
"@

[System.IO.File]::WriteAllText($headerPath, $headerText, [System.Text.Encoding]::ASCII)
