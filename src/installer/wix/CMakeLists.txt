##
## WiX MSI Installer Build
## Produces: GlobalVSTHost-<VERSION>-x64.msi
##
## Note: This is a post-build step that runs AFTER the main targets are built.
## Manual build command:
##   wix build -arch x64 -d "JyGlobalVSTExe=<path>" -d "AudioEngineDll=<path>"
##     -d "ProductVersion=<version>" -out GlobalVSTHost-<version>-x64.msi
##     -ext WixUIExtension -ext WixUtilExtension src/installer/wix/product.wxs
##

find_program(WIX_TOOL wix)

if(NOT WIX_TOOL)
    message(STATUS "WiX Toolset not found in PATH.")
    message(STATUS "To use: install WiX Toolset v4+ via dotnet tool install --global WiX")
    message(STATUS "or: https://github.com/wixtoolset/wix4/releases")
    message(STATUS "MSI builds will be skipped.")
    return()
endif()

message(STATUS "WiX Toolset found: ${WIX_TOOL}")

# For manual MSI builds, use the paths below:
# EXE: build/src/tray-app/jyglobalvst_tray_artefacts/Release/JyGlobalVST.exe
# DLL: build/lib/Release/jyglobalvst_audio_engine.dll
# CMD: wix build -arch x64 -d "JyGlobalVSTExe=..." -d "AudioEngineDll=..."
#      -d "ProductVersion=${PROJECT_VERSION}" -out build/GlobalVSTHost-${PROJECT_VERSION}-x64.msi
#      -ext WixUIExtension -ext WixUtilExtension src/installer/wix/product.wxs

message(STATUS "To build MSI after Release binaries are ready:")
message(STATUS "  wix build -arch x64 \\")
message(STATUS "    -d \"JyGlobalVSTExe=build/src/tray-app/jyglobalvst_tray_artefacts/Release/JyGlobalVST.exe\" \\")
message(STATUS "    -d \"AudioEngineDll=build/lib/Release/jyglobalvst_audio_engine.dll\" \\")
message(STATUS "    -d \"ProductVersion=${PROJECT_VERSION}\" \\")
message(STATUS "    -out build/GlobalVSTHost-${PROJECT_VERSION}-x64.msi \\")
message(STATUS "    -ext WixUIExtension -ext WixUtilExtension \\")
message(STATUS "    src/installer/wix/product.wxs")
