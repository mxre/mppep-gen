
$PROGRAMFILES = Get-ItemPropertyValue 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion' 'ProgramFilesDir (x86)'

$VC_PATH = "$PROGRAMFILES\Microsoft Visual Studio\2017\BuildTools\VC"

$WK_PATH = Get-ItemPropertyValue 'HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows Kits\Installed Roots' 'KitsRoot10'

$VC_VERSION = Get-ChildItem "$VC_PATH\Tools\MSVC"
$WK_VERSION = (Get-ChildItem 'HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows Kits\Installed Roots').Name
$WK_VERSION = [io.path]::GetFileNameWithoutExtension($WK_VERSION)
$WK_VERSION = "$WK_VERSION.0"

$CLANG_VERSION = Get-ChildItem "$VC_PATH\Tools\ClangC2"

$old_path = $Env:PATH

$Env:BOOST = "$Env:UserProfile\Downloads\boost_1_65_1"

$Env:PATH = "$VC_PATH\Tools\MSVC\$VC_VERSION\bin\Hostx64\x64"
$Env:PATH += ";$VC_PATH\Tools\ClangC2\$CLANG_VERSION\bin\HostX64"
$Env:PATH += ";$WK_PATH\bin\$WK_VERSION\x64"

$Env:INCLUDE = "$WK_PATH\Include\$WK_VERSION\ucrt;$WK_PATH\Include\$WK_VERSION\um;$WK_PATH\Include\$WK_VERSION\shared"
$Env:INCLUDE += ";$VC_PATH\Tools\MSVC\$VC_VERSION\include"
$Env:INCLUDE += ";$Env:BOOST"

$Env:LIB = "$WK_PATH\Lib\$WK_VERSION\ucrt\x64;$WK_PATH\Lib\$WK_VERSION\um\x64"
$Env:LIB += ";$VC_PATH\Tools\MSVC\$VC_VERSION\lib\x64"

if ($args.Length -gt 0 -and $args[0] -eq "debug") {
    $Env:CXXFLAGS = "-D_DEBUG -O0 -g3"
    $Env:LDFLAGS = "-debug:full msvcrtd.lib ucrtd.lib vcruntimed.lib msvcprtd.lib kernel32.lib"
    nmake /f nmake.mk
} else {
    $Env:CXXFLAGS = "-O3"
    $Env:LDFLAGS = "msvcrt.lib ucrt.lib vcruntime.lib msvcprt.lib kernel32.lib"
    nmake /f nmake.mk $args
}

$Env:PATH = $old_path 
