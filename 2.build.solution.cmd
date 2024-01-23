set CurPath=%cd%

MSBuild.exe .\build\extio2tcp.sln -t:Build -p:Configuration=Debug

if %errorlevel% neq 0 exit /b %errorlevel%

MSBuild.exe .\build\extio2tcp.sln -t:Build -p:Configuration=RelWithDebInfo

if %errorlevel% neq 0 exit /b %errorlevel%

cd %CurPath%
