/**
   Converts input from stdin into an array of binary data for use in C.

   License: Public Domain

   Usage: app VariableName < input > output.c
*/

#include <stdint.h> /* uintXX_t */
#include <inttypes.h> /* PRIuXX macros */
#include <stdio.h>

static char const * appName = 0;

static void usage()
{
    printf("Usage: %s OBJECT_NAME < input > output.c\n\n", appName );
}

int main( int argc, char const ** argv )
{
    appName = argv[0];
    if( (argc != 2) || (argv[1][0] == '-') )
    {
	usage();
	return 1;
    }
    char const * varname = argv[1];
    enum { bufSize = 1024 * 8 };
    unsigned char buf[bufSize];
    size_t rd = 0;
    size_t i = 0;
    size_t flip = 0;

    printf( "static unsigned char %s[] = {\n\t", varname);
    uint32_t size = 0;
    while( 0 != (rd = fread( buf, 1, bufSize, stdin ) ) )
    {
	size += rd;
	for(i = 0; i < rd; ++i )
	{
	    printf( "0x%02x", buf[i] );
	    if( !( (rd < bufSize) && (i == rd-1)) ) putchar(',');
	    if( 16 == ++flip )
	    {
		flip = 0;
		printf("\n\t");
	    }
	    else putchar(' ');
	}
    }
    printf("\n};\n", varname );
    printf("#define %s_LEN %llu\n",varname,(unsigned long long)size);
    //printf( "enum { %s_length = %"PRIu32"%s }; ", varname, size,"UL");
    //printf("enum { %s_length = sizeof(%s) };\n", varname, varname );
    return 0;
}
