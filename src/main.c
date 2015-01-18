#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <err.h>
#include <fcntl.h>
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
	enum src_pos	 focused;	/* Which box is focused */
	enum which_clip	 clipboard;	/* Where to source text */
	GtkTextBuffer	*top_buf;	/* The buffer with the top text */
	GtkTextBuffer	*bot_buf;	/* The buffer with the bottom text */
	GtkTextView	*top_view;	/* The view with the top text buffer */
	GtkTextView	*bot_view;	/* The view with the bottom text buffer */
	const char	*top_lang;	/* The language up top */
	const char	*bot_lang;	/* The language down bottom */
	GtkWindow	*parent;	/* The parent window */
};

/* This is used to transfer data out of curl */
struct mem_buf {
	char	*mem;	/* The actual string */
	size_t	 size;	/* The size of the string */
};

#define TRANS_URL_FMT "https://translate.google.com/translate_a/single?client=t&sl=%s&tl=%s&dt=bd&dt=t&dt=at&q=%s"

#define USER_AGENT "User-Agent: Mozilla/5.0 (X11; Linux i686; rv:10.0.12) Gecko/20100101 Firefox/10.0.12 Iceweasel/10.0.12"

static void		 top_but_cb(GtkButton *, gpointer);
static void		 bot_but_cb(GtkButton *, gpointer);
static void		 top_combo_cb(GtkComboBox *, gpointer);
static void		 bot_combo_cb(GtkComboBox *, gpointer);
static void		 clear_cb(GtkMenuItem *, gpointer);
static void		 open_cb(GtkMenuItem *, gpointer);
static void		 cut_cb(GtkMenuItem *, gpointer);
static void		 copy_cb(GtkMenuItem *, gpointer);
static void		 paste_cb(GtkMenuItem *, gpointer);
static void		 about_cb(GtkMenuItem *, gpointer);
static void		 from_clip_cb(GtkWidget *, gpointer);
static void		 clip_received_cb(GtkClipboard *, const gchar *,
    gpointer);
static gboolean		 top_text_in_cb(GtkWidget *, GdkEvent *, gpointer);
static gboolean		 bot_text_in_cb(GtkWidget *, GdkEvent *, gpointer);
static gboolean		 top_text_out_cb(GtkWidget *, GdkEvent *, gpointer);
static gboolean		 bot_text_out_cb(GtkWidget *, GdkEvent *, gpointer);

static GtkTextView	*focused_text_view(struct state *s);

static void		 translate_box(struct state *);
static void		 insert_sentence(JsonArray *, guint, JsonNode *,
    gpointer);

static size_t		 accumulate_mem_buf(char *, size_t, size_t, void *);
static struct mem_buf	 mem_buf_new();
static void		 mem_buf_free(struct mem_buf);

void			 replace_text_from_file(GtkTextBuffer *, char *);
static char		*read_fd(int);

__dead void		 usage();

/*
 * idiom(1) is a GUI program for translating text from one language to another.
 */
int
main(int argc, char *argv[])
{
	GtkBuilder	*builder;
	GtkWidget	*window, *top_text, *bot_text, *top_but, *bot_but;
	GtkWidget	*top_combo, *bot_combo;
	GtkWidget	*file_new, *file_open, *file_quit;
	GtkWidget	*edit_cut, *edit_copy, *edit_paste, *help_about;
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
	file_new = GTK_WIDGET(gtk_builder_get_object(builder, "menu-item-file-new"));
	file_open = GTK_WIDGET(gtk_builder_get_object(builder, "menu-item-file-open"));
	file_quit = GTK_WIDGET(gtk_builder_get_object(builder, "menu-item-file-quit"));
	edit_cut = GTK_WIDGET(gtk_builder_get_object(builder, "menu-item-edit-cut"));
	edit_copy = GTK_WIDGET(gtk_builder_get_object(builder, "menu-item-edit-copy"));
	edit_paste = GTK_WIDGET(gtk_builder_get_object(builder, "menu-item-edit-paste"));
	help_about = GTK_WIDGET(gtk_builder_get_object(builder, "menu-item-help-about"));

	s.top_buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(top_text));
	s.bot_buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(bot_text));
	s.top_view = GTK_TEXT_VIEW(top_text);
	s.bot_view = GTK_TEXT_VIEW(bot_text);
	s.top_lang = gtk_combo_box_get_active_id(GTK_COMBO_BOX(top_combo));
	s.bot_lang = gtk_combo_box_get_active_id(GTK_COMBO_BOX(bot_combo));
	s.clipboard = from_clipboard;
	s.active = NO_BOX;
	s.focused = TOP_BOX;
	s.parent = GTK_WINDOW(window);

	gtk_window_set_default_size(GTK_WINDOW(window), 800, 400);
	g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
	g_signal_connect(window, "realize", G_CALLBACK(from_clip_cb), &s);
	g_signal_connect(top_but, "clicked", G_CALLBACK(top_but_cb), &s);
	g_signal_connect(bot_but, "clicked", G_CALLBACK(bot_but_cb), &s);
	g_signal_connect(top_combo, "changed", G_CALLBACK(top_combo_cb), &s);
	g_signal_connect(bot_combo, "changed", G_CALLBACK(bot_combo_cb), &s);
	g_signal_connect(top_text, "focus-in-event", G_CALLBACK(top_text_in_cb), &s);
	g_signal_connect(bot_text, "focus-in-event", G_CALLBACK(bot_text_in_cb), &s);
	g_signal_connect(top_text, "focus-out-event", G_CALLBACK(top_text_out_cb), &s);
	g_signal_connect(bot_text, "focus-out-event", G_CALLBACK(bot_text_out_cb), &s);
	g_signal_connect(file_new, "activate", G_CALLBACK(clear_cb), &s);
	g_signal_connect(file_open, "activate", G_CALLBACK(open_cb), &s);
	g_signal_connect(file_quit, "activate", G_CALLBACK(gtk_main_quit), NULL);
	g_signal_connect(edit_cut, "activate", G_CALLBACK(cut_cb), &s);
	g_signal_connect(edit_copy, "activate", G_CALLBACK(copy_cb), &s);
	g_signal_connect(edit_paste, "activate", G_CALLBACK(paste_cb), &s);
	g_signal_connect(help_about, "activate", G_CALLBACK(about_cb), window);

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
 * When the user focuses the top box, consider it focused.
 */
