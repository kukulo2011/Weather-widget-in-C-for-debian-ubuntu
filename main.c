/* gcc main.c -o weather $(pkg-config --cflags --libs gtk+-3.0 appindicator3-0.1 json-c libcurl) */
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <libappindicator/app-indicator.h>
#include <curl/curl.h>
#include <json-c/json.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define APPINDICATOR_ID "weather-indicator"

#define DEFAULT_LAT "50.0755"
#define DEFAULT_LON "14.4378"

#define DEFAULT_REFRESH 600

typedef struct {
    char *memory;
    size_t size;
} Memory;

static char latitude[64] = DEFAULT_LAT;
static char longitude[64] = DEFAULT_LON;

static int refresh_interval =
    DEFAULT_REFRESH;

static gboolean dark_mode = TRUE;

static AppIndicator *indicator;

static GtkWidget *window;
static GtkWidget *clock_label;
static GtkWidget *weather_icon_image;
static GtkWidget *current_label;
static GtkWidget *h1;
static GtkWidget *h2;
static GtkWidget *m1;
static GtkWidget *m2;
static GtkWidget *colon;

static GtkWidget *icon_row;
static GtkWidget *day_row;
static GtkWidget *night_row;
static GtkWidget *weekday_row;

static guint weather_timer_id = 0;

/* ------------------------------------------------ */

static void update_weather(void);

/* ---------------- HTTP ---------------- */

static size_t write_callback(
    void *contents,
    size_t size,
    size_t nmemb,
    void *userp)
{
    size_t realsize =
        size * nmemb;

    Memory *mem =
        (Memory *)userp;

    char *ptr =
        realloc(
            mem->memory,
            mem->size + realsize + 1
        );

    if (!ptr)
        return 0;

    mem->memory = ptr;

    memcpy(
        &(mem->memory[mem->size]),
        contents,
        realsize
    );

    mem->size += realsize;

    mem->memory[mem->size] = 0;

    return realsize;
}

static char *http_get(
    const char *url)
{
    CURL *curl =
        curl_easy_init();

    if (!curl)
        return NULL;

    Memory chunk;

    chunk.memory = malloc(1);
    chunk.size = 0;

    curl_easy_setopt(
        curl,
        CURLOPT_URL,
        url
    );

    curl_easy_setopt(
        curl,
        CURLOPT_WRITEFUNCTION,
        write_callback
    );

    curl_easy_setopt(
        curl,
        CURLOPT_WRITEDATA,
        &chunk
    );

    curl_easy_setopt(
        curl,
        CURLOPT_USERAGENT,
        "weather-widget/11.0"
    );

    curl_easy_setopt(
        curl,
        CURLOPT_FOLLOWLOCATION,
        1L
    );

    curl_easy_setopt(
        curl,
        CURLOPT_SSL_VERIFYPEER,
        0L
    );

    curl_easy_setopt(
        curl,
        CURLOPT_SSL_VERIFYHOST,
        0L
    );

    CURLcode res =
        curl_easy_perform(curl);

    if (res != CURLE_OK)
    {
        fprintf(stderr,
                "curl error: %s\n",
                curl_easy_strerror(res));

        free(chunk.memory);

        curl_easy_cleanup(curl);

        return NULL;
    }

    curl_easy_cleanup(curl);

    return chunk.memory;
}

/* ---------------- CONFIG ---------------- */

static void save_config(void)
{
    gchar *path =
        g_build_filename(
            g_get_home_dir(),
            ".weather-widget.conf",
            NULL
        );

    gchar *content =
        g_strdup_printf(
            "%s\n%s\n%d\n%d\n",
            latitude,
            longitude,
            refresh_interval,
            dark_mode
        );

    g_file_set_contents(
        path,
        content,
        -1,
        NULL
    );

    g_free(content);
    g_free(path);
}

static void load_config(void)
{
    gchar *path =
        g_build_filename(
            g_get_home_dir(),
            ".weather-widget.conf",
            NULL
        );

    gchar *content = NULL;

    if (g_file_get_contents(
            path,
            &content,
            NULL,
            NULL))
    {
        char lat[64];
        char lon[64];
        int interval;
        int dark;

        if (sscanf(
                content,
                "%63s\n%63s\n%d\n%d",
                lat,
                lon,
                &interval,
                &dark) >= 2)
        {
            g_strlcpy(
                latitude,
                lat,
                sizeof(latitude)
            );

            g_strlcpy(
                longitude,
                lon,
                sizeof(longitude)
            );

            if (interval > 10)
                refresh_interval =
                    interval;

            dark_mode = dark;
        }

        g_free(content);
    }

    g_free(path);
}

/* ---------------- POSITION ---------------- */

static void save_position(void)
{
    gint x, y;

    gtk_window_get_position(
        GTK_WINDOW(window),
        &x,
        &y
    );

    gchar *content =
        g_strdup_printf(
            "%d %d",
            x,
            y
        );

    gchar *path =
        g_build_filename(
            g_get_home_dir(),
            ".weather-widget.pos",
            NULL
        );

    g_file_set_contents(
        path,
        content,
        -1,
        NULL
    );

    g_free(content);
    g_free(path);
}

static void load_position(void)
{
    gchar *path =
        g_build_filename(
            g_get_home_dir(),
            ".weather-widget.pos",
            NULL
        );

    gchar *content = NULL;

    if (g_file_get_contents(
            path,
            &content,
            NULL,
            NULL))
    {
        int x, y;

        if (sscanf(
                content,
                "%d %d",
                &x,
                &y) == 2)
        {
            gtk_window_move(
                GTK_WINDOW(window),
                x,
                y
            );
        }

        g_free(content);
    }

    g_free(path);
}

/* ---------------- GEOLOCATION ---------------- */

static void autodetect_location(void)
{
    char *response =
        http_get(
            "https://ipapi.co/json/"
        );

    if (!response)
        return;

    struct json_object *root =
        json_tokener_parse(response);

    if (!root)
    {
        free(response);
        return;
    }

    struct json_object *lat_obj = NULL;
    struct json_object *lon_obj = NULL;

    json_object_object_get_ex(
        root,
        "latitude",
        &lat_obj
    );

    json_object_object_get_ex(
        root,
        "longitude",
        &lon_obj
    );

    if (lat_obj && lon_obj)
    {
        g_ascii_formatd(
            latitude,
            sizeof(latitude),
            "%.6f",
            json_object_get_double(
                lat_obj
            )
        );

        g_ascii_formatd(
            longitude,
            sizeof(longitude),
            "%.6f",
            json_object_get_double(
                lon_obj
            )
        );
    }

    json_object_put(root);

    free(response);
}

