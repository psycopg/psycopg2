@echo on

pip install delvewheel wheel

if not defined VCPKG_TARGET_TRIPLET (
    if /I "%PROCESSOR_ARCHITECTURE%" == "ARM64" (
        set "VCPKG_TARGET_TRIPLET=arm64-windows"
    ) else (
        set "VCPKG_TARGET_TRIPLET=x64-windows-release"
    )
)

vcpkg install libpq:%VCPKG_TARGET_TRIPLET%

pipx install .\scripts\build\pg_config_vcpkg_stub\
