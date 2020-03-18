#include "list_ui.h"
#include "system_log.h"
#include "data_source.h"
#include "lang_resource.h"
#include "request_parser.h"
#include "session_client.h"
#include "mail_func.h"
#include "util.h"
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <iconv.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define HTML_COMMON_1	\
"<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0 Transitional//EN\">\n\
<HTML><HEAD><TITLE>"

/* fill html title here */

#define HTML_COMMON_2	\
"</TITLE><LINK href=\"../data/css/result.css\" type=text/css rel=stylesheet>\n\
<META http-equiv=Content-Type content=\"text/html; charset=utf-8\">\n\
<META content=\"MSHTML 6.00.2900.2963\" name=GENERATOR></HEAD>\n\
<BODY bottomMargin=0 leftMargin=0 topMargin=0 rightMargin=0\n\
marginheight=\"0\" marginwidth=\"0\"><CENTER>\n\
<TABLE cellSpacing=0 cellPadding=0 width=\"100%\" border=0>\n\
<TBODY><TR><TD noWrap align=middle background=\"../data/picture/di1.gif\"\n\
height=55><SPAN class=ReportTitle> "

/* fill list title here */

#define HTML_COMMON_4	\
"</SPAN></TD><TD vAlign=bottom noWrap width=\"22%\"\n\
background=\"../data/picture/di1.gif\"><A href=\""

/* fill logo url link here */

#define HTML_MAIN_5	\
"\" target=_blank><IMG height=48 src=\"../data/picture/logo_bb.gif\"\n\
width=195 align=right border=0></A></TD></TR></TBODY></TABLE><BR><BR>\n\
<SCRIPT language=\"JavaScript\">\n\
function DeleteItem(room) {location.href='%s?domain=%s&session=%s&action=remove&room=' + room;}\n\
function EditItem(room) {location.href='%s?domain=%s&session=%s&action=list-permission&room=' + room;}\n\
</SCRIPT><FORM class=SearchForm name=opeform method=get action="

#define HTML_MAIN_6	\
" ><TABLE border=0><INPUT type=hidden value=%s name=domain />\n\
<INPUT type=hidden value=%s name=session />\n\
<INPUT type=hidden value=add name=action />\n\
<TR><TD></TD><TD>%s:</TD><TD><INPUT type=text value=\"\" tabindex=1 name=room /></TD></TR>\n\
<TR><TD></TD><TD></TD><TD><INPUT type=submit tabindex=4 value=\"%s\" onclick=\"\n\
if (0 == opeform.room.value.length) {return false;} return true;\" /></TD></TR></TABLE></FORM>\n\
<TABLE cellSpacing=0 cellPadding=0 width=\"90%\"\n\
border=0><TBODY><TR><TD background=\"../data/picture/di2.gif\">\n\
<IMG height=30 src=\"../data/picture/kl.gif\" width=3></TD>\n\
<TD class=TableTitle noWrap align=middle background=\"../data/picture/di2.gif\">"

/* fill list table title here */

#define HTML_MAIN_7	\
"</TD><TD align=right background=\"../data/picture/di2.gif\"><IMG height=30\n\
src=\"../data/picture/kr.gif\" width=3></TD></TR><TR bgColor=#bfbfbf>\n\
<TD colSpan=5><TABLE cellSpacing=1 cellPadding=2 width=\"100%\" border=0>\n\
<TBODY>"

#define HTML_MAIN_8	\
"</TBODY></TABLE></TD></TR></TBODY></TABLE><BR><BR></CENTER></BODY></HTML>"

#define HTML_PERMISSION_5	\
"\" target=_blank><IMG height=48 src=\"../data/picture/logo_bb.gif\"\n\
width=195 align=right border=0></A></TD></TR></TBODY></TABLE><BR><BR>\n\
<TABLE cellSpacing=1 cellPadding=1 width=\"90%\" border=0>\n\
<TBODY><TR><TD align=right><A href=\"%s?domain=%s&session=%s\">%s</A>\n\
</TD></TR></TBODY></TABLE><SCRIPT language=\"JavaScript\">\n\
function DeleteItem(username) {location.href='%s?domain=%s&session=%s\
&action=remove-permission&room=%s&username=' + username;}\n\
</SCRIPT><FORM class=SearchForm name=opeform method=get action="

