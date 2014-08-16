#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <err.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>

#include <curl/curl.h>
#include <gtk/gtk.h>
#include <json-glib/json-glib.h>

#include "pathnames.h"

/*
 * NOTES:
 *
 * curl 'https://translate.google.com/translate_a/single?client=t&sl=sv&tl=en&hl=en&dt=bd&dt=ex&dt=ld&dt=md&dt=qc&dt=rw&dt=rm&dt=ss&dt=t&dt=at&dt=sw&ie=UTF-8&oe=UTF-8&oc=1&otf=2&ssel=4&tsel=0&q=hej%20d%C3%A5' -H 'User-Agent: Mozilla/5.0 (X11; Linux i686; rv:10.0.12) Gecko/20100101 Firefox/10.0.12 Iceweasel/10.0.12'
 *
 * Minimal:
 *
 * curl 'https://translate.google.com/translate_a/single?client=t&sl=sv&tl=en&dt=bd&dt=t&dt=at&q=hej%20d%C3%A5' -H 'User-Agent: Mozilla/5.0 (X11; Linux i686; rv:10.0.12) Gecko/20100101 Firefox/10.0.12 Iceweasel/10.0.12
 *
 * sl - source language - "sv"
 * tl - target language - "en"
 * q  - query           - "hej då"
 *
 */

enum src_pos {
	TOP_BOX,
	BOT_BOX,
	BOX_CNT
};

struct state {
	enum src_pos	 active;
	GtkTextView	*top_text;
	GtkTextView	*bot_text;
};

struct mem_buf {
	char	*mem;
	size_t	 size;
};

#define TRANS_URL_FMT "https://translate.google.com/translate_a/single?client=t&sl=sv&tl=en&dt=bd&dt=t&dt=at&q=%s"
#define USER_AGENT "User-Agent: Mozilla/5.0 (X11; Linux i686; rv:10.0.12) Gecko/20100101 Firefox/10.0.12 Iceweasel/10.0.12"

static void		top_but_cb(GtkButton *, gpointer);
static void		bot_but_cb(GtkButton *, gpointer);
static void		translate_box(struct state *);
static size_t		accumulate_json(char *, size_t, size_t, void *);
static struct mem_buf	mem_buf_new();
void			mem_buf_free(struct mem_buf);

int
main(int argc, char *argv[])
{
	GtkBuilder	*builder;
	GtkWidget	*window, *top_text, *bot_text, *top_but, *bot_but;
	struct state	 s;

	gtk_init(&argc, &argv);

	builder = gtk_builder_new_from_file(INTERFACE_PATH);
	window = GTK_WIDGET(gtk_builder_get_object(builder, "window1"));
	top_text = GTK_WIDGET(gtk_builder_get_object(builder, "textview1"));
	bot_text = GTK_WIDGET(gtk_builder_get_object(builder, "textview2"));
	top_but = GTK_WIDGET(gtk_builder_get_object(builder, "button1"));
	bot_but = GTK_WIDGET(gtk_builder_get_object(builder, "button2"));

	s.top_text = GTK_TEXT_VIEW(top_text);
	s.bot_text = GTK_TEXT_VIEW(bot_text);

	gtk_window_set_default_size(GTK_WINDOW(window), 800, 400);
	g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
	g_signal_connect(top_but, "clicked", G_CALLBACK(top_but_cb), &s);
	g_signal_connect(bot_but, "clicked", G_CALLBACK(bot_but_cb), &s);

	gtk_widget_show(window);

	gtk_main();

	return 0;
}

static void
top_but_cb(GtkButton *button, gpointer user_data)
{
	struct state	*s;
	s = (struct state *)user_data;

	s->active = TOP_BOX;

	translate_box(s);
}

static void
bot_but_cb(GtkButton *button, gpointer user_data)
{
	struct state	*s;
	s = (struct state *)user_data;

	s->active = BOT_BOX;

	translate_box(s);
}

