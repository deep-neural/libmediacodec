

Clone vcpkg global install
This must be clone once and stored global place, so other packages can share libs, includes
```bash
$ git clone https://github.com/microsoft/vcpkg
$ cd vcpkg
```

Config vcpkg on Linux
```bash
$ ./bootstrap-vcpkg.sh
```


Config vcpkg on Windows
```powershell
$ .\bootstrap-vcpkg.bat
```


Install vcpkg packages linux
```bash
$ cd ../
$ ./vcpkg/vcpkg install
```

vcpkg may need system packages 
```
$ apt-get install -y nasm
```






Install vcpkg packages windows
```bash
$ cd ../
$ .\vcpkg/vcpkg install
```


Build On Windows
```bash
$ mkdir build && cd build
$ cmake -G "Visual Studio 17 2022" .. 
$ cmake --build .
```


Build On Linux
```bash
$ mkdir build && cd build
$ cmake .. 
$ cmake --build .
```