#define HTML_PERMISSION_6 \
" ><TABLE border=0><INPUT type=hidden value=%s name=domain />\n\
<INPUT type=hidden value=%s name=session />\n\
<INPUT type=hidden value='%s' name=room />\n\
<INPUT type=hidden value=add-permission name=action />\n\
<TR><TD></TD><TD>%s:</TD><TD><INPUT type=text value=\"\" tabindex=1 \n\
size=30 name=username /><INPUT style=\"display:none\">\n\
</TD></TR><TR><TD></TD><TD></TD><TD><INPUT type=submit tabindex=2 \n\
value=\"  %s  \" /></TD></TR></TABLE></FORM>\n\
<TABLE cellSpacing=0 cellPadding=0 width=\"90%\"\n\
border=0><TBODY><TR><TD background=\"../data/picture/di2.gif\">\n\
<IMG height=30 src=\"../data/picture/kl.gif\" width=3></TD>\n\
<TD class=TableTitle noWrap align=middle background=\"../data/picture/di2.gif\">"

/* fill table title here */

#define HTML_PERMISSION_7 \
"</TD><TD align=right background=\"../data/picture/di2.gif\"><IMG height=30\n\
src=\"../data/picture/kr.gif\" width=3></TD></TR><TR bgColor=#bfbfbf>\n\
<TD colSpan=5><TABLE cellSpacing=1 cellPadding=2 width=\"100%\" border=0>\n\
<TBODY>"

#define HTML_PERMISSION_8 \
"</TBODY></TABLE></TD></TR></TBODY></TABLE><BR><BR></CENTER></BODY></HTML>"

#define HTML_ERROR_5    \
"\" target=_blank><IMG height=48 src=\"../data/picture/logo_bb.gif\"\n\
width=195 align=right border=0></A></TD></TR></TBODY></TABLE><BR><BR>\n\
<P align=right><A href=domain_main target=_parent>%s</A>&nbsp;&nbsp;&nbsp;\n\
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp\n\
</P><BR><BR>%s</CENTER></BODY></HTML>"

#define HTML_RESULT_5    \
"\" target=_blank><IMG height=48 src=\"../data/picture/logo_bb.gif\"\n\
width=195 align=right border=0></A></TD></TR></TBODY></TABLE><BR><BR>\n\
<BR><BR><BR><BR>%s</CENTER></BODY></HTML>"

#define HTML_TBITEM_FIRST   \
"<TR class=SolidRow><TD>&nbsp;%s&nbsp;</TD><TD>&nbsp;%s&nbsp;</TD></TR>\n"

#define HTML_TBITEM_NORMAL	\
"<TR class=ItemRow><TD>&nbsp;%s&nbsp;</TD> \
<TD>&nbsp;<A href=\"javascript:EditItem('%s')\">%s</A>&nbsp;|&nbsp;\
<A href=\"javascript:DeleteItem('%s')\">%s</A>&nbsp;</TD></TR>\n"

#define HTML_USERITEM_FIRST   \
"<TR class=SolidRow><TD>&nbsp;%s&nbsp;</TD><TD>&nbsp;%s&nbsp;</TD></TR>\n"

#define HTML_USERITEM_NORMAL	\
"<TR class=ItemRow><TD>&nbsp;%s&nbsp;</TD><TD>&nbsp;\
<A href=\"javascript:DeleteItem('%s')\">%s</A>&nbsp;</TD></TR>\n"


static void list_ui_error_html(const char *error_string);

static void list_ui_main_html(const char *domain, const char *session);

static void list_ui_permission_html(const char *domain,
	const char *session, const char *room_name);

static BOOL list_ui_get_self(char *url_buff, int length);

static char g_logo_link[1024];
static char g_resource_path[256];
static LANG_RESOURCE *g_lang_resource;