static void geocode_city(
    const char *city)
{
    CURL *curl =
        curl_easy_init();

    if (!curl)
        return;

    char *escaped =
        curl_easy_escape(
            curl,
            city,
            0
        );

    if (!escaped)
    {
        curl_easy_cleanup(curl);
        return;
    }

    char url[2048];

    snprintf(
        url,
        sizeof(url),
        "https://geocoding-api.open-meteo.com/v1/search?name=%s&count=1",
        escaped
    );

    curl_free(escaped);

    curl_easy_cleanup(curl);

    char *response =
        http_get(url);

    if (!response)
        return;

    struct json_object *root =
        json_tokener_parse(response);

    if (!root)
    {
        free(response);
        return;
    }

    struct json_object *results = NULL;

    json_object_object_get_ex(
        root,
        "results",
        &results
    );

    if (results &&
        json_object_array_length(results) > 0)
    {
        struct json_object *first =
            json_object_array_get_idx(
                results,
                0
            );

        struct json_object *lat_obj = NULL;
        struct json_object *lon_obj = NULL;

        json_object_object_get_ex(
            first,
            "latitude",
            &lat_obj
        );

        json_object_object_get_ex(
            first,
            "longitude",
            &lon_obj
        );

        if (lat_obj && lon_obj)
        {
            g_ascii_formatd(
                latitude,
                sizeof(latitude),
                "%.6f",
                json_object_get_double(
                    lat_obj
                )
            );

            g_ascii_formatd(
                longitude,
                sizeof(longitude),
                "%.6f",
                json_object_get_double(
                    lon_obj
                )
            );
        }
    }

    json_object_put(root);

    free(response);
}

/* ---------------- WEATHER ---------------- */

static const char *get_weather_desc(
    int code)
{
    if (code == 0)
        return "Clear";

    if (code <= 3)
        return "Cloudy";

    if (code <= 67)
        return "Rain";

    if (code <= 77)
        return "Snow";

    if (code <= 82)
        return "Showers";

    return "Unknown";
}

/* ---------------- ICONS ---------------- */

static const char *get_weather_icon_file(
    int code)
{
    if (code == 0)
        return "/opt/extras.ubuntu.com/my-weather-indicator/share/my-weather-indicator/wimages/05.png";

    if (code <= 3)
        return "/opt/extras.ubuntu.com/my-weather-indicator/share/my-weather-indicator/wimages/03.png";

    if (code <= 55)
        return "/opt/extras.ubuntu.com/my-weather-indicator/share/my-weather-indicator/wimages/mwig-partly-cloudy.png";

    if (code <= 67)
        return "/opt/extras.ubuntu.com/my-weather-indicator/share/my-weather-indicator/wimages/20.png";

    if (code <= 77)
        return "/opt/extras.ubuntu.com/my-weather-indicator/share/my-weather-indicator/wimages/22.png";

    if (code <= 82)
        return "/opt/extras.ubuntu.com/my-weather-indicator/share/my-weather-indicator/wimages/21.png";

    return "/opt/extras.ubuntu.com/my-weather-indicator/share/my-weather-indicator/wimages/17.png";
}

static GdkPixbuf *load_scaled_pixbuf(
    const char *path,
    int size)
{
    GError *err = NULL;

    GdkPixbuf *pb =
        gdk_pixbuf_new_from_file_at_scale(
            path,
            size,
            size,
            TRUE,
            &err
        );

    if (!pb)
    {
        if (err)
        {
            fprintf(stderr,
                    "pixbuf error: %s\n",
                    err->message);

            g_error_free(err);
        }
    }

    return pb;
}

/* ---------------- CSS ---------------- */

static void apply_theme(void)
{
    GtkCssProvider *provider =
        gtk_css_provider_new();

    const char *dark_css =
        "window {"
        "background-color: transparent;"
        "}"
        "label {"
        "color: white;"
        "}"
        "dialog label {"
        "color: black;"
        "}"
        "dialog checkbutton label {"
        "color: black;"
        "}"
        "dialog spinbutton {"
        "color: black;"
        "background: white;"
        "}"
        "dialog entry {"
        "color: black;"
        "background: white;"
        "}"
        "button label {"
        "color: black;"
        "}"
        "#current_label {"
        "font-size: 15px;"
        "font-weight: bold;"
        "}"
        "#forecast_temp {"
        "font-size: 10px;"
        "}";

    const char *light_css =
        "window {"
        "background-color: rgba(255,255,255,0.65);"
        "}"
        "label {"
        "color: black;"
        "}"
        "dialog label {"
        "color: black;"
        "}"
        "button label {"
        "color: black;"
        "}";

    gtk_css_provider_load_from_data(
        provider,
        dark_mode
            ? dark_css
            : light_css,
        -1,
        NULL
    );

    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
}

/* ---------------- FLIP DIGIT ---------------- */

/*
 * Each flip digit is a GtkDrawingArea drawn with Cairo.
 * We store the current char and animate a "flip" via a
 * fractional progress value (0.0 = still, 1.0 = done).
 *
 *  Card dimensions (pixels):
 *    CARD_W x CARD_H with CARD_R corner radius
 *
 *  During flip:
 *    0.0 .. 0.5  top half folds down  (old digit shrinks)
 *    0.5 .. 1.0  new digit revealed   (new digit grows up)
 */

/* Card sized for a 300×300 widget.
 * Narrower digits: CARD_W 44, font 36pt.
 */
#define CARD_W   44
#define CARD_H   64
#define CARD_R    7
#define CARD_FONT 36.0
#define FLIP_STEPS 14
#define FLIP_MS    16

typedef struct {
    GtkWidget *area;
    char       cur;        /* digit currently shown */
    char       next;       /* digit we are flipping to */
    int        step;       /* 0 = idle, 1..FLIP_STEPS = animating */
    guint      timer_id;
} FlipDigit;

