#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <assert.h>

#include "hftp.h"
#include "url.h"
#include "utilities.h"

int url_print(struct url_data url_data)
{
	printf("print url data\n");
	if (url_data.scheme)	printf("scheme : %s\n", url_data.scheme);
	if (url_data.username) 	printf("username : %s\n", url_data.username);
	if (url_data.password) 	printf("password : %s\n", url_data.password);
	printf("domain : %s\n",	url_data.domain);
	printf("port : %d\n", 	url_data.port);
	if (url_data.dir)		printf("dir : %s\n", url_data.dir);
	if (url_data.param)		printf("param : %s\n", url_data.param);
	if (url_data.query)		printf("query : %s\n", url_data.query);
	if (url_data.frag)		printf("frag : %s\n", url_data.frag);
	return 0;
}

int url_data_cleanup(struct url_data* data)
{
	if (data->scheme) 	free(data->scheme);
	if (data->username) free(data->username);
	if (data->password) free(data->password);
	if (data->domain) 	free(data->domain);
	if (data->dir) 		free(data->dir);
	if (data->param)	free(data->param);
	if (data->query)	free(data->query);
	if (data->frag)		free(data->frag);

	return 0;
}

int url_to_data(char* url, struct url_data* out)
{ 
	assert(url != NULL);
	assert(out != NULL);

    char* url_cp = cstrdup(url);
	struct url_data data;
	memset(&data, 0, sizeof(struct url_data));

	char* cururl = url_cp;
	char* nextSeparator = strstr(cururl, "://"), origin;

	if (nextSeparator)
	{
		int schemeLen = nextSeparator - cururl;

		if (schemeLen > SCHEMEMAX)
		{
			fprintf(stderr, "hftp.gettest >> scheme length too long : %d\n", schemeLen);
			goto url_data_cleanup;
		}

		origin = *nextSeparator;
		*nextSeparator = '\0';

		data.scheme = cstrdup(cururl);
		*nextSeparator = origin;
		cururl  += schemeLen + 3;
	}

	int session_len;
	nextSeparator = strstr(cururl, "/");

	// after domain
	if (nextSeparator)
	{
		char* suburl = nextSeparator;
 		int suburl_len = strlen(suburl);

		data.dir = cstrdup(suburl);

		session_len = nextSeparator - cururl;
	}
	else
		session_len = strlen(cururl);

	char* account_separator = strstr(cururl, "@");

	// account
	if (account_separator)
	{
		char* colon_separator = strstr(cururl, ":");

		if (colon_separator)
		{
			data.username = cstrndup(cururl, colon_separator - cururl);
			data.password = cstrndup(colon_separator + 1, account_separator - colon_separator - 1);
		}
		else
			data.username = cstrndup(cururl, account_separator - cururl);

		cururl = account_separator + 1;
	}

	// domain
	{
		char* colon_separator = strstr(cururl, ":");

		if (colon_separator)
		{
			data.domain = cstrndup(cururl, colon_separator - cururl);

			char* end = NULL;
			data.port = strtol(colon_separator + 1, &end, 10);
		}
		else
		{
			data.domain = cstrndup(cururl, session_len);
			data.port = 80;
		}
	}

	if (data.domain != NULL)
	{
		memcpy(out, &data, sizeof(struct url_data));
		return 0;
	}
	else
	{
		fprintf(stderr, "hftp.gettest >> domain must exist..\n");
		goto url_data_cleanup;
	}

url_data_cleanup:
    if (url_cp)         free(url_cp);

	if (data.scheme) 	free(data.scheme);
	if (data.username) 	free(data.username);
	if (data.password) 	free(data.password);
	if (data.domain) 	free(data.domain);
	if (data.dir) 		free(data.dir);
	if (data.param)		free(data.param);
	if (data.query)		free(data.query);
	if (data.frag)		free(data.frag);

	return 1;
}


int url_to_words(char* url, char* scheme, char* username, char* password, char* domain, unsigned short* port, char* dir, char* param, char* query, char* frag)
{
	assert(url != NULL);

    char* cur_url = url;
	char* next_separator = strstr(cur_url, "://"), origin;

	if (next_separator)
	{
		int schemeLen = next_separator - cur_url;

		if (schemeLen > SCHEMEMAX)
		{
			hftp_log_err("hftp.err >> scheme length too long : %d\n", schemeLen);
            return 1;
		}

        if (scheme != NULL)
        {
            strncpy(scheme, cur_url, cur_url - next_separator);
            scheme[cur_url - next_separator] = '\0';
        }

		cur_url  += schemeLen + 3;
	}

	int session_len;
	next_separator = strstr(cur_url, "/");

	// after domain
	if (next_separator)
	{
        if (dir != NULL)
        {
            strncpy(dir, next_separator, next_separator - cur_url);
            dir[next_separator - cur_url] = '\0';
        }

		session_len = next_separator - cur_url;
	}
	else
    {
		char* ptr = NULL;
        if (!ptr) ptr = strstr(cur_url, " ");
        if (!ptr) ptr = strstr(cur_url, "\n");
        if (!ptr) ptr = strstr(cur_url, "\r");
        if (!ptr) ptr = strstr(cur_url, "\t");
        if (!ptr) ptr = strstr(cur_url, "\b");
        if (!ptr) ptr = strstr(cur_url, "\0");

        session_len = strlen(url) - (cur_url - url);
    }

	char* account_separator = strstr(cur_url, "@");

	// account
	if (account_separator)
	{
		char* colon_separator = strstr(cur_url, ":");

		if (colon_separator)
		{
            if (username)
            {
                strncpy(username, cur_url, colon_separator - cur_url);
                username[colon_separator - cur_url] = '\0';
            }

            if (password)
            {
                strncpy(password, colon_separator + 1, account_separator - colon_separator - 1);
                password[account_separator - colon_separator - 1] = '\0';
            }
		}
		else
        {
            if (username)
            {
                strncpy(username, cur_url, account_separator - cur_url);
            }
        }

		cur_url = account_separator + 1;
	}

	// domain
	{
		char* colon_separator = strstr(cur_url, ":");

		if (colon_separator)
		{
            if (domain)
            {
                strncpy(domain, cur_url, colon_separator - cur_url);
                domain[colon_separator - cur_url] = '\0';
            }

			char* end = NULL;
        
            if (port)
			    *port = strtol(colon_separator + 1, &end, 10);
		}
		else
		{
            if (domain)
            {
                strncpy(domain, cur_url, session_len);
                domain[session_len] = '\0';
            }
   

            if (port)
                *port = 80;
		}
	}

    return 0;
}

