
#include "lv2/lv2plug.in/ns/lv2core/lv2.h"
#include "lv2/lv2plug.in/ns/extensions/ui/ui.h"

// xwidgets.h includes xputty.h and all defined widgets from Xputty
#include "xwidgets.h"


#include "profiler.h"

/*---------------------------------------------------------------------
-----------------------------------------------------------------------    
                define controller numbers
-----------------------------------------------------------------------
----------------------------------------------------------------------*/

#define CONTROLS 4

/*---------------------------------------------------------------------
-----------------------------------------------------------------------    
                the main LV2 handle->XWindow
-----------------------------------------------------------------------
----------------------------------------------------------------------*/


// main window struct
typedef struct {
    void *parentXwindow;
    Xputty main;
    Widget_t *win;
    Widget_t *widget[CONTROLS];
    int block_event;
    char tolltiptext[250];

    void *controller;
    LV2UI_Write_Function write_function;
    LV2UI_Resize* resize;
} X11_UI;

static void box_shadow(Widget_t *w) {
    XWindowAttributes attrs;
    XGetWindowAttributes(w->app->dpy, (Window)w->widget, &attrs);
    int width = attrs.width;
    int height = attrs.height;
    if (w->state == 3) {
        cairo_pattern_t *pat = cairo_pattern_create_linear (0, 0, width, 0);
        cairo_pattern_add_color_stop_rgba (pat, 1,  0.33, 0.33, 0.33, 1.0);
        cairo_pattern_add_color_stop_rgba (pat, 0.95,  0.2, 0.2, 0.2, 0.0);
        cairo_pattern_add_color_stop_rgba (pat, 0.1,  0.1, 0.1, 0.1, 0.0);
        cairo_pattern_add_color_stop_rgba (pat, 0,  0.05, 0.05, 0.05, 1.0);
        cairo_set_source(w->crb, pat);
        cairo_paint (w->crb);
        cairo_pattern_destroy (pat);
        pat = NULL;
        pat = cairo_pattern_create_linear (0, 0, 0, height);
        cairo_pattern_add_color_stop_rgba (pat, 1,  0.33, 0.33, 0.33, 1.0);
        cairo_pattern_add_color_stop_rgba (pat, 0.9,  0.2, 0.2, 0.2, 0.0);
        cairo_pattern_add_color_stop_rgba (pat, 0.1,  0.1, 0.1, 0.1, 0.0);
        cairo_pattern_add_color_stop_rgba (pat, 0,  0.05, 0.05, 0.05, 1.0);
        cairo_set_source(w->crb, pat);
        cairo_paint (w->crb);
        cairo_pattern_destroy (pat);
    } else {
        cairo_pattern_t *pat = cairo_pattern_create_linear (0, 0, width, 0);
        cairo_pattern_add_color_stop_rgba (pat, 0,  0.33, 0.33, 0.33, 1.0);
        cairo_pattern_add_color_stop_rgba (pat, 0.1,  0.2, 0.2, 0.2, 0.0);
        cairo_pattern_add_color_stop_rgba (pat, 0.9,  0.1, 0.1, 0.1, 0.0);
        cairo_pattern_add_color_stop_rgba (pat, 1,  0.05, 0.05, 0.05, 1.0);
        cairo_set_source(w->crb, pat);
        cairo_paint (w->crb);
        cairo_pattern_destroy (pat);
        pat = NULL;
        pat = cairo_pattern_create_linear (0, 0, 0, height);
        cairo_pattern_add_color_stop_rgba (pat, 0,  0.33, 0.33, 0.33, 1.0);
        cairo_pattern_add_color_stop_rgba (pat, 0.1,  0.2, 0.2, 0.2, 0.0);
        cairo_pattern_add_color_stop_rgba (pat, 0.9,  0.1, 0.1, 0.1, 0.0);
        cairo_pattern_add_color_stop_rgba (pat, 1,  0.05, 0.05, 0.05, 1.0);
        cairo_set_source(w->crb, pat);
        cairo_paint (w->crb);
        cairo_pattern_destroy (pat);
    }
}

// draw the window
static void draw_window(void *w_, void* user_data) {
    Widget_t *w = (Widget_t*)w_;
    set_pattern(w,&w->app->color_scheme->selected,&w->app->color_scheme->normal,BACKGROUND_);
    cairo_paint (w->crb);
    box_shadow(w);
}