static gboolean
top_text_in_cb(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
	struct state	*s;
	s = (struct state *)user_data;

	s->focused = TOP_BOX;

	return FALSE;
}

/*
 * When the user focuses the bottom box, consider it focused.
 */
static gboolean
bot_text_in_cb(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
	struct state	*s;
	s = (struct state *)user_data;

	s->focused = BOT_BOX;

	return FALSE;
}

/*
 * When the user unfocuses the top box, and that box was focused, consider no
 * box focused.
 */
static gboolean
top_text_out_cb(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
	struct state	*s;
	s = (struct state *)user_data;

	if (s->focused == TOP_BOX)
		s->focused = NO_BOX;

	return FALSE;
}

/*
 * When the user unfocuses the bottom box, and that box was focused, consider
 * no box focused.
 */
static gboolean
bot_text_out_cb(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
	struct state	*s;
	s = (struct state *)user_data;

	if (s->focused == BOT_BOX)
		s->focused = NO_BOX;

	return FALSE;
}

/*
 * Clear the text buffers.
 */
static void
clear_cb(GtkMenuItem *menuitem, gpointer user_data)
{
	struct state	*s;
	s = (struct state *)user_data;

	gtk_text_buffer_set_text(s->top_buf, "", 0);
	gtk_text_buffer_set_text(s->bot_buf, "", 0);
}

/*
 * Show a file open dialog, then load the selected file into the top buffer.
 */
static void
open_cb(GtkMenuItem *menuitem, gpointer user_data)
{
	GtkWidget	*dialog;
	struct state	*s;
	char		*path;

	s = (struct state *)user_data;

	dialog = gtk_file_chooser_dialog_new(
	    "Open File", s->parent, GTK_FILE_CHOOSER_ACTION_OPEN,
	    "_Cancel", GTK_RESPONSE_CANCEL,
	    "_Open", GTK_RESPONSE_ACCEPT,
	    NULL);

	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		if ((path = gtk_file_chooser_get_filename(
				    GTK_FILE_CHOOSER(dialog))) != NULL) {
			replace_text_from_file(s->top_buf, path);
		}
		g_free(path);
	}

	gtk_widget_destroy(dialog);
}

/*
 * Show the about dialog.
 */
static void
about_cb(GtkMenuItem *menuitem, gpointer user_data)
{
	GtkWindow	*parent;
	char		*authors[] = { "Mike Burns", NULL };

	parent = GTK_WINDOW(user_data);

	gtk_show_about_dialog(parent,
	    "program-name", "Idiom",
	    "version", PACKAGE_VERSION,
	    "copyright", "Copyright 2014, 2015 Mike Burns",
	    "comments", "A utility to translate the written word.",
	    "website", "https://github.com/bitptr/idiom",
	    "website_label", "Web site",
	    "license_type", GTK_LICENSE_BSD,
	    "authors", authors,
	    NULL);
}

/*
 * Activate the cut-clipboard signal on the focused text view.
 */
static void
cut_cb(GtkMenuItem *menuitem, gpointer user_data)
{
	GtkTextView	*text_view;
	struct state	*s;

	s = (struct state *)user_data;
	if ((text_view = focused_text_view(s)) == NULL)
		return;

	g_signal_emit_by_name(text_view, "cut-clipboard", NULL);
}