void list_ui_init(const char *url_link, const char *resource_path)
{
	strcpy(g_logo_link, url_link);
	strcpy(g_resource_path, resource_path);
}

int list_ui_run()
{
	char *query;
	char *request;
	char *language;
	const char *room;
	const char *action;
	const char *domain;
	const char *session;
	const char *username;
	REQUEST_PARSER *pparser;

	language = getenv("HTTP_ACCEPT_LANGUAGE");
	if (NULL == language) {
		list_ui_error_html(NULL);
		return 0;
	}
	g_lang_resource = lang_resource_init(g_resource_path);
	if (NULL == g_lang_resource) {
		system_log_info("[list_ui]: fail to init language resource");
		return -1;
	}
	request = getenv("REQUEST_METHOD");
	if (NULL == request) {
		system_log_info("[list_ui]: fail to get REQUEST_METHOD"
			" environment!");
		return -2;
	}
	if (0 == strcmp(request, "POST")) {
		list_ui_error_html(lang_resource_get(
			g_lang_resource, "ERROR_REQUEST", language));
		return 0;
	} else if (0 == strcmp(request, "GET")) {
		query = getenv("QUERY_STRING");
		if (NULL == query) {
			system_log_info("[list_ui]: fail to get QUERY_STRING "
				"environment!");
			list_ui_error_html(lang_resource_get(
				g_lang_resource, "ERROR_REQUEST", language));
			return 0;
		}
		
		pparser = request_parser_init(query);
		if (NULL == pparser) {
			system_log_info("[list_ui]: fail to init request_parser");
			list_ui_error_html(lang_resource_get(
				g_lang_resource, "ERROR_REQUEST", language));
			return 0;
		}
		
		domain = request_parser_get(pparser, "domain");
		if (NULL == domain) {
			system_log_info("[list_ui]: query string of GET format error");
			list_ui_error_html(lang_resource_get(
				g_lang_resource, "ERROR_REQUEST", language));
			return 0;
		}
		
		session = request_parser_get(pparser, "session");
		if (NULL == session) {
			system_log_info("[list_ui]: query string of GET format error");
			list_ui_error_html(lang_resource_get(
				 g_lang_resource, "ERROR_REQUEST", language));
			return 0;
		}
		if (FALSE == session_client_check(domain, session)) {
			list_ui_error_html(lang_resource_get(
				g_lang_resource, "ERROR_SESSION", language));
			return 0;
		}
		
		action = request_parser_get(pparser, "action");
		if (NULL == action) {
			list_ui_main_html(domain, session);
			return 0;
		}
		
		room = request_parser_get(pparser, "room");
		if (NULL == room) {
			system_log_info("[list_ui]: query string of GET format error");
			list_ui_error_html(lang_resource_get(
				g_lang_resource, "ERROR_REQUEST", language));
			return 0;
		}
		if (0 == strcasecmp(action, "add")) {
			if (FALSE == data_source_add_room(domain, room)) {
				list_ui_error_html(lang_resource_get(
					g_lang_resource, "ERROR_REQUEST", language));
				return 0;
			}
			list_ui_main_html(domain, session);
			return 0;
		} else if (0 == strcasecmp(action, "remove")) {
			data_source_del_room(domain, room);
			list_ui_main_html(domain, session);
			return 0;
		} else if (0 == strcasecmp(action, "list-permission")) {
			list_ui_permission_html(domain, session, room);
			return 0;
		} else if (0 == strcasecmp(action, "add-permission")) {
			username = request_parser_get(pparser, "username");
			if (NULL == username) {
				list_ui_error_html(lang_resource_get(
					g_lang_resource, "ERROR_REQUEST", language));
				return 0;
			}
			if (FALSE == data_source_add_permission(domain, room, username)) {
				list_ui_error_html(lang_resource_get(
					g_lang_resource, "ERROR_REQUEST", language));
				return 0;
			}
			list_ui_permission_html(domain, session, room);
			return 0;
		} else if (0 == strcasecmp(action, "remove-permission")) {
			username = request_parser_get(pparser, "username");
			if (NULL == username) {
				list_ui_error_html(lang_resource_get(
					g_lang_resource, "ERROR_REQUEST", language));
				return 0;
			}
			data_source_del_permission(domain, room, username);
			list_ui_permission_html(domain, session, room);
			return 0;
		} else {
			system_log_info("[list_ui]: query string of GET format error");
			list_ui_error_html(lang_resource_get(
				g_lang_resource, "ERROR_REQUEST", language));
			return 0;
		}
	} else {
		system_log_info("[list_ui]: unrecognized REQUEST_METHOD \"%s\"!", request);
		list_ui_error_html(lang_resource_get(g_lang_resource,"ERROR_REQUEST", language));
		return 0;
	}
}