/* Draw one rounded rectangle card */
static void card_rounded_rect(cairo_t *cr,
                               double x, double y,
                               double w, double h,
                               double r)
{
    cairo_new_sub_path(cr);
    cairo_arc(cr, x+w-r, y+r,   r, -G_PI/2,  0);
    cairo_arc(cr, x+w-r, y+h-r, r,  0,       G_PI/2);
    cairo_arc(cr, x+r,   y+h-r, r,  G_PI/2,  G_PI);
    cairo_arc(cr, x+r,   y+r,   r,  G_PI,   3*G_PI/2);
    cairo_close_path(cr);
}

/* Paint a digit face (top or bottom half) into cr.
 * half: 0 = top half, 1 = bottom half.
 * White card, black text — train-station style. */
static void draw_digit_half(cairo_t *cr,
                              int w, int h,
                              char digit,
                              int half,          /* 0=top 1=bottom */
                              double brightness) /* 0..1 */
{
    /* White card background with a very subtle gradient */
    cairo_pattern_t *bg;
    if (half == 0) {
        bg = cairo_pattern_create_linear(0, 0, 0, h);
        cairo_pattern_add_color_stop_rgb(bg, 0.0,
            1.0*brightness, 1.0*brightness, 1.0*brightness);
        cairo_pattern_add_color_stop_rgb(bg, 1.0,
            0.90*brightness, 0.90*brightness, 0.90*brightness);
    } else {
        bg = cairo_pattern_create_linear(0, 0, 0, h);
        cairo_pattern_add_color_stop_rgb(bg, 0.0,
            0.88*brightness, 0.88*brightness, 0.88*brightness);
        cairo_pattern_add_color_stop_rgb(bg, 1.0,
            0.96*brightness, 0.96*brightness, 0.96*brightness);
    }

    /* Clip to top or bottom half */
    cairo_save(cr);
    if (half == 0)
        cairo_rectangle(cr, 0, 0, w, h/2.0);
    else
        cairo_rectangle(cr, 0, h/2.0, w, h/2.0);
    cairo_clip(cr);

    /* Full card shape (clipping shows only the half) */
    card_rounded_rect(cr, 1, 1, w-2, h-2, CARD_R);
    cairo_set_source(cr, bg);
    cairo_fill_preserve(cr);

    /* Card border – dark grey */
    cairo_set_source_rgba(cr, 0.4, 0.4, 0.4, 1.0);
    cairo_set_line_width(cr, 1.5);
    cairo_stroke(cr);

    /* Digit text – black */
    char txt[2] = { digit, '\0' };
    cairo_select_font_face(cr,
        "DejaVu Sans Mono",
        CAIRO_FONT_SLANT_NORMAL,
        CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, CARD_FONT);
    cairo_text_extents_t ext;
    cairo_text_extents(cr, txt, &ext);
    double tx = (w - ext.width)  / 2.0 - ext.x_bearing;
    double ty = (h - ext.height) / 2.0 - ext.y_bearing;
    /* dim text proportionally when animating */
    double d = brightness < 1.0 ? brightness * 0.15 : 0.0;
    cairo_set_source_rgba(cr, d, d, d, 1.0);
    cairo_move_to(cr, tx, ty);
    cairo_show_text(cr, txt);

    cairo_restore(cr);
    cairo_pattern_destroy(bg);
}

/* Draw the centre crease (horizontal line splitting top/bottom) */
static void draw_crease(cairo_t *cr, int w, int h)
{
    cairo_set_source_rgba(cr, 0.55, 0.55, 0.55, 1.0);
    cairo_set_line_width(cr, 2.0);
    cairo_move_to(cr, CARD_R, h/2.0);
    cairo_line_to(cr, w - CARD_R, h/2.0);
    cairo_stroke(cr);
}

/* Main draw callback */
static gboolean flip_digit_draw(GtkWidget *widget,
                                 cairo_t   *cr,
                                 gpointer   user_data)
{
    FlipDigit *fd = (FlipDigit *)user_data;
    /* Use actual allocated size so the card fills the widget exactly */
    GtkAllocation alloc;
    gtk_widget_get_allocation(widget, &alloc);
    int w = alloc.width;
    int h = alloc.height;

    /* ---- static bottom half ---- */
    char bot_char = (fd->step > 0) ? fd->next : fd->cur;
    draw_digit_half(cr, w, h, bot_char, 1, 1.0);

    /* ---- static top half ---- */
    {
        char top_char = (fd->step > FLIP_STEPS/2) ? fd->next : fd->cur;
        draw_digit_half(cr, w, h, top_char, 0, 1.0);
    }

    /* ---- animated flap ---- */
    if (fd->step > 0) {
        double progress = (double)fd->step / FLIP_STEPS; /* 0..1 */
        double scale_y, brightness;
        char   flap_char;

        if (progress <= 0.5) {
            double t  = progress / 0.5;          /* 0 → 1 */
            scale_y   = 1.0 - t * 0.92;          /* never reaches 0 */
            brightness = 1.0 - t * 0.6;
            flap_char = fd->cur;
        } else {
            double t  = (progress - 0.5) / 0.5;  /* 0 → 1 */
            scale_y   = t * 0.92 + 0.08;          /* starts from 0.08 */
            brightness = t * 0.7 + 0.3;
            flap_char = fd->next;
        }

        /* scale_y is always in (0.08 .. 0.92) — never 0, never 1 */
        cairo_save(cr);
        cairo_rectangle(cr, 0, 0, w, h/2.0);
        cairo_clip(cr);

        cairo_translate(cr, 0, h/2.0);
        cairo_scale(cr, 1.0, scale_y);
        cairo_translate(cr, 0, -h/2.0);

        draw_digit_half(cr, w, h, flap_char, 0, brightness);
        draw_crease(cr, w, h);

        cairo_restore(cr);
    }

    /* Always draw the crease on the static card */
    draw_crease(cr, w, h);

    return FALSE;
}

/* Timer callback: advance animation frame */
static gboolean flip_digit_tick(gpointer user_data)
{
    FlipDigit *fd = (FlipDigit *)user_data;
    fd->step++;
    gtk_widget_queue_draw(fd->area);

    if (fd->step >= FLIP_STEPS) {
        fd->cur      = fd->next;
        fd->step     = 0;
        fd->timer_id = 0;
        return G_SOURCE_REMOVE;
    }
    return G_SOURCE_CONTINUE;
}