static void draw_my_button(void *w_, void* user_data) {
    Widget_t *w = (Widget_t*)w_;
    if (!w) return;
    XWindowAttributes attrs;
    XGetWindowAttributes(w->app->dpy, (Window)w->widget, &attrs);
    int width = attrs.width-2;
    int height = attrs.height-2;
    if (attrs.map_state != IsViewable) return;

    if (!w->state && (int)w->adj_y->value)
        w->state = 3;
    if (w->state == 3 && !(int)w->adj_y->value)
        w->state = 0;

    cairo_rectangle(w->crb,2.0, 2.0, width, height);

    if(w->state==0) {
        cairo_set_line_width(w->crb, 1.0);
        cairo_fill_preserve(w->crb);
        use_bg_color_scheme(w, PRELIGHT_);
    } else if(w->state==1) {
        use_shadow_color_scheme(w, PRELIGHT_);
        cairo_fill_preserve(w->crb);
        cairo_set_line_width(w->crb, 1.5);
        use_bg_color_scheme(w, PRELIGHT_);
    } else if(w->state==2) {
        use_bg_color_scheme(w, SELECTED_);
        cairo_fill_preserve(w->crb);
        cairo_set_line_width(w->crb, 1.0);
        use_bg_color_scheme(w, PRELIGHT_);
    } else if(w->state==3) {
        use_bg_color_scheme(w, ACTIVE_);
        cairo_fill_preserve(w->crb);
        cairo_set_line_width(w->crb, 1.0);
        use_bg_color_scheme(w, PRELIGHT_);
    }
    cairo_stroke(w->crb); 

    if(w->state==2) {
        cairo_rectangle(w->crb,4.0, 4.0, width, height);
        cairo_stroke(w->crb);
        cairo_rectangle(w->crb,3.0, 3.0, width, height);
        cairo_stroke(w->crb);
    } else if (w->state==3) {
        cairo_rectangle(w->crb,3.0, 3.0, width, height);
        cairo_stroke(w->crb);
    }

    float offset = 0.0;
    cairo_text_extents_t extents;
    if(w->state==0) {
        use_fg_color_scheme(w, NORMAL_);
    } else if(w->state==1 && ! (int)w->adj_y->value) {
        use_fg_color_scheme(w, PRELIGHT_);
        offset = 1.0;
    } else if(w->state==1) {
        use_fg_color_scheme(w, ACTIVE_);
        offset = 2.0;
    } else if(w->state==2) {
        use_fg_color_scheme(w, SELECTED_);
        offset = 2.0;
    } else if(w->state==3) {
        use_fg_color_scheme(w, ACTIVE_);
        offset = 1.0;
    }
    box_shadow(w);

    use_text_color_scheme(w, get_color_state(w));
    cairo_set_font_size (w->crb, height/2.2);
    cairo_select_font_face (w->crb, "Sans", CAIRO_FONT_SLANT_NORMAL,
                               CAIRO_FONT_WEIGHT_BOLD);
    cairo_text_extents(w->crb,w->label , &extents);

    cairo_move_to (w->crb, (width-extents.width)*0.5 +offset, (height+extents.height)*0.5 +offset);
    cairo_show_text(w->crb, w->label);
    cairo_new_path (w->crb);
}

void draw_my_hslider(void *w_, void* user_data) {
    Widget_t *w = (Widget_t*)w_;
    XWindowAttributes attrs;
    XGetWindowAttributes(w->app->dpy, (Window)w->widget, &attrs);
    int width = attrs.width-2;
    int height = attrs.height-2;
    
    if (attrs.map_state != IsViewable) return;

    use_bg_color_scheme(w, PRELIGHT_);
    cairo_rectangle(w->crb,2, 2, width, height);
    cairo_set_line_width(w->crb,2);
    cairo_stroke(w->crb);

    float sliderstate = adj_get_state(w->adj_x);
    use_bg_color_scheme(w, ACTIVE_);
    cairo_rectangle(w->crb,4, 4, (width-4) * sliderstate, height-4);
    cairo_fill(w->crb);

    cairo_text_extents_t extents;
    cairo_set_font_size (w->crb, w->app->normal_font/w->scale.ascale);
    char s[64];
    float value = adj_get_value(w->adj);
    snprintf(s, 63,"%d%%",  (int) (value * 100.0));
    cairo_text_extents(w->crb,s , &extents);
    cairo_move_to (w->crb, width/2-extents.width/2,  height/2 + extents.height/2 );
    use_fg_color_scheme(w, NORMAL_);
    cairo_show_text(w->crb, s);
    cairo_new_path (w->crb);

}

