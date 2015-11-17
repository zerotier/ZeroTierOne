/* $Id: testigddescparse.c,v 1.10 2015/08/06 09:55:24 nanard Exp $ */
/* Project : miniupnp
 * http://miniupnp.free.fr/
 * Author : Thomas Bernard
 * Copyright (c) 2008-2015 Thomas Bernard
 * This software is subject to the conditions detailed in the
 * LICENCE file provided in this distribution.
 * */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "igd_desc_parse.h"
#include "minixml.h"
#include "miniupnpc.h"

/* count number of differences */
int compare_service(struct IGDdatas_service * s, FILE * f)
{
	int n = 0;
	char line[1024];

	while(fgets(line, sizeof(line), f)) {
		char * value;
		char * equal;
		char * name;
		char * parsedvalue;
		int l;
		l = strlen(line);
		while((l > 0) && ((line[l-1] == '\r') || (line[l-1] == '\n') || (line[l-1] == ' ')))
			line[--l] = '\0';
		if(l == 0)
			break;	/* end on blank line */
		if(line[0] == '#')
			continue;	/* skip comments */
		equal = strchr(line, '=');
		if(equal == NULL) {
			fprintf(stderr, "Warning, line does not contain '=' : %s\n", line);
			continue;
		}
		*equal = '\0';
		name = line;
		while(*name == ' ' || *name == '\t')
			name++;
		l = strlen(name);
		while((l > 0) && (name[l-1] == ' ' || name[l-1] == '\t'))
			name[--l] = '\0';
		value = equal + 1;
		while(*value == ' ' || *value == '\t')
			value++;
		if(strcmp(name, "controlurl") == 0)
			parsedvalue = s->controlurl;
		else if(strcmp(name, "eventsuburl") == 0)
			parsedvalue = s->eventsuburl;
		else if(strcmp(name, "scpdurl") == 0)
			parsedvalue = s->scpdurl;
		else if(strcmp(name, "servicetype") == 0)
			parsedvalue = s->servicetype;
		else {
			fprintf(stderr, "unknown field '%s'\n", name);
			continue;
		}
		if(0 != strcmp(parsedvalue, value)) {
			fprintf(stderr, "difference : '%s' != '%s'\n", parsedvalue, value);
			n++;
		}
	}
	return n;
}

int compare_igd(struct IGDdatas * p, FILE * f)
{
	int n = 0;
	char line[1024];
	struct IGDdatas_service * s;

	while(fgets(line, sizeof(line), f)) {
		char * colon;
		int l = (int)strlen(line);
		while((l > 0) && (line[l-1] == '\r' || (line[l-1] == '\n')))
			line[--l] = '\0';
		if(l == 0 || line[0] == '#')
			continue;	/* skip blank lines and comments */
		colon = strchr(line, ':');
		if(colon == NULL) {
			fprintf(stderr, "Warning, no ':' : %s\n", line);
			continue;
		}
		s = NULL;
		*colon = '\0';
		if(strcmp(line, "CIF") == 0)
			s = &p->CIF;
		else if(strcmp(line, "first") == 0)
			s = &p->first;
		else if(strcmp(line, "second") == 0)
			s = &p->second;
		else if(strcmp(line, "IPv6FC") == 0)
			s = &p->IPv6FC;
		else {
			s = NULL;
			fprintf(stderr, "*** unknown service '%s' ***\n", line);
			n++;
			continue;
		}
		n += compare_service(s, f);
	}
	if(n > 0)
		fprintf(stderr, "*** %d difference%s ***\n", n, (n > 1) ? "s" : "");
	return n;
}

int test_igd_desc_parse(char * buffer, int len, FILE * f)
{
	int n;
	struct IGDdatas igd;
	struct xmlparser parser;
	struct UPNPUrls urls;

	memset(&igd, 0, sizeof(struct IGDdatas));
	memset(&parser, 0, sizeof(struct xmlparser));
	parser.xmlstart = buffer;
	parser.xmlsize = len;
	parser.data = &igd;
	parser.starteltfunc = IGDstartelt;
	parser.endeltfunc = IGDendelt;
	parser.datafunc = IGDdata;
	parsexml(&parser);
#ifdef DEBUG
	printIGD(&igd);
#endif /* DEBUG */
	GetUPNPUrls(&urls, &igd, "http://fake/desc/url/file.xml", 0);
	printf("ipcondescURL='%s'\n", urls.ipcondescURL);
	printf("controlURL='%s'\n", urls.controlURL);
	printf("controlURL_CIF='%s'\n", urls.controlURL_CIF);
	n = f ? compare_igd(&igd, f) : 0;
	FreeUPNPUrls(&urls);
	return n;
}

int main(int argc, char * * argv)
{
	FILE * f;
	char * buffer;
	int len;
	int r;
	if(argc<2) {
		fprintf(stderr, "Usage: %s file.xml [file.values]\n", argv[0]);
		return 1;
	}
	f = fopen(argv[1], "r");
	if(!f) {
		fprintf(stderr, "Cannot open %s for reading.\n", argv[1]);
		return 1;
	}
	fseek(f, 0, SEEK_END);
	len = ftell(f);
	fseek(f, 0, SEEK_SET);
	buffer = malloc(len);
	if(!buffer) {
		fprintf(stderr, "Memory allocation error.\n");
		fclose(f);
		return 1;
	}
	r = (int)fread(buffer, 1, len, f);
	if(r != len) {
		fprintf(stderr, "Failed to read file %s. %d out of %d bytes.\n",
		        argv[1], r, len);
		fclose(f);
		free(buffer);
		return 1;
	}
	fclose(f);
	f = NULL;
	if(argc > 2) {
		f = fopen(argv[2], "r");
		if(!f) {
			fprintf(stderr, "Cannot open %s for reading.\n", argv[2]);
			free(buffer);
			return 1;
		}
	}
	r = test_igd_desc_parse(buffer, len, f);
	free(buffer);
	if(f)
		fclose(f);
	return r;
}