/* Public: set digit (triggers animation if changed) */
static void flip_digit_set(FlipDigit *fd, char c)
{
    if (fd->cur == c && fd->step == 0)
        return;
    if (fd->step > 0) {
        /* Already animating – commit instantly and restart */
        fd->cur = fd->next;
        if (fd->timer_id) {
            g_source_remove(fd->timer_id);
            fd->timer_id = 0;
        }
        fd->step = 0;
    }
    if (fd->cur == c)
        return;
    fd->next = c;
    fd->step = 1;
    fd->timer_id = g_timeout_add(FLIP_MS, flip_digit_tick, fd);
}

/* Constructor */
static FlipDigit *flip_digit_new(char initial)
{
    FlipDigit *fd = g_new0(FlipDigit, 1);
    fd->cur  = initial;
    fd->next = initial;
    fd->area = gtk_drawing_area_new();
    gtk_widget_set_size_request(fd->area, CARD_W, CARD_H);
    g_signal_connect(fd->area, "draw",
                     G_CALLBACK(flip_digit_draw), fd);
    return fd;
}

/* Colon widget – dark dots on transparent background, sized for 300px widget */
static gboolean colon_draw(GtkWidget *widget, cairo_t *cr,
                            gpointer data)
{
    GtkAllocation alloc;
    gtk_widget_get_allocation(widget, &alloc);
    int w = alloc.width;
    int h = alloc.height;
    double r = w * 0.18;           /* dot radius */
    double cx = w / 2.0;
    cairo_set_source_rgba(cr, 0.15, 0.15, 0.15, 1.0);
    /* upper dot */
    cairo_arc(cr, cx, h * 0.34, r, 0, 2*G_PI);
    cairo_fill(cr);
    /* lower dot */
    cairo_arc(cr, cx, h * 0.66, r, 0, 2*G_PI);
    cairo_fill(cr);
    return FALSE;
}

static FlipDigit *fd_h1;
static FlipDigit *fd_h2;
static FlipDigit *fd_m1;
static FlipDigit *fd_m2;

/* ============================================================
 * FLIP ICON
 * A white flip-card that holds a weather icon pixbuf.
 * Same split-card animation as FlipDigit but renders a scaled
 * pixbuf instead of a character.
 * ============================================================ */

#define ICON_W   44      /* card width  – matches one digit card  */
#define ICON_H   58      /* card height – slightly shorter        */
#define ICON_R    7      /* corner radius                         */
#define ICON_PAD  4      /* padding around icon inside card       */

typedef struct {
    GtkWidget  *area;
    GdkPixbuf  *cur_pb;   /* pixbuf currently shown (or NULL)     */
    GdkPixbuf  *next_pb;  /* pixbuf we are flipping to            */
    int         step;     /* 0 = idle, 1..FLIP_STEPS = animating  */
    guint       timer_id;
} FlipIcon;

/* Draw the white flip-card background (full card, clipped to half) */
static void fi_draw_card_half(cairo_t *cr, int w, int h,
                               GdkPixbuf *pb,
                               int half,        /* 0=top 1=bottom */
                               double alpha)    /* 0..1 dim        */
{
    /* Card gradient — dark in dark mode, white in light mode */
    cairo_pattern_t *bg;
    if (half == 0) {
        bg = cairo_pattern_create_linear(0, 0, 0, h);
        cairo_pattern_add_color_stop_rgb(bg, 0.0,
            dark_mode ? 0.16 : 1.00,
            dark_mode ? 0.16 : 1.00,
            dark_mode ? 0.16 : 1.00);
        cairo_pattern_add_color_stop_rgb(bg, 1.0,
            dark_mode ? 0.08 : 0.90,
            dark_mode ? 0.08 : 0.90,
            dark_mode ? 0.08 : 0.90);
    } else {
        bg = cairo_pattern_create_linear(0, 0, 0, h);
        cairo_pattern_add_color_stop_rgb(bg, 0.0,
            dark_mode ? 0.06 : 0.88,
            dark_mode ? 0.06 : 0.88,
            dark_mode ? 0.06 : 0.88);
        cairo_pattern_add_color_stop_rgb(bg, 1.0,
            dark_mode ? 0.13 : 0.96,
            dark_mode ? 0.13 : 0.96,
            dark_mode ? 0.13 : 0.96);
    }

    cairo_save(cr);

    /* Clip to the requested half */
    if (half == 0)
        cairo_rectangle(cr, 0, 0, w, h/2.0);
    else
        cairo_rectangle(cr, 0, h/2.0, w, h/2.0);
    cairo_clip(cr);

    /* Card background */
    card_rounded_rect(cr, 1, 1, w-2, h-2, ICON_R);
    cairo_set_source(cr, bg);
    cairo_fill_preserve(cr);

    /* Border */
    cairo_set_source_rgba(cr, 0.4, 0.4, 0.4, alpha);
    cairo_set_line_width(cr, 1.5);
    cairo_stroke(cr);

    /* Pixbuf centred in card */
    if (pb) {
        int iw = gdk_pixbuf_get_width(pb);
        int ih = gdk_pixbuf_get_height(pb);
        double sx = (double)(w - 2*ICON_PAD) / iw;
        double sy = (double)(h - 2*ICON_PAD) / ih;
        double s  = sx < sy ? sx : sy;
        double dx = (w - iw*s) / 2.0;
        double dy = (h - ih*s) / 2.0;

        cairo_translate(cr, dx, dy);
        cairo_scale(cr, s, s);
        gdk_cairo_set_source_pixbuf(cr, pb, 0, 0);
        cairo_paint_with_alpha(cr, alpha);
    }

    cairo_restore(cr);
    cairo_pattern_destroy(bg);
}

/* Centre crease for icon card */
static void fi_draw_crease(cairo_t *cr, int w, int h)
{
    cairo_set_source_rgba(cr, 0.55, 0.55, 0.55, 1.0);
    cairo_set_line_width(cr, 2.0);
    cairo_move_to(cr, ICON_R, h/2.0);
    cairo_line_to(cr, w - ICON_R, h/2.0);
    cairo_stroke(cr);
}

