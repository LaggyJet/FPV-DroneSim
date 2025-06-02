@echo off

echo Deleting saved folder
rmdir /S /Q Saved

echo Deleting binaries folder
rmdir /S /Q Binaries

echo Deleting intermediate folder
rmdir /S /Q Intermediate

echo Deleting editor folders
rmdir /S /Q .idea
rmdir /S /Q .vs

echo Deleting platforms folder
rmdir /S /Q Platforms

echo Deleting derived data cache
rmdir /S /Q DerivedDataCache

echo Deleting solution
del /F /Q .vsconfig
del /F /Q *.sln
del /F /Q *.DotSettings.user

echo Cleanup complete
PAUSE