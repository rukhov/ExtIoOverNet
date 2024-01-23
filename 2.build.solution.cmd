set CurPath=%cd%

echo "CurPath: %CurPath%"

MSBuild.exe .\build\extio2tcp.sln -t:Build -p:Configuration=Debug
MSBuild.exe .\build\extio2tcp.sln -t:Build -p:Configuration=RelWithDebInfo

cd %CurPath%