// if controller value changed send notify to host
static void value_changed(void *w_, void* user_data) {
    Widget_t *w = (Widget_t*)w_;
    X11_UI* ui = (X11_UI*)w->parent_struct;
    ui->write_function(ui->controller,w->data,sizeof(float),0,&w->adj->value);
}

static void show_path(void *w_, void* user_data) {
    Widget_t *w = (Widget_t*)w_;
    X11_UI* ui = (X11_UI*)w->parent_struct;
    memset(ui->tolltiptext, 0, 250 * (sizeof ui->tolltiptext[0]));
    const char *Path = NULL;
    strcpy(ui->tolltiptext,"Capture to: ");
    if ((Path == NULL) || (strcmp(Path,"profile") == 0)) {
        Path = getenv("HOME");
        strncat(ui->tolltiptext, Path, 249 - strlen(ui->tolltiptext));
        strncat(ui->tolltiptext,"/profile/", 249 - strlen(ui->tolltiptext));
    }
    Path = ui->tolltiptext;
    tooltip_set_text(w, Path);
    w->state = 1;
    expose_widget(w);
}

void dummy1_callback(void *w_, void* _data, void* user_data) {
    
}

void dummy_callback(void *w_, void* user_data) {
    
}

// init the xwindow and return the LV2UI handle
static LV2UI_Handle instantiate(const LV2UI_Descriptor * descriptor,
            const char * plugin_uri, const char * bundle_path,
            LV2UI_Write_Function write_function,
            LV2UI_Controller controller, LV2UI_Widget * widget,
            const LV2_Feature * const * features) {

    X11_UI* ui = (X11_UI*)malloc(sizeof(X11_UI));

    if (!ui) {
        fprintf(stderr,"ERROR: failed to instantiate plugin with URI %s\n", plugin_uri);
        return NULL;
    }

    ui->parentXwindow = 0;
    LV2UI_Resize* resize = NULL;
    ui->block_event = -1;

    int i = 0;
    for (; features[i]; ++i) {
        if (!strcmp(features[i]->URI, LV2_UI__parent)) {
            ui->parentXwindow = features[i]->data;
        } else if (!strcmp(features[i]->URI, LV2_UI__resize)) {
            resize = (LV2UI_Resize*)features[i]->data;
        }
    }

    if (ui->parentXwindow == NULL)  {
        fprintf(stderr, "ERROR: Failed to open parentXwindow for %s\n", plugin_uri);
        free(ui);
        return NULL;
    }
    // init Xputty
    main_init(&ui->main);
    //set_colors(&ui->main);
    // create the toplevel Window on the parentXwindow provided by the host
    ui->win = create_window(&ui->main, (Window)ui->parentXwindow, 0, 0, 338, 180);
    // connect the expose func
    ui->win->func.expose_callback = draw_window;
    memset(ui->tolltiptext, 0, 250 * (sizeof ui->tolltiptext[0]));
    // create a toggle button
    ui->widget[0] = add_toggle_button(ui->win, "Capture", 60, 25, 220, 30);
    // set resize mode for the toggle button to CENTER ratio
    ui->widget[0]->scale.gravity = CENTER;
    // add tooltip widget to show path were recorded files were saved
    ui->widget[0]->flags |=HAS_TOOLTIP;
    add_tooltip(ui->widget[0], "Capture to: ");
    // store the Port Index in the Widget_t data field
    ui->widget[0]->data = PROFILE;
    // store a pointer to the X11_UI struct in the parent_struct Widget_t field
    ui->widget[0]->parent_struct = ui;
    // connect the value changed callback with the write_function
    ui->widget[0]->func.value_changed_callback = value_changed;
    // connect the expose func
    ui->widget[0]->func.expose_callback = draw_my_button;
    // connect the enter func
    ui->widget[0]->func.enter_callback = show_path;
    // create a toggle button
    ui->widget[1] = add_toggle_button(ui->win, "", 15, 18, 25, 20);
    // store the Port Index in the Widget_t data field
    ui->widget[1]->data = CLIP;
    // set adjustment to be of type CL_METER
    set_adjustment(ui->widget[1]->adj,0.0, 0.0, 0.0, 1.0, 1.0, CL_METER);
    ui->widget[1]->func.expose_callback = draw_my_button;
    // disable unwanted callbacks
    ui->widget[1]->func.enter_callback = dummy_callback;
    ui->widget[1]->func.leave_callback = dummy_callback;
    ui->widget[1]->func.button_press_callback = dummy1_callback;
    ui->widget[1]->func.button_release_callback = dummy1_callback;
    // store a pointer to the X11_UI struct in the parent_struct Widget_t field
    ui->widget[1]->parent_struct = ui;
    // create a meter widget
    ui->widget[2] = add_hmeter(ui->win, "Meter", true, 60, 135, 220, 10);
    // store the port index in the Widget_t data field
    ui->widget[2]->data = METER;
    // set resize mode for the toggle button to CENTER ratio
    ui->widget[2]->scale.gravity = CENTER;
    // fetch the meter scale widget and set gravity to CENTER ratio
    int a = childlist_find_child(ui->main.childlist, ui->widget[2]);
    ui->main.childlist->childs[a+1]->scale.gravity = CENTER;


    ui->widget[3] = add_hslider(ui->win, "STATE", 60, 80, 220, 30);
    set_adjustment(ui->widget[3]->adj, 0.0, 0.0, 0.0, 1.0, 0.001, CL_CONTINUOS);
    ui->widget[3]->data = STATE;
    // set resize mode for the toggle button to CENTER ratio
    ui->widget[3]->scale.gravity = CENTER;
    ui->widget[3]->func.expose_callback = draw_my_hslider;
    
    // finally map all Widgets on screen
    widget_show_all(ui->win);
    // set the widget pointer to the X11 Window from the toplevel Widget_t
    *widget = (void*)ui->win->widget;
    // request to resize the parentXwindow to the size of the toplevel Widget_t
    if (resize){
        ui->resize = resize;
        resize->ui_resize(resize->handle, 380, 280);
    }
    // store pointer to the host controller
    ui->controller = controller;
    // store pointer to the host write function
    ui->write_function = write_function;
    
    return (LV2UI_Handle)ui;
}