/* Draw callback */
static gboolean flip_icon_draw(GtkWidget *widget,
                                cairo_t   *cr,
                                gpointer   user_data)
{
    FlipIcon *fi = (FlipIcon *)user_data;
    GtkAllocation alloc;
    gtk_widget_get_allocation(widget, &alloc);
    int w = alloc.width;
    int h = alloc.height;

    /* Static bottom half */
    GdkPixbuf *bot_pb = (fi->step > 0) ? fi->next_pb : fi->cur_pb;
    fi_draw_card_half(cr, w, h, bot_pb, 1, 1.0);

    /* Static top half */
    {
        GdkPixbuf *top_pb = (fi->step > FLIP_STEPS/2) ? fi->next_pb : fi->cur_pb;
        fi_draw_card_half(cr, w, h, top_pb, 0, 1.0);
    }

    /* Animated flap */
    if (fi->step > 0) {
        double progress = (double)fi->step / FLIP_STEPS;
        double scale_y, alpha;
        GdkPixbuf *flap_pb;

        if (progress <= 0.5) {
            double t = progress / 0.5;
            scale_y  = 1.0 - t * 0.92;
            alpha    = 1.0 - t * 0.6;
            flap_pb  = fi->cur_pb;
        } else {
            double t = (progress - 0.5) / 0.5;
            scale_y  = t * 0.92 + 0.08;
            alpha    = t * 0.7 + 0.3;
            flap_pb  = fi->next_pb;
        }

        cairo_save(cr);
        cairo_rectangle(cr, 0, 0, w, h/2.0);
        cairo_clip(cr);
        cairo_translate(cr, 0, h/2.0);
        cairo_scale(cr, 1.0, scale_y);
        cairo_translate(cr, 0, -h/2.0);
        fi_draw_card_half(cr, w, h, flap_pb, 0, alpha);
        fi_draw_crease(cr, w, h);
        cairo_restore(cr);
    }

    fi_draw_crease(cr, w, h);
    return FALSE;
}

/* Timer tick */
static gboolean flip_icon_tick(gpointer user_data)
{
    FlipIcon *fi = (FlipIcon *)user_data;
    fi->step++;
    gtk_widget_queue_draw(fi->area);

    if (fi->step >= FLIP_STEPS) {
        if (fi->cur_pb) g_object_unref(fi->cur_pb);
        fi->cur_pb   = fi->next_pb;
        fi->next_pb  = NULL;
        fi->step     = 0;
        fi->timer_id = 0;
        return G_SOURCE_REMOVE;
    }
    return G_SOURCE_CONTINUE;
}

/* Set a new pixbuf — triggers flip animation */
static void flip_icon_set(FlipIcon *fi, GdkPixbuf *pb)
{
    /* If already animating, commit current and restart */
    if (fi->step > 0) {
        if (fi->cur_pb) g_object_unref(fi->cur_pb);
        fi->cur_pb = fi->next_pb;
        fi->next_pb = NULL;
        if (fi->timer_id) {
            g_source_remove(fi->timer_id);
            fi->timer_id = 0;
        }
        fi->step = 0;
    }
    /* Reference the new pixbuf as next */
    if (fi->next_pb) g_object_unref(fi->next_pb);
    fi->next_pb  = pb ? g_object_ref(pb) : NULL;
    fi->step     = 1;
    fi->timer_id = g_timeout_add(FLIP_MS, flip_icon_tick, fi);
    gtk_widget_queue_draw(fi->area);
}

/* Constructor */
static FlipIcon *flip_icon_new(void)
{
    FlipIcon *fi = g_new0(FlipIcon, 1);
    fi->area = gtk_drawing_area_new();
    gtk_widget_set_size_request(fi->area, ICON_W, ICON_H);
    g_signal_connect(fi->area, "draw",
                     G_CALLBACK(flip_icon_draw), fi);
    return fi;
}

/* Up to 5 forecast flip-icon slots (one per day) */
#define MAX_FORECAST 5
static FlipIcon *forecast_icons[MAX_FORECAST];

/* ---------------- DRAW ---------------- */

static gboolean draw_background(
    GtkWidget *widget,
    cairo_t *cr,
    gpointer data)
{
    GtkAllocation allocation;

    gtk_widget_get_allocation(
        widget,
        &allocation
    );

    cairo_set_source_rgba(
        cr,
        dark_mode ? 0.08 : 0.97,
        dark_mode ? 0.08 : 0.97,
        dark_mode ? 0.08 : 0.97,
        dark_mode ? 0.82 : 0.90
    );

    double radius = 18.0;

    double x = 0.0;
    double y = 0.0;

    double w =
        allocation.width;

    double h =
        allocation.height;

    cairo_new_sub_path(cr);

    cairo_arc(
        cr,
        x + w - radius,
        y + radius,
        radius,
        -90 * G_PI / 180,
        0 * G_PI / 180
    );

    cairo_arc(
        cr,
        x + w - radius,
        y + h - radius,
        radius,
        0 * G_PI / 180,
        90 * G_PI / 180
    );

    cairo_arc(
        cr,
        x + radius,
        y + h - radius,
        radius,
        90 * G_PI / 180,
        180 * G_PI / 180
    );

    cairo_arc(
        cr,
        x + radius,
        y + radius,
        radius,
        180 * G_PI / 180,
        270 * G_PI / 180
    );

    cairo_close_path(cr);

    cairo_fill(cr);

    return FALSE;
}

/* ---------------- CLOCK ---------------- */

static gboolean update_clock(
    gpointer data)
{
    time_t now =
        time(NULL);

    struct tm *tm_info =
        localtime(&now);


    char buffer[6];
    strftime(buffer, sizeof(buffer), "%H%M", tm_info);

    flip_digit_set(fd_h1, buffer[0]);
    flip_digit_set(fd_h2, buffer[1]);
    flip_digit_set(fd_m1, buffer[2]);
    flip_digit_set(fd_m2, buffer[3]);

    return TRUE;
}

/* ---------------- CLEAR BOX ---------------- */

static void clear_box(
    GtkWidget *box)
{
    GList *children =
        gtk_container_get_children(
            GTK_CONTAINER(box)
        );

    for (GList *iter = children;
         iter != NULL;
         iter = g_list_next(iter))
    {
        gtk_widget_destroy(
            GTK_WIDGET(iter->data)
        );
    }

    g_list_free(children);
}

/* ---------------- WEEKDAY ---------------- */

static void get_weekday(
    int offset,
    char *buf,
    size_t len)
{
    time_t now =
        time(NULL) +
        offset * 86400;

    struct tm *tm_info =
        localtime(&now);

    strftime(
        buf,
        len,
        "%a",
        tm_info
    );
}

