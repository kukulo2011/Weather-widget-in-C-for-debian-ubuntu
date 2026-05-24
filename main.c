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
/* ---------------- Edit your longitude and lattitude on the below two lines to get accurate weather for your location ---------------- */
#define LAT "50.0755"
#define LON "14.4378"
typedef struct {
    char *memory;
    size_t size;
} Memory;
static AppIndicator *indicator;
static GtkWidget *window;
static GtkWidget *clock_label;
static GtkWidget *weather_icon_image;
static GtkWidget *current_label;
static GtkWidget *icon_row;
static GtkWidget *day_row;
static GtkWidget *night_row;
static GtkWidget *tray_label;

/* ---------------- POSITION SAVE/LOAD ---------------- */
static void save_position(void)
{
    gint x, y;
    gtk_window_get_position(GTK_WINDOW(window), &x, &y);
    gchar *content = g_strdup_printf("%d %d", x, y);
    gchar *path = g_build_filename(g_get_home_dir(), ".weather-widget.pos", NULL);
    g_file_set_contents(path, content, -1, NULL);
    g_free(content);
    g_free(path);
}

static void load_position(void)
{
    gchar *path = g_build_filename(g_get_home_dir(), ".weather-widget.pos", NULL);
    gchar *content = NULL;
    if (g_file_get_contents(path, &content, NULL, NULL)) {
        int x, y;
        if (sscanf(content, "%d %d", &x, &y) == 2) {
            gtk_window_move(GTK_WINDOW(window), x, y);
        }
        g_free(content);
    }
    g_free(path);
}

static gboolean on_configure(GtkWidget *widget,
                             GdkEventConfigure *event,
                             gpointer data)
{
    static gint last_x = -1, last_y = -1;
    if (event->x != last_x || event->y != last_y) {
        last_x = event->x;
        last_y = event->y;
        save_position();
    }
    return FALSE;
}

static gboolean on_delete_event(GtkWidget *widget,
                                GdkEvent *event,
                                gpointer data)
{
    save_position();
    gtk_widget_hide(widget);
    return TRUE;
}

