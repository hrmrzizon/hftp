#pragma once

#define SCHEMEMAX 32

struct url_data
{
	// -://
	char* 	scheme;
	// ?://-:
	char* 	username;
	// ?://?:-
	char* 	password;
	// ?://?:?@-
	char* 	domain;
	// ?://?:?@?:-
	int 	port;
	// ?://?:?@?:?/-
	char* 	dir;
	// ?://?:?@?:?/?;-
	char* 	param;
	// ?://?:?@?:?/?;??-
	char* 	query;
	// ?://?:?@?:?/?;???#-
	char*	frag;
};

int url_print(struct url_data url_data);
int url_data_cleanup(struct url_data* data);
int url_to_data(char* url, struct url_data* out);

int url_to_words(char* url, char* scheme, char* username, char* password, char* domain, unsigned short* port, char* dir, char* param, char* query, char* frag);