int list_ui_stop()
{
	if (NULL != g_lang_resource) {
		lang_resource_free(g_lang_resource);
		g_lang_resource = NULL;
	}
	return 0;
}

void list_ui_free()
{
	/* do nothing */
}

static BOOL list_ui_get_self(char *url_buff, int length)
{
	char *host;
	char *https;
	char *script;
	
	host = getenv("HTTP_HOST");
	script = getenv("SCRIPT_NAME");
	https = getenv("HTTPS");
	if (NULL == host || NULL == script) {
		system_log_info("[ui_main]: fail to get HTTP_HOST or SCRIPT_NAME "
				"environment!");
		return FALSE;
	}
	if (NULL == https || 0 != strcasecmp(https, "ON")) {
		snprintf(url_buff, length, "http://%s%s", host, script);
	} else {
		snprintf(url_buff, length, "https://%s%s", host, script);
	}
	return TRUE;
}

static void list_ui_error_html(const char *error_string)
{
	char *language;
	
	if (NULL == error_string) {
		error_string = "fatal error!!!";
	}
	
	language = getenv("HTTP_ACCEPT_LANGUAGE");
	if (NULL == language) {
		language = "en";
	}
	printf("Content-Type:text/html;charset=utf-8\n\n");
	printf(HTML_COMMON_1);
	printf(lang_resource_get(g_lang_resource,"ERROR_HTML_TITLE", language));
	printf(HTML_COMMON_2);
	printf(lang_resource_get(g_lang_resource,"ERROR_HTML_TITLE", language));
	printf(HTML_COMMON_4);
	printf(g_logo_link);
	printf(HTML_ERROR_5, lang_resource_get(g_lang_resource,"BACK_LABEL", language),
		error_string);
}

static void list_ui_main_html(const char *domain, const char *session)
{
	char *language;
	char *proom_name;
	char url_buff[1024];
	DATA_COLLECT *pcollect;
	
	pcollect = data_source_collect_init();
	if (NULL == pcollect) {
		list_ui_error_html(lang_resource_get(
			g_lang_resource, "ERROR_INTERNAL", language));
		return;
	}
	language = getenv("HTTP_ACCEPT_LANGUAGE");
	if (NULL == language) {
		language = "en";
	}
	if (FALSE == list_ui_get_self(url_buff, 1024)) {
		list_ui_error_html(lang_resource_get(
			g_lang_resource, "ERROR_INTERNAL", language));
		return;
	}
	
	if (FALSE == data_source_list_rooms(domain, pcollect)) {
		list_ui_error_html(lang_resource_get(
			g_lang_resource, "ERROR_INTERNAL", language));
		return;
	}
	
	language = getenv("HTTP_ACCEPT_LANGUAGE");
	printf("Content-Type:text/html;charset=utf-8\n\n");
	printf(HTML_COMMON_1);
	printf(lang_resource_get(g_lang_resource,"MAIN_HTML_TITLE", language));
	printf(HTML_COMMON_2);
	printf(lang_resource_get(g_lang_resource,"MAIN_HTML_TITLE", language));
	printf(HTML_COMMON_4);
	printf(g_logo_link);
	printf(HTML_MAIN_5, url_buff, domain, session, url_buff, domain, session);
	printf(url_buff);
	printf(HTML_MAIN_6, domain, session,
		lang_resource_get(g_lang_resource, "MAIN_ROOM", language),
		lang_resource_get(g_lang_resource, "ADD_LABEL", language));
	printf(lang_resource_get(g_lang_resource, "MAIN_TABLE_TITLE", language));	
	printf(HTML_MAIN_7);
	printf(HTML_TBITEM_FIRST,
		lang_resource_get(g_lang_resource,"MAIN_ROOM", language),
		lang_resource_get(g_lang_resource,"MAIN_OPERATION", language));
	for (data_source_collect_begin(pcollect);
		!data_source_collect_done(pcollect);
		data_source_collect_forward(pcollect)) {
		proom_name = (char*)data_source_collect_get_value(pcollect);
		printf(HTML_TBITEM_NORMAL, proom_name, proom_name,
			lang_resource_get(g_lang_resource, "EDIT_LABEL", language), proom_name,
			lang_resource_get(g_lang_resource, "DELETE_LABEL", language));
	}
	data_source_collect_free(pcollect);
	printf(HTML_MAIN_8);
}