/* ---------------- HTTP ---------------- */
static size_t write_callback(void *contents,
                             size_t size,
                             size_t nmemb,
                             void *userp)
{
    size_t realsize = size * nmemb;
    Memory *mem = (Memory *)userp;
    char *ptr = realloc(
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
static char *fetch_weather(void)
{
    CURL *curl = curl_easy_init();
    if (!curl)
        return NULL;
    Memory chunk;
    chunk.memory = malloc(1);
    chunk.size = 0;
    char url[4096];
    snprintf(
        url,
        sizeof(url),
        "https://api.open-meteo.com/v1/forecast"
        "?latitude=%s"
        "&longitude=%s"
        "&current=temperature_2m,weather_code"
        "&daily=weather_code,"
        "temperature_2m_max,"
        "temperature_2m_min"
        "&timezone=auto",
        LAT,
        LON
    );
    curl_easy_setopt(curl, CURLOPT_URL, url);
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
        "weather-widget/9.0"
    );
    curl_easy_setopt(
        curl,
        CURLOPT_FOLLOWLOCATION,
        1L
    );
    CURLcode res =
        curl_easy_perform(curl);
    if (res != CURLE_OK) {
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
/* ---------------- WEATHER ---------------- */
static const char *get_weather_desc(int code)
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
/* ---------------- COLORFUL ICONS ---------------- */
static const char *get_weather_icon_file(int code)
{
    /*
    Ubuntu/Yaru colorful weather icons
    */
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
/* ---------------- SCALED PIXBUF HELPER ---------------- */
static GdkPixbuf *load_scaled_pixbuf(const char *path, int size)
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
    if (!pb) {
        if (err) {
            fprintf(stderr,
                    "pixbuf error: %s\n",
                    err->message);
            g_error_free(err);
        }
    }
    return pb;
}
/* ---------------- TRANSPARENT BACKGROUND ---------------- */
static gboolean draw_background(GtkWidget *widget,
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
        0.08,
        0.08,
        0.08,
        0.35
    );
    double radius = 18.0;
    double x = 0.0;
    double y = 0.0;
    double w = allocation.width;
    double h = allocation.height;
    cairo_new_sub_path(cr);
    cairo_arc(cr,
              x + w - radius,
              y + radius,
              radius,
              -90 * G_PI / 180,
              0 * G_PI / 180);
    cairo_arc(cr,
              x + w - radius,
              y + h - radius,
              radius,
              0 * G_PI / 180,
              90 * G_PI / 180);
    cairo_arc(cr,
              x + radius,
              y + h - radius,
              radius,
              90 * G_PI / 180,
              180 * G_PI / 180);
    cairo_arc(cr,
              x + radius,
              y + radius,
              radius,
              180 * G_PI / 180,
              270 * G_PI / 180);
    cairo_close_path(cr);
    cairo_fill(cr);
    return FALSE;
}
/* ---------------- CSS ---------------- */
static void load_css(void)
{
    GtkCssProvider *provider =
        gtk_css_provider_new();
    gtk_css_provider_load_from_data(
        provider,
        "window {"
        " background-color: transparent;"
        "}"
        "label {"
        " color: white;"
        "}"
        "#clock_label {"
        " font-size: 22px;"
        " font-weight: bold;"
        " font-family: Monospace;"
        "}"
        "#current_label {"
        " font-size: 15px;"
        " font-weight: bold;"
        "}"
        "#forecast_temp {"
        " font-size: 10px;"
        "}"
        ,
        -1,
        NULL
    );
    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
}
/* ---------------- CLOCK ---------------- */
static gboolean update_clock(gpointer data)
{
    time_t now = time(NULL);
    struct tm *tm_info =
        localtime(&now);
    char buffer[64];
    strftime(
        buffer,
        sizeof(buffer),
        "%H:%M",
        tm_info
    );
    gtk_label_set_text(
        GTK_LABEL(clock_label),
        buffer
    );
    return TRUE;
}
/* ---------------- MOVE WINDOW ---------------- */
static gboolean on_button_press(GtkWidget *widget,
                                GdkEventButton *event,
                                gpointer data)
{
    if (event->button == 1 &&
        (event->state & GDK_MOD1_MASK))
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
/* ---------------- CLEAR BOX ---------------- */
static void clear_box(GtkWidget *box)
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
/* ---------------- WEATHER UPDATE ---------------- */
static void update_weather(void)
{
    char *response =
        fetch_weather();
    if (!response)
        return;
    struct json_object *root =
        json_tokener_parse(response);
    if (!root) {
        free(response);
        return;
    }
    /* CURRENT */
    struct json_object *current = NULL;
    if (!json_object_object_get_ex(
            root,
            "current",
            &current))
    {
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
    if (!temp_obj || !code_obj) {
        json_object_put(root);
        free(response);
        return;
    }
    double temp =
        json_object_get_double(temp_obj);
    int code =
        json_object_get_int(code_obj);
    const char *desc =
        get_weather_desc(code);
    const char *icon_file =
        get_weather_icon_file(code);
    {
        GdkPixbuf *pb =
            load_scaled_pixbuf(icon_file, 48);
        if (pb) {
            gtk_image_set_from_pixbuf(
                GTK_IMAGE(weather_icon_image),
                pb
            );
            g_object_unref(pb);
        } else {
            gtk_image_set_from_file(
                GTK_IMAGE(weather_icon_image),
                icon_file
            );
        }
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
    /* DAILY */
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
    if (!tmax || !tmin || !codes) {
        json_object_put(root);
        free(response);
        return;
    }
    clear_box(icon_row);
    clear_box(day_row);
    clear_box(night_row);
    int count =
        json_object_array_length(tmax);
    if (count > 5)
        count = 5;
    for (int i = 0; i < count; i++) {
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
        const char *ficon_file =
            get_weather_icon_file(fcode);
        /* ICON ROW */
        GtkWidget *icon_img;
        {
            GdkPixbuf *fpb =
                load_scaled_pixbuf(ficon_file, 28);
            if (fpb) {
                icon_img = gtk_image_new_from_pixbuf(fpb);
                g_object_unref(fpb);
            } else {
                icon_img = gtk_image_new_from_file(ficon_file);
            }
        }
        gtk_box_pack_start(
            GTK_BOX(icon_row),
            icon_img,
            TRUE,
            TRUE,
            4
        );
        /* DAY TEMP */
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
        /* NIGHT TEMP */
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
static gboolean weather_timer(gpointer data)
{
    update_weather();
    return TRUE;
}
/* ---------------- MENU ---------------- */
static void show_widget(GtkMenuItem *item,
                        gpointer data)
{
    gtk_widget_show_all(window);
    gtk_window_present(
        GTK_WINDOW(window)
    );
}
static void quit_cb(GtkMenuItem *item,
                    gpointer data)
{
    save_position();
    gtk_main_quit();
}
/* ---------------- MAIN ---------------- */
int main(int argc, char **argv)
{
    gtk_init(&argc, &argv);
    curl_global_init(CURL_GLOBAL_DEFAULT);
    load_css();
    /* WINDOW */
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
        260,
        200
    );
    gtk_window_set_decorated(
        GTK_WINDOW(window),
        FALSE
    );
    gtk_window_set_type_hint(
        GTK_WINDOW(window),
        GDK_WINDOW_TYPE_HINT_NORMAL
    );
    gtk_window_set_resizable(
        GTK_WINDOW(window),
        FALSE
    );
    gtk_widget_set_app_paintable(
        window,
        TRUE
    );
    GdkScreen *screen =
        gtk_widget_get_screen(window);
    GdkVisual *visual =
        gdk_screen_get_rgba_visual(screen);
    if (visual &&
        gdk_screen_is_composited(screen))
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
    /* APPINDICATOR */
    indicator = app_indicator_new(
        APPINDICATOR_ID,
        "weather-clear",
        APP_INDICATOR_CATEGORY_APPLICATION_STATUS
    );
    app_indicator_set_status(
        indicator,
        APP_INDICATOR_STATUS_ACTIVE
    );
    /* MENU */
    GtkWidget *menu =
        gtk_menu_new();
    tray_label =
        gtk_menu_item_new_with_label(
            "Weather"
        );
    GtkWidget *open_item =
        gtk_menu_item_new_with_label(
            "Open Widget"
        );
    GtkWidget *quit_item =
        gtk_menu_item_new_with_label(
            "Quit"
        );
    gtk_menu_shell_append(
        GTK_MENU_SHELL(menu),
        tray_label
    );
    gtk_menu_shell_append(
        GTK_MENU_SHELL(menu),
        open_item
    );
    gtk_menu_shell_append(
        GTK_MENU_SHELL(menu),
        quit_item
    );
    g_signal_connect(
        open_item,
        "activate",
        G_CALLBACK(show_widget),
        NULL
    );
    g_signal_connect(
        quit_item,
        "activate",
        G_CALLBACK(quit_cb),
        NULL
    );
    gtk_widget_show_all(menu);
    app_indicator_set_menu(
        indicator,
        GTK_MENU(menu)
    );
    /* MAIN LAYOUT */
    GtkWidget *main_box =
        gtk_box_new(
            GTK_ORIENTATION_VERTICAL,
            4
        );
    gtk_container_set_border_width(
        GTK_CONTAINER(main_box),
        10
    );
    gtk_container_add(
        GTK_CONTAINER(window),
        main_box
    );
    /* CLOCK */
    clock_label =
        gtk_label_new("");
    gtk_widget_set_name(
        clock_label,
        "clock_label"
    );
    gtk_box_pack_start(
        GTK_BOX(main_box),
        clock_label,
        FALSE,
        FALSE,
        1
    );
    /* MAIN ICON */
    weather_icon_image =
        gtk_image_new();
    gtk_box_pack_start(
        GTK_BOX(main_box),
        weather_icon_image,
        FALSE,
        FALSE,
        1
    );
    /* CURRENT */
    current_label =
        gtk_label_new("Loading...");
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
    /* SEPARATOR */
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
    /* ICON ROW */
    icon_row =
        gtk_box_new(
            GTK_ORIENTATION_HORIZONTAL,
            2
        );
    gtk_box_pack_start(
        GTK_BOX(main_box),
        icon_row,
        FALSE,
        FALSE,
        1
    );
    /* DAY ROW */
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
    /* NIGHT ROW */
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
    /* START */
    load_position();
    update_weather();
    update_clock(NULL);
    g_timeout_add_seconds(
        600,
        weather_timer,
        NULL
    );
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
