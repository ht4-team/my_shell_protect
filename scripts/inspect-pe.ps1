param(
    [Parameter(Mandatory = $true)][string]$Path
)

$fs = [System.IO.File]::OpenRead($Path)
try {
    $br = New-Object System.IO.BinaryReader($fs)
    $fs.Seek(0x3C, [System.IO.SeekOrigin]::Begin) | Out-Null
    $e_lfanew = $br.ReadInt32()

    $fs.Seek($e_lfanew + 4, [System.IO.SeekOrigin]::Begin) | Out-Null
    $machine = $br.ReadUInt16()
    $sectionCount = $br.ReadUInt16()
    $fs.Seek(12, [System.IO.SeekOrigin]::Current) | Out-Null
    $optionalHeaderSize = $br.ReadUInt16()
    $fs.Seek(2, [System.IO.SeekOrigin]::Current) | Out-Null

    $optionalHeaderStart = $e_lfanew + 24
    $fs.Seek($optionalHeaderStart + 16, [System.IO.SeekOrigin]::Begin) | Out-Null
    $oep = $br.ReadUInt32()

    Write-Output ("Machine=0x{0:X4}" -f $machine)
    Write-Output ("Sections={0}" -f $sectionCount)
    Write-Output ("OEP=0x{0:X8}" -f $oep)
    Write-Output "SectionTable:"

    $sectionTableStart = $optionalHeaderStart + $optionalHeaderSize
    $fs.Seek($sectionTableStart, [System.IO.SeekOrigin]::Begin) | Out-Null
    for ($i = 0; $i -lt $sectionCount; $i++) {
        $nameBytes = $br.ReadBytes(8)
        $name = [Text.Encoding]::ASCII.GetString($nameBytes).Trim([char]0)
        $virtualSize = $br.ReadUInt32()
        $virtualAddress = $br.ReadUInt32()
        $sizeOfRawData = $br.ReadUInt32()
        $pointerToRawData = $br.ReadUInt32()
        $fs.Seek(16, [System.IO.SeekOrigin]::Current) | Out-Null
        Write-Output ("{0} VA=0x{1:X8} VS=0x{2:X8} RAW=0x{3:X8} PTR=0x{4:X8}" -f $name, $virtualAddress, $virtualSize, $sizeOfRawData, $pointerToRawData)
    }
}
finally {
    $fs.Close()
}
