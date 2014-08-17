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

#include "compat.h"
#include "pathnames.h"

enum src_pos {
	NO_BOX,
	TOP_BOX,
	BOT_BOX
};

enum which_clip {
	NO_CLIPBOARD,
	PRIMARY,
	SECONDARY
};

struct state {
	enum src_pos	 active;	/* Which box should be translated */
	enum which_clip	 clipboard;
	GtkTextBuffer	*top_buf;	/* The buffer with the top text */
	GtkTextBuffer	*bot_buf;	/* The buffer with the bottom text */
	const char	*top_lang;	/* The language up top */
	const char	*bot_lang;	/* The language down bottom */
};

/* This is used to transfer data out of curl */
struct mem_buf {
	char	*mem;	/* The actual string */
	size_t	 size;	/* The size of the string */
};

#define TRANS_URL_FMT "https://translate.google.com/translate_a/single?client=t&sl=%s&tl=%s&dt=bd&dt=t&dt=at&q=%s"

#define USER_AGENT "User-Agent: Mozilla/5.0 (X11; Linux i686; rv:10.0.12) Gecko/20100101 Firefox/10.0.12 Iceweasel/10.0.12"

static void		top_but_cb(GtkButton *, gpointer);
static void		bot_but_cb(GtkButton *, gpointer);
static void		top_combo_cb(GtkComboBox *, gpointer);
static void		bot_combo_cb(GtkComboBox *, gpointer);
static void		from_clip_cb(GtkWidget *, gpointer);
static void		clip_received_cb(GtkClipboard *, const gchar *,
    gpointer);

static void		translate_box(struct state *);
static size_t		accumulate_json(char *, size_t, size_t, void *);

static struct mem_buf	mem_buf_new();
static void		mem_buf_free(struct mem_buf);

__dead void		usage();

/*
 * idiom(1) is a GUI program for translating text from one language to another.
 */
int
main(int argc, char *argv[])
{
	GtkBuilder	*builder;
	GtkWidget	*window, *top_text, *bot_text, *top_but, *bot_but;
	GtkWidget	*top_combo, *bot_combo;
	struct state	 s;
	enum which_clip	 from_clipboard;
	int		 ch;

	from_clipboard = NO_CLIPBOARD;

	gtk_init(&argc, &argv);

	while ((ch = getopt(argc, argv, "p")) != -1)
		switch (ch) {
		case 'p':
			from_clipboard = PRIMARY;
			break;
		default:
			usage();
			/* NOTREACHED */
			break;
		}
	argc -= optind;
	argv += optind;

	builder = gtk_builder_new_from_file(INTERFACE_PATH);
	window = GTK_WIDGET(gtk_builder_get_object(builder, "window1"));
	top_text = GTK_WIDGET(gtk_builder_get_object(builder, "textview1"));
	bot_text = GTK_WIDGET(gtk_builder_get_object(builder, "textview2"));
	top_but = GTK_WIDGET(gtk_builder_get_object(builder, "button1"));
	bot_but = GTK_WIDGET(gtk_builder_get_object(builder, "button2"));
	top_combo = GTK_WIDGET(gtk_builder_get_object(builder, "comboboxtext1"));
	bot_combo = GTK_WIDGET(gtk_builder_get_object(builder, "comboboxtext2"));

	s.top_buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(top_text));
	s.bot_buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(bot_text));
	s.top_lang = gtk_combo_box_get_active_id(GTK_COMBO_BOX(top_combo));
	s.bot_lang = gtk_combo_box_get_active_id(GTK_COMBO_BOX(bot_combo));
	s.clipboard = from_clipboard;
	s.active = NO_BOX;

	gtk_window_set_default_size(GTK_WINDOW(window), 800, 400);
	g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
	g_signal_connect(window, "realize", G_CALLBACK(from_clip_cb), &s);
	g_signal_connect(top_but, "clicked", G_CALLBACK(top_but_cb), &s);
	g_signal_connect(bot_but, "clicked", G_CALLBACK(bot_but_cb), &s);
	g_signal_connect(top_combo, "changed", G_CALLBACK(top_combo_cb), &s);
	g_signal_connect(bot_combo, "changed", G_CALLBACK(bot_combo_cb), &s);

	gtk_widget_show(window);

	gtk_main();

	return 0;
}

