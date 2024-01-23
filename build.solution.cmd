set CurPath=%cd%

echo "CurPath: %CurPath%"
::cd extio2tcp
::cmake -DCMAKE_SYSTEM_VERSION=10.0.19041.0 -S ./ -B ./out -G "Visual Studio 17 2022" -A Win32 --debug-find
cmake -S ./ -B ./out -G "Visual Studio 17 2022" -A Win32

cd %CurPath%
