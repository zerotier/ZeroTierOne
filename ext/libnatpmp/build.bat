@echo Compiling with MinGW
@SET LIBS=-lws2_32 -liphlpapi

@echo Compile getgateway
gcc -c -Wall -Os -DWIN32 -DSTATICLIB -DENABLE_STRNATPMPERR getgateway.c
gcc -c -Wall -Os -DWIN32 -DSTATICLIB -DENABLE_STRNATPMPERR testgetgateway.c
gcc -o testgetgateway getgateway.o testgetgateway.o %LIBS%
del testgetgateway.o

@echo Compile natpmp-static:
gcc -c -Wall -Os -DWIN32 -DSTATICLIB -DENABLE_STRNATPMPERR getgateway.c
gcc -c -Wall -Os -DWIN32 -DSTATICLIB -DENABLE_STRNATPMPERR natpmp.c
gcc -c -Wall -Os -DWIN32 wingettimeofday.c
ar cr natpmp.a getgateway.o natpmp.o wingettimeofday.o
del getgateway.o natpmp.o
gcc -c -Wall -Os -DWIN32 -DSTATICLIB -DENABLE_STRNATPMPERR natpmpc.c
gcc -o natpmpc-static natpmpc.o natpmp.a %LIBS%
upx --best natpmpc-static.exe
del natpmpc.o

@echo Create natpmp.dll:
gcc -c -Wall -Os -DWIN32 -DENABLE_STRNATPMPERR -DNATPMP_EXPORTS getgateway.c
gcc -c -Wall -Os -DWIN32 -DENABLE_STRNATPMPERR -DNATPMP_EXPORTS natpmp.c
dllwrap -k --driver-name gcc --def natpmp.def --output-def natpmp.dll.def --implib natpmp.lib -o natpmp.dll getgateway.o natpmp.o wingettimeofday.o %LIBS%

@echo Compile natpmp-shared:
gcc -c -Wall -Os -DWIN32 -DENABLE_STRNATPMPERR -DNATPMP_EXPORTS natpmpc.c
gcc -o natpmpc-shared natpmpc.o natpmp.lib -lws2_32
upx --best natpmpc-shared.exe
del *.o