// cleanup after usage
static void cleanup(LV2UI_Handle handle) {
    X11_UI* ui = (X11_UI*)handle;
    // Xputty free all memory used
    main_quit(&ui->main);
    free(ui);
}

/*---------------------------------------------------------------------
-----------------------------------------------------------------------    
                        LV2 interface
-----------------------------------------------------------------------
----------------------------------------------------------------------*/

// port value change message from host
static void port_event(LV2UI_Handle handle, uint32_t port_index,
                        uint32_t buffer_size, uint32_t format,
                        const void * buffer) {
    X11_UI* ui = (X11_UI*)handle;
    float value = *(float*)buffer;
    int i=0;
    for (;i<CONTROLS;i++) {
        if (port_index == ui->widget[i]->data) {
            // prevent event loop between host and plugin
            xevfunc store = ui->widget[i]->func.value_changed_callback;
            ui->widget[i]->func.value_changed_callback = dummy_callback;
            // case port is METER, convert value to meter deflection
            if (port_index == METER)
                value = power2db(ui->widget[i], value);
            if (port_index == STATE) {
                if (value > 0.99999)
                    adj_set_value(ui->widget[0]->adj, 0.0);
            }
            // Xputty check if the new value differs from the old one
            // and set new one, when needed
            check_value_changed(ui->widget[i]->adj, &value);
            ui->widget[i]->func.value_changed_callback = store;
        }
    }
}

// LV2 idle interface to host
static int ui_idle(LV2UI_Handle handle) {
    X11_UI* ui = (X11_UI*)handle;
    // Xputty event loop setup to run one cycle when called
    run_embedded(&ui->main);
    return 0;
}

// LV2 resize interface to host
static int ui_resize(LV2UI_Feature_Handle handle, int w, int h) {
    X11_UI* ui = (X11_UI*)handle;
    // Xputty sends configure event to the toplevel widget to resize itself
    if (ui) send_configure_event(ui->win,0, 0, w, h);
    return 0;
}

// connect idle and resize functions to host
static const void* extension_data(const char* uri) {
    static const LV2UI_Idle_Interface idle = { ui_idle };
    static const LV2UI_Resize resize = { 0 ,ui_resize };
    if (!strcmp(uri, LV2_UI__idleInterface)) {
        return &idle;
    }
    if (!strcmp(uri, LV2_UI__resize)) {
        return &resize;
    }
    return NULL;
}

static const LV2UI_Descriptor descriptor = {
    SCPLUGIN_UI_URI,
    instantiate,
    cleanup,
    port_event,
    extension_data
};

LV2_SYMBOL_EXPORT
const LV2UI_Descriptor* lv2ui_descriptor(uint32_t index) {
    switch (index) {
        case 0:
            return &descriptor;
        default:
        return NULL;
    }
}

