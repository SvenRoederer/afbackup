gcc -c -O2 -D_WIN32 -I. client.c
gcc -c -O2 -D_WIN32 -I. util.c
gcc -c -O2 -D_WIN32 -I. utils.c
gcc -c -O2 -D_WIN32 -I. fileutil.c
gcc -c -O2 -D_WIN32 -I. read.c
gcc -c -O2 -D_WIN32 -I. writeout.c
gcc -c -O2 -D_WIN32 -I. x_regex.c
gcc -c -O2 -D_WIN32 -I. crypt.c
gcc -c -O2 -D_WIN32 -I. goptions.c
gcc -o client.exe *.o
strip client.exe