/* ---------------- FETCH WEATHER ---------------- */

static char *fetch_weather(void)
{
    char url[4096];

    snprintf(
        url,
        sizeof(url),
        "https://api.open-meteo.com/v1/forecast"
        "?latitude=%s"
        "&longitude=%s"
        "&current=temperature_2m,weather_code"
        "&daily=weather_code,temperature_2m_max,temperature_2m_min"
        "&timezone=auto",
        latitude,
        longitude
    );

    return http_get(url);
}

/* ---------------- WEATHER UPDATE ---------------- */

static void update_weather(void)
{
    char *response =
        fetch_weather();

    if (!response)
    {
        gtk_label_set_text(
            GTK_LABEL(current_label),
            "Network error"
        );

        return;
    }

    struct json_object *root =
        json_tokener_parse(response);

    if (!root)
    {
        gtk_label_set_text(
            GTK_LABEL(current_label),
            "JSON error"
        );

        free(response);

        return;
    }

    struct json_object *current = NULL;

    if (!json_object_object_get_ex(
            root,
            "current",
            &current))
    {
        gtk_label_set_text(
            GTK_LABEL(current_label),
            "No current data"
        );

        json_object_put(root);

        free(response);

        return;
    }

    struct json_object *temp_obj = NULL;
    struct json_object *code_obj = NULL;

    json_object_object_get_ex(
        current,
        "temperature_2m",
        &temp_obj
    );

    json_object_object_get_ex(
        current,
        "weather_code",
        &code_obj
    );

    if (!temp_obj || !code_obj)
    {
        gtk_label_set_text(
            GTK_LABEL(current_label),
            "Invalid weather data"
        );

        json_object_put(root);

        free(response);

        return;
    }

    double temp =
        json_object_get_double(
            temp_obj
        );

    int code =
        json_object_get_int(
            code_obj
        );

    const char *desc =
        get_weather_desc(code);

    const char *icon_file =
        get_weather_icon_file(code);

    GdkPixbuf *pb =
        load_scaled_pixbuf(
            icon_file,
            75
        );

    if (pb)
    {
        gtk_image_set_from_pixbuf(
            GTK_IMAGE(weather_icon_image),
            pb
        );

        g_object_unref(pb);
    }

    char current_text[128];

    snprintf(
        current_text,
        sizeof(current_text),
        "%.1f°C %s",
        temp,
        desc
    );

    gtk_label_set_text(
        GTK_LABEL(current_label),
        current_text
    );

    struct json_object *daily = NULL;

    if (!json_object_object_get_ex(
            root,
            "daily",
            &daily))
    {
        json_object_put(root);

        free(response);

        return;
    }

    struct json_object *tmax = NULL;
    struct json_object *tmin = NULL;
    struct json_object *codes = NULL;

    json_object_object_get_ex(
        daily,
        "temperature_2m_max",
        &tmax
    );

    json_object_object_get_ex(
        daily,
        "temperature_2m_min",
        &tmin
    );

    json_object_object_get_ex(
        daily,
        "weather_code",
        &codes
    );

    clear_box(day_row);
    clear_box(night_row);
    clear_box(weekday_row);

    int count =
        json_object_array_length(
            tmax
        );

    if (count > 5)
        count = 5;

    for (int i = 0;
         i < count;
         i++)
    {
        double max =
            json_object_get_double(
                json_object_array_get_idx(
                    tmax,
                    i
                )
            );

        double min =
            json_object_get_double(
                json_object_array_get_idx(
                    tmin,
                    i
                )
            );

        int fcode =
            json_object_get_int(
                json_object_array_get_idx(
                    codes,
                    i
                )
            );

        char weekday[32];

        get_weekday(
            i,
            weekday,
            sizeof(weekday)
        );

        GtkWidget *weekday_lbl =
            gtk_label_new(
                weekday
            );

        gtk_box_pack_start(
            GTK_BOX(weekday_row),
            weekday_lbl,
            TRUE,
            TRUE,
            2
        );

        const char *ficon_file =
            get_weather_icon_file(
                fcode
            );

        GdkPixbuf *fpb =
            load_scaled_pixbuf(
                ficon_file,
                40
            );

        if (forecast_icons[i]) {
            flip_icon_set(forecast_icons[i], fpb);
        }

        if (fpb)
            g_object_unref(fpb);

        char maxbuf[32];

        snprintf(
            maxbuf,
            sizeof(maxbuf),
            "%.0f°",
            max
        );

        GtkWidget *day_lbl =
            gtk_label_new(maxbuf);

        gtk_widget_set_name(
            day_lbl,
            "forecast_temp"
        );

        gtk_box_pack_start(
            GTK_BOX(day_row),
            day_lbl,
            TRUE,
            TRUE,
            4
        );

        char minbuf[32];

        snprintf(
            minbuf,
            sizeof(minbuf),
            "%.0f°",
            min
        );

        GtkWidget *night_lbl =
            gtk_label_new(minbuf);

        gtk_widget_set_name(
            night_lbl,
            "forecast_temp"
        );

        gtk_box_pack_start(
            GTK_BOX(night_row),
            night_lbl,
            TRUE,
            TRUE,
            4
        );
    }

    gtk_widget_show_all(window);

    json_object_put(root);

    free(response);
}

/* ---------------- TIMER ---------------- */

static gboolean weather_timer(
    gpointer data)
{
    update_weather();

    return TRUE;
}

static void restart_weather_timer(void)
{
    if (weather_timer_id)
        g_source_remove(
            weather_timer_id
        );

    weather_timer_id =
        g_timeout_add_seconds(
            refresh_interval,
            weather_timer,
            NULL
        );
}

/* ---------------- SETTINGS ---------------- */

