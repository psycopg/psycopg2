@echo on

pip install delvewheel wheel

vcpkg install libpq:x64-windows-release

pipx install .\scripts\build\pg_config_vcpkg_stub\
