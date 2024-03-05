This folder contains the Windows driver code, Windows-specific service code, and the Microsoft Visual Studio projects and "solution" for doing Windows builds.

This code may also build with MinGW but this hasn't been tested.

## Building

```
MSBuild ZeroTierOne.sln /property:Configuration=Release /property:Platform=ARM64
```

(substitute e.g. `Debug`, `x64`, etc. as needed above, or use Visual Studio to select a target)

## Testing

PowerShell admin session #1 inside `<PROJECT_DIR>\windows\Build\<ARCH>\<TARGET>`:

```
PS C:\Users\<USER>\ZeroTierOne\windows\Build\ARM64\Release> .\zerotier-one_arm64.exe -p9994 -C C:\<SOME_TEMP_DIR>\
Starting Control Plane...
Starting V6 Control Plane...                                             
```

Session #2:
```
PS C:\Users\<USER>\ZeroTierOne\windows\Build\ARM64\Release> zerotier-cli.bat -p9994 -D"\Users\rcode\scratch\zt1_home\" info
info c3e46fa070 1.12.2 ONLINE
PS C:\Users\<USER>\code\ZeroTierOne\windows\Build\ARM64\Release> zerotier-cli.bat -p9994 -D"\Users\<SOME_TEMP_DIR>\scratch\zt1_home\" join ebe7fbd4458bde97
200 join OK                                                                                                             PS C:\Users\<USER>\code\ZeroTierOne\windows\Build\ARM64\Release>   
```