static void settings_cb(
    GtkMenuItem *item,
    gpointer data)
{
    GtkWidget *dialog =
        gtk_dialog_new_with_buttons(
            "Settings",
            GTK_WINDOW(window),
            GTK_DIALOG_MODAL,
            "_Cancel",
            GTK_RESPONSE_CANCEL,
            "_Save",
            GTK_RESPONSE_ACCEPT,
            NULL
        );

    GtkWidget *content =
        gtk_dialog_get_content_area(
            GTK_DIALOG(dialog)
        );

    GtkWidget *grid =
        gtk_grid_new();

    gtk_grid_set_row_spacing(
        GTK_GRID(grid),
        8
    );

    gtk_grid_set_column_spacing(
        GTK_GRID(grid),
        8
    );

    GtkWidget *city_entry =
        gtk_entry_new();

    GtkAdjustment *lat_adj =
        gtk_adjustment_new(
            g_ascii_strtod(latitude, NULL),
            -90,
            90,
            0.0001,
            1,
            0
        );

    GtkAdjustment *lon_adj =
        gtk_adjustment_new(
            g_ascii_strtod(longitude, NULL),
            -180,
            180,
            0.0001,
            1,
            0
        );

    GtkWidget *lat_spin =
        gtk_spin_button_new(
            lat_adj,
            0.0001,
            4
        );

    GtkWidget *lon_spin =
        gtk_spin_button_new(
            lon_adj,
            0.0001,
            4
        );

    GtkAdjustment *refresh_adj =
        gtk_adjustment_new(
            refresh_interval,
            30,
            7200,
            30,
            60,
            0
        );

    GtkWidget *refresh_spin =
        gtk_spin_button_new(
            refresh_adj,
            1,
            0
        );

    GtkWidget *auto_btn =
        gtk_check_button_new_with_label(
            "Auto detect location"
        );

    GtkWidget *dark_btn =
        gtk_check_button_new_with_label(
            "Dark mode"
        );

    gtk_toggle_button_set_active(
        GTK_TOGGLE_BUTTON(dark_btn),
        dark_mode
    );

    gtk_grid_attach(
        GTK_GRID(grid),
        gtk_label_new("City"),
        0,0,1,1
    );

    gtk_grid_attach(
        GTK_GRID(grid),
        city_entry,
        1,0,1,1
    );

    gtk_grid_attach(
        GTK_GRID(grid),
        gtk_label_new("Latitude"),
        0,1,1,1
    );

    gtk_grid_attach(
        GTK_GRID(grid),
        lat_spin,
        1,1,1,1
    );

    gtk_grid_attach(
        GTK_GRID(grid),
        gtk_label_new("Longitude"),
        0,2,1,1
    );

    gtk_grid_attach(
        GTK_GRID(grid),
        lon_spin,
        1,2,1,1
    );

    gtk_grid_attach(
        GTK_GRID(grid),
        gtk_label_new("Refresh (sec)"),
        0,3,1,1
    );

    gtk_grid_attach(
        GTK_GRID(grid),
        refresh_spin,
        1,3,1,1
    );

    gtk_grid_attach(
        GTK_GRID(grid),
        auto_btn,
        0,4,2,1
    );

    gtk_grid_attach(
        GTK_GRID(grid),
        dark_btn,
        0,5,2,1
    );

    gtk_container_add(
        GTK_CONTAINER(content),
        grid
    );

    gtk_widget_show_all(dialog);

    gint result =
        gtk_dialog_run(
            GTK_DIALOG(dialog)
        );

    if (result ==
        GTK_RESPONSE_ACCEPT)
    {
        const char *city =
            gtk_entry_get_text(
                GTK_ENTRY(city_entry)
            );

        if (strlen(city) > 0)
        {
            geocode_city(city);
        }
        else if (
            gtk_toggle_button_get_active(
                GTK_TOGGLE_BUTTON(auto_btn)
            ))
        {
            autodetect_location();
        }
        else
        {
            g_ascii_formatd(
                latitude,
                sizeof(latitude),
                "%.4f",
                gtk_spin_button_get_value(
                    GTK_SPIN_BUTTON(lat_spin)
                )
            );

            g_ascii_formatd(
                longitude,
                sizeof(longitude),
                "%.4f",
                gtk_spin_button_get_value(
                    GTK_SPIN_BUTTON(lon_spin)
                )
            );
        }

        refresh_interval =
            gtk_spin_button_get_value_as_int(
                GTK_SPIN_BUTTON(refresh_spin)
            );

        dark_mode =
            gtk_toggle_button_get_active(
                GTK_TOGGLE_BUTTON(dark_btn)
            );

        save_config();

        apply_theme();

        restart_weather_timer();

        update_weather();
    }

    gtk_widget_destroy(dialog);
}

/* ---------------- EVENTS ---------------- */

static gboolean on_configure(
    GtkWidget *widget,
    GdkEventConfigure *event,
    gpointer data)
{
    save_position();

    return FALSE;
}

static gboolean on_delete_event(
    GtkWidget *widget,
    GdkEvent *event,
    gpointer data)
{
    save_position();

    gtk_widget_hide(widget);

    return TRUE;
}

static gboolean on_button_press(
    GtkWidget *widget,
    GdkEventButton *event,
    gpointer data)
{
    if (event->button == 1 &&
        (event->state &
         GDK_MOD1_MASK))
    {
        gtk_window_begin_move_drag(
            GTK_WINDOW(window),
            event->button,
            event->x_root,
            event->y_root,
            event->time
        );

        return TRUE;
    }

    return FALSE;
}

/* ---------------- MAIN ---------------- */