static void
translate_box(struct state *s)
{
	GtkTextView		*src_text, *dst_text;
	GtkTextBuffer		*src_g_buf, *dst_g_buf;
	GtkTextIter		src_start, src_end;
	CURL			*handle;
	struct curl_slist	*headers;
 	gchar			*src_buf;
	char			*url, *esc_buf, errbuf[CURL_ERROR_SIZE];
	int			 len;
	size_t			 i;
	struct mem_buf		 result;
	JsonParser		*parser;
	GError			*error;
	JsonNode		*root;
	const gchar		*value;

	src_text = NULL;
	dst_text = NULL;

	/* get the string from the user */

	switch (s->active) {
	case TOP_BOX:
		src_text = s->top_text;
		dst_text = s->bot_text;
		break;
	case BOT_BOX:
		src_text = s->bot_text;
		dst_text = s->top_text;
		break;
	default:
		errx(EX_SOFTWARE, "unknown src_pos");
	}

	headers = NULL;

	src_g_buf = gtk_text_view_get_buffer(src_text);
	dst_g_buf = gtk_text_view_get_buffer(dst_text);

	gtk_text_buffer_get_start_iter(src_g_buf, &src_start);
	gtk_text_buffer_get_end_iter(src_g_buf, &src_end);

	src_buf = gtk_text_buffer_get_text(src_g_buf, &src_start, &src_end, 0);

	result = mem_buf_new();

	/* get the translation JSON */

	if ((handle = curl_easy_init()) == NULL)
		errx(1, "curl_easy_init failed");

	len = strlen(src_buf);
	if ((esc_buf = curl_easy_escape(handle, src_buf, len)) == NULL)
		err(1, "curl_easy_escape");
	/*    URL format            - %s + user input      + \0 */
	len = strlen(TRANS_URL_FMT) -  2 + strlen(esc_buf) + 1;
	if ((url = calloc(len, sizeof(char))) == NULL)
		err(1, "calloc");
	snprintf(url, len, TRANS_URL_FMT, esc_buf);

	if ((headers = curl_slist_append(headers, "User-Agent: "USER_AGENT)) == NULL)
		errx(1, "curl_slist_append");

	curl_easy_setopt(handle, CURLOPT_URL, url);
	curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(handle, CURLOPT_ERRORBUFFER, &errbuf);
	curl_easy_setopt(handle, CURLOPT_VERBOSE, 0);
	curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, accumulate_json);
	curl_easy_setopt(handle, CURLOPT_WRITEDATA, &result);

	if (curl_easy_perform(handle) != 0)
		errx(2, "curl: %s", errbuf);

	curl_easy_cleanup(handle);
	curl_slist_free_all(headers);

	/* parse the JSON */

	/*
	 * Remove doubled commas. This traverses from the back but checks from
	 * the front - that is, given ",,,", it will go from the right but
	 * check whether the character to the left is a comma, resulting in 
	 * ",  ".
	 */
	for (i = result.size; i > 0; i--)
		if (result.mem[i-1] == ',' && result.mem[i] == ',')
			result.mem[i] = ' ';

	error = NULL;
	parser = json_parser_new();
	if (!json_parser_load_from_data(parser, result.mem, result.size, &error))
		errx(2, "json_parser_load_from_data: %s", error->message);

	root = json_parser_get_root(parser);
	value = json_array_get_string_element(
	    json_array_get_array_element(
		    json_array_get_array_element(
			    json_node_get_array(root), 0), 0), 0);

	/* insert the value into the GtkTextView */
	gtk_text_buffer_set_text(dst_g_buf, value, -1);

	/* cleanup */
	g_object_unref(parser);
	mem_buf_free(result);
}

size_t
accumulate_json(char *ptr, size_t size, size_t nmemb, void *userdata)
{
	size_t	len;
	struct mem_buf	*m;

	len = size * nmemb;
	m = (struct mem_buf *)userdata;

	if ((m->mem = realloc(m->mem, m->size + len + 1)) == NULL)
		err(1, "realloc");

	memcpy(&(m->mem[m->size]), ptr, len);
	m->size += len;
	m->mem[m->size] = 0;

	return len;
}

struct mem_buf
mem_buf_new()
{
	struct mem_buf	m;

	m.size = 0;
	if ((m.mem = calloc(1, sizeof(char))) == NULL)
		err(1, "calloc");

	return m;
}

void
mem_buf_free(struct mem_buf m)
{
	free(m.mem);
}