/*
 * Activate the copy-clipboard signal on the focused text view.
 */
static void
copy_cb(GtkMenuItem *menuitem, gpointer user_data)
{
	GtkTextView	*text_view;
	struct state	*s;

	s = (struct state *)user_data;
	if ((text_view = focused_text_view(s)) == NULL)
		return;

	g_signal_emit_by_name(text_view, "copy-clipboard", NULL);
}

/*
 * Activate the paste-clipboard signal on the focused text view.
 */
static void
paste_cb(GtkMenuItem *menuitem, gpointer user_data)
{
	GtkTextView	*text_view;
	struct state	*s;

	s = (struct state *)user_data;
	if ((text_view = focused_text_view(s)) == NULL)
		return;

	g_signal_emit_by_name(text_view, "paste-clipboard", NULL);
}

/*
 * RETURN: a pointer to the GtkTexView that is currently focused. If no box is
 * currently focused, return NULL.
 */
static GtkTextView *
focused_text_view(struct state *s)
{
	GtkTextView	*text_view;

	switch (s->focused) {
	case TOP_BOX:
		text_view = s->top_view;
		break;
	case BOT_BOX:
		text_view = s->bot_view;
		break;
	case NO_BOX:
		return NULL;
		/* NOTREACHED */
		break;
	default:
		errx(EX_SOFTWARE, "unknown src_pos");
	}

	return text_view;
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
	struct mem_buf		 raw_json, translation;
	JsonParser		*parser;
	GError			*error;
	JsonNode		*root;
	JsonArray		*sentences;
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

	raw_json = mem_buf_new();

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
	curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, accumulate_mem_buf);
	curl_easy_setopt(handle, CURLOPT_WRITEDATA, &raw_json);

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
	for (i = raw_json.size; i > 0; i--)
		if (raw_json.mem[i-1] == ',' && raw_json.mem[i] == ',')
			raw_json.mem[i] = ' ';

	error = NULL;
	parser = json_parser_new();
	if (!json_parser_load_from_data(parser, raw_json.mem, raw_json.size, &error))
		errx(2, "json_parser_load_from_data: %s", error->message);

	root = json_parser_get_root(parser);
	sentences = json_array_get_array_element(
	    json_node_get_array(root), 0);

	translation = mem_buf_new();
	json_array_foreach_element(sentences, insert_sentence, &translation);
	gtk_text_buffer_set_text(dst_g_buf, translation.mem, -1);
	mem_buf_free(translation);

	/* cleanup */
	if (parser != NULL)
		g_object_unref(parser);
	mem_buf_free(raw_json);
}

/*
 * Pull the sentence from the translation pair and add it to the memory buffer.
 */
static void
insert_sentence(JsonArray *array, guint index, JsonNode *element_node,
    gpointer user_data)
{
	struct mem_buf	*res;
	JsonArray	*pair;
	const gchar	*value;
	char		*ptr;
	size_t		 cpy_len, len;

	res = (struct mem_buf *)user_data;
	pair = json_node_get_array(element_node);
	value = json_array_get_string_element(pair, 0);

	if ((ptr = calloc(sizeof(char), 1)) == NULL)
		err(1, "calloc");
	len = strlen(value);
	cpy_len = strlcpy(ptr, value, len + 1);
	if (cpy_len < len)
		err(1, "strlcpy");

	accumulate_mem_buf(ptr, sizeof(char), strlen(value), res);

	free(ptr);
}

/*
 * Add the string to the memory buffer.
 */
static size_t
accumulate_mem_buf(char *ptr, size_t size, size_t nmemb, void *userdata)
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

/*
 * Load the contents of the file into the given text buffer widget.
 */
void
replace_text_from_file(GtkTextBuffer *text_buf, char *path)
{
	char	*buf;
	int	 fd;

	if ((fd = open(path, O_RDONLY)) == -1)
		err(1, "open");
	buf = read_fd(fd);
	close(fd);

	gtk_text_buffer_set_text(text_buf, buf, -1);

	free(buf);
}

/*
 * Read the contents of a file descriptor and produce that as a character
 * pointer.
 */
static char *
read_fd(int fd)
{
	char	*buf, *nbuf;
	int	 nread;
	ssize_t	 ret;
	size_t	 nbytes;

	nbytes = 1024;
	nread = 0;

	if ((buf = calloc(nbytes, sizeof(char))) == NULL)
		err(1, "calloc");

	while ((ret = read(fd, buf + nread, nbytes)) > 0) {
		nread += nbytes;
		nbuf = reallocarray(buf, nread + nbytes, sizeof(char));
		if (nbuf == NULL)
			err(1, "reallocarray");

		buf = nbuf;
	}

	if (ret == -1)
		err(1, "read");

	return buf;
}