int main(
    int argc,
    char **argv)
{
    gtk_init(&argc, &argv);

    curl_global_init(
        CURL_GLOBAL_DEFAULT
    );

    load_config();

    apply_theme();

    window =
        gtk_window_new(
            GTK_WINDOW_TOPLEVEL
        );

    gtk_window_set_title(
        GTK_WINDOW(window),
        "Weather Widget"
    );

    gtk_window_set_default_size(
        GTK_WINDOW(window),
        300,
        300
    );

    gtk_window_set_decorated(
        GTK_WINDOW(window),
        FALSE
    );

    gtk_window_set_resizable(
        GTK_WINDOW(window),
        FALSE
    );

    gtk_window_stick(
        GTK_WINDOW(window)
    );

    gtk_window_set_keep_above(
        GTK_WINDOW(window),
        FALSE
    );

    gtk_widget_set_app_paintable(
        window,
        TRUE
    );

    GdkScreen *screen =
        gtk_widget_get_screen(
            window
        );

    GdkVisual *visual =
        gdk_screen_get_rgba_visual(
            screen
        );

    if (visual &&
        gdk_screen_is_composited(
            screen
        ))
    {
        gtk_widget_set_visual(
            window,
            visual
        );
    }

    g_signal_connect(
        window,
        "draw",
        G_CALLBACK(draw_background),
        NULL
    );

    gtk_widget_add_events(
        window,
        GDK_BUTTON_PRESS_MASK
    );

    g_signal_connect(
        window,
        "button-press-event",
        G_CALLBACK(on_button_press),
        NULL
    );

    g_signal_connect(
        window,
        "configure-event",
        G_CALLBACK(on_configure),
        NULL
    );

    g_signal_connect(
        window,
        "delete-event",
        G_CALLBACK(on_delete_event),
        NULL
    );

    indicator =
        app_indicator_new(
            APPINDICATOR_ID,
            "weather-clear",
            APP_INDICATOR_CATEGORY_APPLICATION_STATUS
        );

    app_indicator_set_status(
        indicator,
        APP_INDICATOR_STATUS_ACTIVE
    );

    GtkWidget *menu =
        gtk_menu_new();

    GtkWidget *open_item =
        gtk_menu_item_new_with_label(
            "Open Widget"
        );

    GtkWidget *settings_item =
        gtk_menu_item_new_with_label(
            "Settings"
        );

    GtkWidget *quit_item =
        gtk_menu_item_new_with_label(
            "Quit"
        );

    gtk_menu_shell_append(
        GTK_MENU_SHELL(menu),
        open_item
    );

    gtk_menu_shell_append(
        GTK_MENU_SHELL(menu),
        settings_item
    );

    gtk_menu_shell_append(
        GTK_MENU_SHELL(menu),
        quit_item
    );

    g_signal_connect_swapped(
        open_item,
        "activate",
        G_CALLBACK(gtk_widget_show_all),
        window
    );

    g_signal_connect(
        settings_item,
        "activate",
        G_CALLBACK(settings_cb),
        NULL
    );

    g_signal_connect_swapped(
        quit_item,
        "activate",
        G_CALLBACK(gtk_main_quit),
        NULL
    );

    gtk_widget_show_all(menu);

    app_indicator_set_menu(
        indicator,
        GTK_MENU(menu)
    );

    GtkWidget *main_box =
        gtk_box_new(
            GTK_ORIENTATION_VERTICAL,
            3
        );

    gtk_container_set_border_width(
        GTK_CONTAINER(main_box),
        6
    );

    gtk_container_add(
        GTK_CONTAINER(window),
        main_box
    );

GtkWidget *clock_box =
    gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);

/* Centre the clock row horizontally */
gtk_widget_set_halign(clock_box, GTK_ALIGN_CENTER);

/* Get current time for initial digits */
{
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char buf[6];
    strftime(buf, sizeof(buf), "%H%M", tm_info);
    fd_h1 = flip_digit_new(buf[0]);
    fd_h2 = flip_digit_new(buf[1]);
    fd_m1 = flip_digit_new(buf[2]);
    fd_m2 = flip_digit_new(buf[3]);
}

colon = gtk_drawing_area_new();
gtk_widget_set_size_request(colon, 16, CARD_H);
g_signal_connect(colon, "draw", G_CALLBACK(colon_draw), NULL);

/* Compatibility: keep h1/h2/m1/m2 pointing somewhere harmless */
h1 = fd_h1->area;
h2 = fd_h2->area;
m1 = fd_m1->area;
m2 = fd_m2->area;

gtk_box_pack_start(GTK_BOX(clock_box), fd_h1->area, FALSE, FALSE, 0);
gtk_box_pack_start(GTK_BOX(clock_box), fd_h2->area, FALSE, FALSE, 0);
gtk_box_pack_start(GTK_BOX(clock_box), colon,       FALSE, FALSE, 0);
gtk_box_pack_start(GTK_BOX(clock_box), fd_m1->area, FALSE, FALSE, 0);
gtk_box_pack_start(GTK_BOX(clock_box), fd_m2->area, FALSE, FALSE, 0);

gtk_box_pack_start(
    GTK_BOX(main_box),
    clock_box,
    FALSE,
    FALSE,
    2
);

    weather_icon_image =
        gtk_image_new();

    gtk_box_pack_start(
        GTK_BOX(main_box),
        weather_icon_image,
        FALSE,
        FALSE,
        1
    );

    current_label =
        gtk_label_new(
            "Loading..."
        );

    gtk_widget_set_name(
        current_label,
        "current_label"
    );

    gtk_box_pack_start(
        GTK_BOX(main_box),
        current_label,
        FALSE,
        FALSE,
        1
    );

    GtkWidget *separator =
        gtk_separator_new(
            GTK_ORIENTATION_HORIZONTAL
        );

    gtk_box_pack_start(
        GTK_BOX(main_box),
        separator,
        FALSE,
        FALSE,
        3
    );

    weekday_row =
        gtk_box_new(
            GTK_ORIENTATION_HORIZONTAL,
            2
        );

    gtk_box_pack_start(
        GTK_BOX(main_box),
        weekday_row,
        FALSE,
        FALSE,
        1
    );

    icon_row =
        gtk_box_new(
            GTK_ORIENTATION_HORIZONTAL,
            2
        );

    /* Pre-allocate one FlipIcon per forecast slot */
    for (int i = 0; i < MAX_FORECAST; i++) {
        forecast_icons[i] = flip_icon_new();
        gtk_box_pack_start(
            GTK_BOX(icon_row),
            forecast_icons[i]->area,
            TRUE,
            TRUE,
            2
        );
    }

    gtk_box_pack_start(
        GTK_BOX(main_box),
        icon_row,
        FALSE,
        FALSE,
        1
    );

    day_row =
        gtk_box_new(
            GTK_ORIENTATION_HORIZONTAL,
            2
        );

    gtk_box_pack_start(
        GTK_BOX(main_box),
        day_row,
        FALSE,
        FALSE,
        1
    );

    night_row =
        gtk_box_new(
            GTK_ORIENTATION_HORIZONTAL,
            2
        );

    gtk_box_pack_start(
        GTK_BOX(main_box),
        night_row,
        FALSE,
        FALSE,
        1
    );

    load_position();

    update_weather();

    update_clock(NULL);

    restart_weather_timer();

    g_timeout_add_seconds(
        1,
        update_clock,
        NULL
    );

    gtk_widget_show_all(window);

    gtk_main();

    curl_global_cleanup();

    return 0;
}