static void list_ui_permission_html(const char *domain,
	const char *session, const char *room)
{
	int i;
	char *language;
	char *pusername;
	char url_buff[1024];
	DATA_COLLECT *pcollect;
	
	pcollect = data_source_collect_init();
	if (NULL == pcollect) {
		list_ui_error_html(lang_resource_get(
			g_lang_resource, "ERROR_INTERNAL", language));
		return;
	}
	language = getenv("HTTP_ACCEPT_LANGUAGE");
	if (NULL == language) {
		language = "en";
	}
	if (FALSE == list_ui_get_self(url_buff, 1024)) {
		list_ui_error_html(lang_resource_get(
			g_lang_resource, "ERROR_INTERNAL", language));
		return;
	}
	
	if (FALSE == data_source_list_permissions(domain, room, pcollect)) {
		list_ui_error_html(lang_resource_get(
			g_lang_resource, "ERROR_INTERNAL", language));
		return;
	}
	language = getenv("HTTP_ACCEPT_LANGUAGE");
	if (FALSE == list_ui_get_self(url_buff, 1024)) {
		list_ui_error_html(lang_resource_get(
			g_lang_resource, "ERROR_INTERNAL", language));
		return;
	}
	printf("Content-Type:text/html;charset=utf-8\n\n");
	printf(HTML_COMMON_1);
	printf(lang_resource_get(g_lang_resource, "PERMISSION_HTML_TITLE", language));
	printf(HTML_COMMON_2);
	printf(lang_resource_get(g_lang_resource, "PERMISSION_HTML_TITLE", language));
	printf(HTML_COMMON_4);
	printf(g_logo_link);
	printf(HTML_PERMISSION_5, url_buff, domain, session,
		lang_resource_get(g_lang_resource, "BACK_TO_MAIN", language),
		url_buff, domain, session, room);
	printf(url_buff);
	printf(HTML_PERMISSION_6, domain, session, room,
		lang_resource_get(g_lang_resource, "MAIN_USERNAME", language),
		lang_resource_get(g_lang_resource, "ADD_LABEL", language));
	printf(lang_resource_get(g_lang_resource, "PERMISSION_TABLE_TITLE", language));
	printf(HTML_PERMISSION_7);
	printf(HTML_USERITEM_FIRST,
		lang_resource_get(g_lang_resource, "MAIN_OPERATION", language),
		lang_resource_get(g_lang_resource, "MAIN_USERNAME", language));
	for (data_source_collect_begin(pcollect);
		!data_source_collect_done(pcollect);
		data_source_collect_forward(pcollect)) {
		pusername = (char*)data_source_collect_get_value(pcollect);
		printf(HTML_USERITEM_NORMAL, pusername, pusername,
			lang_resource_get(g_lang_resource,"DELETE_LABEL", language));
	}
	printf(HTML_PERMISSION_8);
}
