/* $Id: testminixml.c,v 1.10 2014/11/17 17:19:13 nanard Exp $
 * MiniUPnP project
 * Website : http://miniupnp.free.fr/ or http://miniupnp.tuxfamily.org/
 * Author : Thomas Bernard.
 * Copyright (c) 2005-2014 Thomas Bernard
 *
 * testminixml.c
 * test program for the "minixml" functions.
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "minixml.h"
#include "igd_desc_parse.h"

/* ---------------------------------------------------------------------- */
void printeltname1(void * d, const char * name, int l)
{
	int i;
	(void)d;
	printf("element ");
	for(i=0;i<l;i++)
		putchar(name[i]);
}
void printeltname2(void * d, const char * name, int l)
{
	int i;
	(void)d;
	putchar('/');
	for(i=0;i<l;i++)
		putchar(name[i]);
	putchar('\n');
}
void printdata(void *d, const char * data, int l)
{
	int i;
	(void)d;
	printf("data : ");
	for(i=0;i<l;i++)
		putchar(data[i]);
	putchar('\n');
}

void burptest(const char * buffer, int bufsize)
{
	struct IGDdatas data;
	struct xmlparser parser;
	/*objet IGDdatas */
	memset(&data, 0, sizeof(struct IGDdatas));
	/* objet xmlparser */
	parser.xmlstart = buffer;
	parser.xmlsize = bufsize;
	parser.data = &data;
	/*parser.starteltfunc = printeltname1;
	parser.endeltfunc = printeltname2;
	parser.datafunc = printdata; */
	parser.starteltfunc = IGDstartelt;
	parser.endeltfunc = IGDendelt;
	parser.datafunc = IGDdata;
	parsexml(&parser);
#ifdef DEBUG
	printIGD(&data);
#endif /* DEBUG */
}

/* ----- main ---- */
#define XML_MAX_SIZE (8192)
int main(int argc, char * * argv)
{
	FILE * f;
	char buffer[XML_MAX_SIZE];
	int bufsize;
	if(argc<2)
	{
		printf("usage:\t%s file.xml\n", argv[0]);
		return 1;
	}
	f = fopen(argv[1], "r");
	if(!f)
	{
		printf("cannot open file %s\n", argv[1]);
		return 1;
	}
	bufsize = (int)fread(buffer, 1, XML_MAX_SIZE, f);
	fclose(f);
	burptest(buffer, bufsize);
	return 0;
}

