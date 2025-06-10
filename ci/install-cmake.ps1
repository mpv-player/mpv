param(
    [string]$arch,
    [string]$version = "4.0.2"
)

if ($arch -eq "x64") {
    $arch = "x86_64"
}

$cmake_fullname = "cmake-$version-windows-$arch"

$cmake_url = "https://github.com/Kitware/CMake/releases/download/v$version/$cmake_fullname.zip"

Invoke-WebRequest $cmake_url -OutFile .\$cmake_fullname.zip
Expand-Archive -Path .\$cmake_fullname.zip -DestinationPath .\ -Force
Rename-Item -Path .\$cmake_fullname -NewName cmake