/*
 * Display a usage message and exit.
 */
void
usage()
{
	fprintf(stderr, "usage: idiom [-p]\n");
	exit(EX_USAGE);
}

/*
 * Copy the clipboard into the top buffer then translate it.
 */
void
from_clip_cb(GtkWidget *widget, gpointer user_data)
{
	GtkClipboard	*cb;
	struct state	*s;

	s = (struct state *)user_data;

	if (s->clipboard == PRIMARY) {
		cb = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
		gtk_clipboard_request_text(cb, clip_received_cb, user_data);
	}
}

/*
 * Set the top buffer's text then translate it.
 */
void
clip_received_cb(GtkClipboard *clipboard, const gchar *text, gpointer data)
{
	struct state	*s;

	s = (struct state *)data;
	s->active = TOP_BOX;
	gtk_text_buffer_set_text(s->top_buf, text, -1);

	translate_box(s);
}

/*
 * Translate the top box.
 *
 * This function exists to set the active box in the state then pass that onto
 * the translator.
 */
static void
top_but_cb(GtkButton *button, gpointer user_data)
{
	struct state	*s;
	s = (struct state *)user_data;

	s->active = TOP_BOX;

	translate_box(s);
}

/*
 * Translate the bottom box.
 *
 * This function exists to set the active box in the state then pass that onto
 * the translator.
 */
static void
bot_but_cb(GtkButton *button, gpointer user_data)
{
	struct state	*s;
	s = (struct state *)user_data;

	s->active = BOT_BOX;

	translate_box(s);
}

/*
 * Change the language up top, then re-translate.
 */
static void
top_combo_cb(GtkComboBox *widget, gpointer user_data)
{
	struct state	*s;
	s = (struct state *)user_data;

	s->top_lang = gtk_combo_box_get_active_id(widget);

	translate_box(s);
}

/*
 * Change the language below, then re-translate.
 */
static void
bot_combo_cb(GtkComboBox *widget, gpointer user_data)
{
	struct state	*s;
	s = (struct state *)user_data;

	s->bot_lang = gtk_combo_box_get_active_id(widget);

	translate_box(s);
}

/*
 * Translate the text into the other box.
 *
 * This function does too much:
 *
 * 1. Read the buffer from the active box.
 * 2. Fetch a translation from the Internet.
 * 3. Parse the JSON.
 * 4. Set the buffer of the inactive box.
 */
static void
translate_box(struct state *s)
{
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
	const char		*src_lang, *dst_lang;

	headers = NULL;
	parser = NULL;
	src_buf = NULL;

	/* get the string from the user */

	switch (s->active) {
	case TOP_BOX:
		src_g_buf = s->top_buf;
		dst_g_buf = s->bot_buf;
		src_lang = s->top_lang;
		dst_lang = s->bot_lang;
		break;
	case BOT_BOX:
		src_g_buf = s->bot_buf;
		dst_g_buf = s->top_buf;
		src_lang = s->bot_lang;
		dst_lang = s->top_lang;
		break;
	case NO_BOX:
		return;
		/* NOTREACHED */
		break;
	default:
		errx(EX_SOFTWARE, "unknown src_pos");
	}

	gtk_text_buffer_get_start_iter(src_g_buf, &src_start);
	gtk_text_buffer_get_end_iter(src_g_buf, &src_end);
	src_buf = gtk_text_buffer_get_text(src_g_buf, &src_start, &src_end, 0);

	if (*src_buf == '\0')
		return;

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
	snprintf(url, len, TRANS_URL_FMT, src_lang, dst_lang, esc_buf);

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
	if (parser != NULL)
		g_object_unref(parser);
	mem_buf_free(result);
}

/*
 * Add the response string from HTTP to the memory buffer.
 */
static size_t
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

/*
 * Build a new empty memory buffer: size zero, allocated space for a single
 * char.
 */
static struct mem_buf
mem_buf_new()
{
	struct mem_buf	m;

	m.size = 0;
	if ((m.mem = calloc(1, sizeof(char))) == NULL)
		err(1, "calloc");

	return m;
}

/*
 * Free the memory buffer.
 */
static void
mem_buf_free(struct mem_buf m)
{
	free(m.mem);
	m.mem = NULL;
	m.size = 0;
}
