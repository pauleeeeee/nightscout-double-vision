#include <pebble.h>
#include <pebble-battery-bar/pebble-battery-bar.h>
#include <pebble-bluetooth-icon/pebble-bluetooth-icon.h>
#include "./nightscout-double-vision.h"

static Window *s_window;
static BluetoothLayer *s_bluetooth_layer;
static BatteryBarLayer *s_battery_layer;
static Layer *s_person_one_holder_layer, *s_person_two_holder_layer, *s_person_one_graph_layer, *s_person_two_graph_layer;
static TextLayer *s_time_layer, *s_background_layer;
static TextLayer *s_p_one_name_text_layer, *s_p_one_sgv_text_layer, *s_p_one_delta_text_layer, *s_p_one_time_ago_text_layer, *s_p_one_iob_text_layer, *s_p_one_battery_text_layer;
static TextLayer *s_p_two_name_text_layer, *s_p_two_sgv_text_layer, *s_p_two_delta_text_layer, *s_p_two_time_ago_text_layer, *s_p_two_iob_text_layer, *s_p_two_battery_text_layer;
static char s_p_one_name_text[24], s_p_one_sgv_text[8], s_p_one_delta_text[8], s_p_one_time_ago_text[24], s_p_one_iob_text[16], s_p_one_battery_text[8];
static char s_p_two_name_text[24], s_p_two_sgv_text[8], s_p_two_delta_text[8], s_p_two_time_ago_text[24], s_p_two_iob_text[16], s_p_two_battery_text[8];
static int s_p_one_ago_int, s_p_two_ago_int;
static GBitmap *s_p_one_icon_bitmap = NULL;
static GBitmap *s_p_two_icon_bitmap = NULL;
static BitmapLayer *s_p_one_icon_layer, *s_p_two_icon_layer;
static int CurrentPerson = 0;
static int s_respect_quiet_time = 0;

//person one graph stuff
static bool s_person_one_graph_should_draw = false;
static uint16_t s_person_one_x_points[20];
static uint16_t s_person_one_y_points[20];

//person two graph stuff
static bool s_person_two_graph_should_draw = false;
static uint16_t s_person_two_x_points[20];
static uint16_t s_person_two_y_points[20];

//generic graph stuff
static int s_graph_high_point = 0;
static int s_graph_low_point = 0;

static void updateTimeAgo(int person_id){
  if (person_id == 0){
    //format integer to string
    snprintf(s_p_one_time_ago_text, sizeof(s_p_one_time_ago_text), "%d", s_p_one_ago_int);
    //concatenate ' min ago' to the end of the formatted int
    strcat(s_p_one_time_ago_text, " min ago");
    text_layer_set_text(s_p_one_time_ago_text_layer, s_p_one_time_ago_text);
  } else if (person_id == 1){
    snprintf(s_p_two_time_ago_text, sizeof(s_p_two_time_ago_text), "%d", s_p_two_ago_int);
    strcat(s_p_two_time_ago_text, " min ago");
    text_layer_set_text(s_p_two_time_ago_text_layer, s_p_two_time_ago_text);
  }
}

static void update_time() {
  // Get a tm structure
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);

  // Write the current hours and minutes into a buffer
  static char s_buffer[16];
  strftime(s_buffer, sizeof(s_buffer), "%l:%M %p", tick_time);

  // Display this time on the TextLayer
  text_layer_set_text(s_time_layer, s_buffer);

  //each minute that goes by... increase ago ints
  s_p_one_ago_int++;
  s_p_two_ago_int++;
  //update timeAgo
  updateTimeAgo(0);
  updateTimeAgo(1);
  //write the int to storage so that accurate values are shown even after leaving and returning to watchface
  persist_write_int(7,s_p_one_ago_int);
  persist_write_int(17,s_p_two_ago_int);

}


static const uint32_t const tripple_segments[] = { 100, 100, 100, 100, 100, 100 };
VibePattern tripple = {
  .durations = tripple_segments,
  .num_segments = ARRAY_LENGTH(tripple_segments),
};

static const uint32_t const quad_segments[] = { 100, 100, 100, 100, 100, 100, 100, 100 };
VibePattern quad = {
  .durations = quad_segments,
  .num_segments = ARRAY_LENGTH(quad_segments),
};

static const uint32_t const sos_segments[] = { 100,30,100,30,100,200,200,30,200,30,200,200,100,30,100,30,100 };
VibePattern sos = {
  .durations = sos_segments,
  .num_segments = ARRAY_LENGTH(sos_segments),
};

static const uint32_t const bum_segments[] = { 150,150,150,150,75,75,150,150,150,150,450 };
VibePattern bum = {
  .durations = bum_segments,
  .num_segments = ARRAY_LENGTH(bum_segments),
};

static void sendAlert(int alert) {
  // APP_LOG(APP_LOG_LEVEL_DEBUG, "switching alert %d", alert);
  switch(alert){
    case 0:
      // APP_LOG(APP_LOG_LEVEL_DEBUG, "no alert %d", alert);
      break;
    case 1:
      // APP_LOG(APP_LOG_LEVEL_DEBUG, "short vibes %d", alert);
      vibes_short_pulse();
      break;
    case 2:
      vibes_long_pulse();
      break;
    case 3:
      vibes_double_pulse();
      break;
    case 4:
      vibes_enqueue_custom_pattern(tripple);
      break;
    case 5:
      vibes_enqueue_custom_pattern(quad);
      break;
    case 6:
      vibes_enqueue_custom_pattern(sos);
      break;
    case 7:
      vibes_enqueue_custom_pattern(bum);
      break;
  }
}

static int tap_count = 0;
// static char display_tap_count_text[8];
// static TextLayer *s_tap_count_layer;

// static AppTimer *timer;

static void tap_timer_callback(){
  tap_count = 0;
  // snprintf(display_tap_count_text, sizeof(display_tap_count_text), "%d", tap_count);
  // text_layer_set_text(s_tap_count_layer, display_tap_count_text);
}

// handle accel event
static void accel_tap_handler(AccelAxisType axis, int32_t direction) {
  if (axis == ACCEL_AXIS_Y && !quiet_time_is_active()){
    if(tap_count == 0) {
      // Schedule the timer
      app_timer_register(2000, tap_timer_callback, NULL);
    }
    tap_count++;
    // snprintf(display_tap_count_text, sizeof(display_tap_count_text), "%d", tap_count);
    // text_layer_set_text(s_tap_count_layer, display_tap_count_text);

    if(tap_count == 2){
      tap_count = 0;
      vibes_short_pulse();
      int ok = 1;
      DictionaryIterator *iter;
      app_message_outbox_begin(&iter);
      dict_write_int(iter, GetGraphs, &ok, sizeof(int), false);
      app_message_outbox_send();
    }
  }

  // vibes_short_pulse();
  // int ok = 1;
  // DictionaryIterator *iter;
  // app_message_outbox_begin(&iter);
  // dict_write_int(iter, GetGraphs, &ok, sizeof(int), false);
  // app_message_outbox_send();


}

//this function is called every time the pebble ticks a minute
static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time();
}

static void person_one_graph_stop_draw_timer_callback(){
  s_person_one_graph_should_draw = false;
  layer_mark_dirty(s_person_one_graph_layer);
}

static void person_two_graph_stop_draw_timer_callback(){
  s_person_two_graph_should_draw = false;
  layer_mark_dirty(s_person_two_graph_layer);
}


//function that receives AppMessages from the JS component
static void in_received_handler(DictionaryIterator *iter, void *context) {
  
  //declare tuples for all known keys. This could be done with better use of looping / arrays / AppSync maybe
  Tuple *person_id_tuple = dict_find (iter, PersonID);
  Tuple *person_name_tuple = dict_find (iter, PersonName);
  Tuple *sgv_tuple = dict_find (iter, SGV);
  Tuple *delta_tuple = dict_find (iter, Delta);
  Tuple *iob_tuple = dict_find (iter, IOB);
  Tuple *battery_tuple = dict_find (iter, Battery);
  Tuple *direction_tuple = dict_find (iter, Direction);
  Tuple *minutes_ago_tuple = dict_find (iter, MinutesAgo);
  Tuple *respect_quiet_time_tuple = dict_find (iter, RespectQuietTime);
  Tuple *send_alert_tuple = dict_find(iter, SendAlert);
  Tuple *graph_high_point_tuple = dict_find(iter, GraphHighPoint);
  Tuple *graph_low_point_tuple = dict_find(iter, GraphLowPoint);
  Tuple *graph_x_points_tuple = dict_find(iter, GraphXPoints);
  Tuple *graph_y_points_tuple = dict_find(iter, GraphYPoints);
  // Tuple *graph_start_stop_tuple = dict_find(iter, GraphStartStop);

  //assign current person to an int. It's important to pipe data from within this function to stable, outside variables. Otherwise data could be lost as a new appmessage comes in
  if (person_id_tuple) {
    CurrentPerson = person_id_tuple->value->int32;
  }

  if (graph_high_point_tuple) {
    s_graph_high_point = graph_high_point_tuple->value->int32;
  }

  if (graph_low_point_tuple) {
    s_graph_low_point = graph_low_point_tuple->value->int32;
  }


  //update values of the watchface. I think making a People array that holds all this stuff would make way more sense.
  if (CurrentPerson == 0){
    // APP_LOG(APP_LOG_LEVEL_DEBUG, "receiving data for person %d", 0);

    if (person_name_tuple) {
      //copy value to outside variable
      strncpy(s_p_one_name_text, person_name_tuple->value->cstring, sizeof(s_p_one_name_text));
      //write the value to memory so that it can be recalled quickly
      persist_write_string(1, s_p_one_name_text);
      //set the text layer text using outside variable
      text_layer_set_text(s_p_one_name_text_layer, s_p_one_name_text);
    }

    if (sgv_tuple) {
      strncpy(s_p_one_sgv_text, sgv_tuple->value->cstring, sizeof(s_p_one_sgv_text));
      persist_write_string(2, s_p_one_sgv_text);
      text_layer_set_text(s_p_one_sgv_text_layer, s_p_one_sgv_text);
    } 

    if (delta_tuple) {
      strncpy(s_p_one_delta_text, delta_tuple->value->cstring, sizeof(s_p_one_delta_text));
      persist_write_string(3, s_p_one_delta_text);
      text_layer_set_text(s_p_one_delta_text_layer, s_p_one_delta_text);
    }

    if (iob_tuple) {
      strncpy(s_p_one_iob_text, iob_tuple->value->cstring, sizeof(s_p_one_iob_text));
      persist_write_string(4, s_p_one_iob_text);
      text_layer_set_text(s_p_one_iob_text_layer, s_p_one_iob_text);
    }

    if (battery_tuple) {
      strncpy(s_p_one_battery_text, battery_tuple->value->cstring, sizeof(s_p_one_battery_text));
      persist_write_string(5, s_p_one_battery_text);
      text_layer_set_text(s_p_one_battery_text_layer, s_p_one_battery_text);
    }

    if (direction_tuple) {
      //write to persistent storage
      persist_write_int(6, direction_tuple->value->int32);
      //if the bitmap exists, destroy it (saving memory)
      if (s_p_one_icon_bitmap) {
        gbitmap_destroy(s_p_one_icon_bitmap);
      }
      //create new bitmap with the value
      s_p_one_icon_bitmap = gbitmap_create_with_resource(ICONS[direction_tuple->value->int32]);
      //set bitmap to layer
      bitmap_layer_set_bitmap(s_p_one_icon_layer, s_p_one_icon_bitmap);
    }

    if (minutes_ago_tuple) {
      //set value to outside variable
      s_p_one_ago_int = minutes_ago_tuple->value->int32;
      //write int to storage
      persist_write_int(7,s_p_one_ago_int);
      //update ago time
      updateTimeAgo(CurrentPerson);
    }

    if (graph_x_points_tuple){
      memcpy(s_person_one_x_points, graph_x_points_tuple->value->data, 40);
    }

    if (graph_y_points_tuple){
      memcpy(s_person_one_y_points, graph_y_points_tuple->value->data, 40);
      s_person_one_graph_should_draw = true;
      layer_mark_dirty(s_person_one_graph_layer);
      app_timer_register(15000, person_one_graph_stop_draw_timer_callback, NULL);
    }


  } else if (CurrentPerson == 1){
    // APP_LOG(APP_LOG_LEVEL_DEBUG, "receiving data for person %d", 1);

    if (person_name_tuple) {
      strncpy(s_p_two_name_text, person_name_tuple->value->cstring, sizeof(s_p_two_name_text));
      persist_write_string(11, s_p_two_name_text);
      text_layer_set_text(s_p_two_name_text_layer, s_p_two_name_text);
    }

    if (sgv_tuple) {
      strncpy(s_p_two_sgv_text, sgv_tuple->value->cstring, sizeof(s_p_two_sgv_text));
      persist_write_string(12, s_p_two_sgv_text);
      text_layer_set_text(s_p_two_sgv_text_layer, s_p_two_sgv_text);
    } 

    if (delta_tuple) {
      strncpy(s_p_two_delta_text, delta_tuple->value->cstring, sizeof(s_p_two_delta_text));
      persist_write_string(13, s_p_two_delta_text);
      text_layer_set_text(s_p_two_delta_text_layer, s_p_two_delta_text);
    }

    if (iob_tuple) {
      strncpy(s_p_two_iob_text, iob_tuple->value->cstring, sizeof(s_p_two_iob_text));
      persist_write_string(14, s_p_two_iob_text);
      text_layer_set_text(s_p_two_iob_text_layer, s_p_two_iob_text);
    }

    if (battery_tuple) {
      strncpy(s_p_two_battery_text, battery_tuple->value->cstring, sizeof(s_p_two_battery_text));
      persist_write_string(15, s_p_two_battery_text);
      text_layer_set_text(s_p_two_battery_text_layer, s_p_two_battery_text);
    }


    if (direction_tuple) {
      persist_write_int(16, direction_tuple->value->int32);
      if (s_p_two_icon_bitmap) {
        gbitmap_destroy(s_p_two_icon_bitmap);
      }
      s_p_two_icon_bitmap = gbitmap_create_with_resource(ICONS[direction_tuple->value->int32]);
      bitmap_layer_set_bitmap(s_p_two_icon_layer, s_p_two_icon_bitmap);
    }

    if (minutes_ago_tuple) {
      s_p_two_ago_int = minutes_ago_tuple->value->int32;
      persist_write_int(17,s_p_two_ago_int);
      updateTimeAgo(CurrentPerson);
    }

    if (graph_x_points_tuple){
      memcpy(s_person_two_x_points, graph_x_points_tuple->value->data, 40);
    }

    if (graph_y_points_tuple){
      memcpy(s_person_two_y_points, graph_y_points_tuple->value->data, 40);
      s_person_two_graph_should_draw = true;
      layer_mark_dirty(s_person_two_graph_layer);
      app_timer_register(15000, person_two_graph_stop_draw_timer_callback, NULL);

    }

  }

  //alerts
  if (respect_quiet_time_tuple) {
    s_respect_quiet_time = respect_quiet_time_tuple->value->int32;
    persist_write_int(52, s_respect_quiet_time);
  }

  if (send_alert_tuple) {

    int alert = send_alert_tuple->value->int32;
    // APP_LOG(APP_LOG_LEVEL_DEBUG, "alert received in tuple %d", alert);
    if (quiet_time_is_active() && s_respect_quiet_time) { 
          // APP_LOG(APP_LOG_LEVEL_DEBUG, "alert suppressed %d", alert);       
      return;
    } else {
      // APP_LOG(APP_LOG_LEVEL_DEBUG, "sending alert to function %d", alert);
      sendAlert(alert);
    }
  }


}


static void in_dropped_handler(AppMessageResult reason, void *context){
  //handle failed message - probably won't happen
}

static void person_one_draw_graph_update_proc(Layer *layer, GContext *ctx){
  // APP_LOG(APP_LOG_LEVEL_DEBUG, "graphing data for person %d", 0);

  GRect bounds = layer_get_bounds(layer);

  graphics_context_set_stroke_color(ctx, GColorDarkGray);

  if (s_person_one_graph_should_draw){
    graphics_context_set_stroke_width(ctx, 1);
    graphics_draw_line(ctx, GPoint(0, s_graph_high_point), GPoint(bounds.size.w, s_graph_high_point));
    graphics_draw_line(ctx, GPoint(0, s_graph_low_point), GPoint(bounds.size.w, s_graph_low_point));
    graphics_context_set_stroke_width(ctx, 3);
    for (int i = 0; i < 20; i++){
      if (i < 19){
        graphics_draw_line(ctx, GPoint(s_person_one_x_points[i], s_person_one_y_points[i]), GPoint(s_person_one_x_points[i+1], s_person_one_y_points[i+1]));
      }
    }
  }

}

static void person_two_draw_graph_update_proc(Layer *layer, GContext *ctx){
  // APP_LOG(APP_LOG_LEVEL_DEBUG, "graphing data for person %d", 1);

  GRect bounds = layer_get_bounds(layer);

  graphics_context_set_stroke_color(ctx, GColorDarkGray);

  if (s_person_two_graph_should_draw){
    graphics_context_set_stroke_width(ctx, 1);
    graphics_draw_line(ctx, GPoint(0, s_graph_high_point), GPoint(bounds.size.w, s_graph_high_point));
    graphics_draw_line(ctx, GPoint(0, s_graph_low_point), GPoint(bounds.size.w, s_graph_low_point));
    graphics_context_set_stroke_width(ctx, 3);
    for (int i = 0; i < 20; i++){
      if (i < 19){
        graphics_draw_line(ctx, GPoint(s_person_two_x_points[i], s_person_two_y_points[i]), GPoint(s_person_two_x_points[i+1], s_person_two_y_points[i+1]));
      }
    }
  }

}

//draws the two white rectangles for each holder layer and the graph
static void person_one_update_proc(Layer *layer, GContext *ctx) {
  //set fill color to white
  graphics_context_set_fill_color(ctx, GColorWhite);
  //define bounds of the rectangle which is passed through from the update_proc assignment
  GRect rectangle = layer_get_bounds(layer);
  //create filled rectangle, rounding all corners with 5px radius
  graphics_fill_rect(ctx, rectangle, 5, GCornersAll);

  if (s_p_one_ago_int > 30){
    graphics_context_set_stroke_color(ctx, GColorBlack);
    graphics_context_set_stroke_width(ctx, 3);
    graphics_draw_line(ctx,GPoint(2,36), GPoint(65, 36));
  }

}

static void person_two_update_proc(Layer *layer, GContext *ctx) {
  //set fill color to white
  graphics_context_set_fill_color(ctx, GColorWhite);
  //define bounds of the rectangle which is passed through from the update_proc assignment
  GRect rectangle = layer_get_bounds(layer);
  //create filled rectangle, rounding all corners with 5px radius
  graphics_fill_rect(ctx, rectangle, 5, GCornersAll);

    if (s_p_two_ago_int > 30){
    graphics_context_set_stroke_color(ctx, GColorBlack);
    graphics_context_set_stroke_width(ctx, 3);
    graphics_draw_line(ctx,GPoint(2,36), GPoint(65, 36));
  }


}

//main jam that draws everything
static void prv_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  //create time text layer
  s_time_layer = text_layer_create(GRect(0,(bounds.size.h/2)-19,bounds.size.w,28));
  text_layer_set_background_color(s_time_layer, GColorBlack);
  text_layer_set_text(s_time_layer, "00:00 AM");
  text_layer_set_text_color(s_time_layer, GColorWhite);
  text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_time_layer));
  
  // //create tap count testing layer
  // s_tap_count_layer = text_layer_create(GRect(0,(bounds.size.h/2)-12,bounds.size.w,24));
  // text_layer_set_background_color(s_tap_count_layer, GColorBlack);
  // text_layer_set_text(s_tap_count_layer, "0");
  // text_layer_set_text_color(s_tap_count_layer, GColorWhite);
  // text_layer_set_font(s_tap_count_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  // text_layer_set_text_alignment(s_tap_count_layer, GTextAlignmentCenter);
  // layer_add_child(window_layer, text_layer_get_layer(s_tap_count_layer));

  //create battery bar
  s_battery_layer = battery_bar_layer_create();
  battery_bar_set_position(GPoint(bounds.size.w-45, (bounds.size.h/2)-5));
  battery_bar_set_colors(GColorWhite, GColorDarkGray, GColorDarkGray, GColorWhite);
  battery_bar_set_percent_hidden(true);
  layer_add_child(window_layer, s_battery_layer);

  //create bluetooth indicator
  s_bluetooth_layer = bluetooth_layer_create();
  bluetooth_set_position(GPoint(10,(bounds.size.h/2)-5));
  bluetooth_vibe_disconnect(true);
  bluetooth_vibe_connect(false);
  //void bluetooth_set_colors(GColor connected_circle, GColor connected_icon, GColor disconnected_circle, GColor disconnected_icon);
  bluetooth_set_colors(GColorBlack, GColorWhite, GColorDarkGray, GColorClear);
  layer_add_child(window_layer, s_bluetooth_layer);

  //recall respect quiet time value; 0 or 1;
  s_respect_quiet_time = persist_read_int(52);


  //***********************************
  //******* PERSON ONE ****************
  //***********************************

  //build holding layer
  GRect person_one_holder_bounds = GRect(0,0,bounds.size.w,(bounds.size.h/2)-12);
  s_person_one_holder_layer = layer_create(person_one_holder_bounds);
  layer_set_update_proc(s_person_one_holder_layer, person_one_update_proc);

  //add directional icon first
  //create layer which will hold the bitmap
  s_p_one_icon_layer = bitmap_layer_create(GRect((person_one_holder_bounds.size.w/2)+30, 20, 30, 30));
	//add bitmap holder to person holder
  layer_add_child(s_person_one_holder_layer, bitmap_layer_get_layer(s_p_one_icon_layer));
  //check to see if the int representing the bitmap exists in local storage
  if(persist_exists(6)){
    //if it exists, check to see if a bitmap already exists
    if (s_p_one_icon_bitmap) {
      //destory existing bitmap for memory
      gbitmap_destroy(s_p_one_icon_bitmap);
    }
    //create new bitmap arrow using int as index
    s_p_one_icon_bitmap = gbitmap_create_with_resource(ICONS[persist_read_int(6)]);
    //set bitmap to layer
    bitmap_layer_set_bitmap(s_p_one_icon_layer, s_p_one_icon_bitmap);
  } else {
    if (s_p_one_icon_bitmap) {
      gbitmap_destroy(s_p_one_icon_bitmap);
    }
    s_p_one_icon_bitmap = gbitmap_create_with_resource(ICONS[4]);
    bitmap_layer_set_bitmap(s_p_one_icon_layer, s_p_one_icon_bitmap);
  }

  //create name layer
  s_p_one_name_text_layer = text_layer_create(GRect(3, -2, person_one_holder_bounds.size.w, 22));
	text_layer_set_text_color(s_p_one_name_text_layer, GColorBlack);
	text_layer_set_background_color(s_p_one_name_text_layer, GColorClear);
	text_layer_set_font(s_p_one_name_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
	text_layer_set_text_alignment(s_p_one_name_text_layer, GTextAlignmentLeft);
  //read name from memory
  if(persist_exists(1)){
    persist_read_string(1, s_p_one_name_text, sizeof(s_p_one_name_text));
  } else {
    strncpy(s_p_one_name_text, "loading", sizeof(s_p_one_name_text));
  }
  text_layer_set_text(s_p_one_name_text_layer, s_p_one_name_text);
  //add name text to holder
	layer_add_child(s_person_one_holder_layer, text_layer_get_layer(s_p_one_name_text_layer));

  //create sgv layer
	s_p_one_sgv_text_layer = text_layer_create(GRect(5, 15, person_one_holder_bounds.size.w/2, 36));
	text_layer_set_text_color(s_p_one_sgv_text_layer, GColorBlack);
	text_layer_set_background_color(s_p_one_sgv_text_layer, GColorClear);
	text_layer_set_font(s_p_one_sgv_text_layer, fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK));
	text_layer_set_text_alignment(s_p_one_sgv_text_layer, GTextAlignmentLeft);
  //read sgv from memory
  if(persist_exists(2)){
    persist_read_string(2, s_p_one_sgv_text, sizeof(s_p_one_sgv_text));
  } else {
    strncpy(s_p_one_sgv_text, "000", sizeof(s_p_one_sgv_text));
  }
  text_layer_set_text(s_p_one_sgv_text_layer, s_p_one_sgv_text);
  //add sgv to holder
	layer_add_child(s_person_one_holder_layer, text_layer_get_layer(s_p_one_sgv_text_layer));

  //create delta layer
	s_p_one_delta_text_layer = text_layer_create(GRect(72, 18, person_one_holder_bounds.size.w/2, 26));
	text_layer_set_text_color(s_p_one_delta_text_layer, GColorBlack);
	text_layer_set_background_color(s_p_one_delta_text_layer, GColorClear);
	text_layer_set_font(s_p_one_delta_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
	text_layer_set_text_alignment(s_p_one_delta_text_layer, GTextAlignmentLeft);
  //read delta from memory
  if(persist_exists(3)){
    persist_read_string(3, s_p_one_delta_text, sizeof(s_p_one_delta_text));
  } else {
    strncpy(s_p_one_delta_text, "+0", sizeof(s_p_one_delta_text));
  }
  text_layer_set_text(s_p_one_delta_text_layer, s_p_one_delta_text);
  //add delta to holder
	layer_add_child(s_person_one_holder_layer, text_layer_get_layer(s_p_one_delta_text_layer));

  //create iob layer
	s_p_one_iob_text_layer = text_layer_create(GRect(20, person_one_holder_bounds.size.h-28, person_one_holder_bounds.size.w, 26));
	text_layer_set_text_color(s_p_one_iob_text_layer, GColorBlack);
	text_layer_set_background_color(s_p_one_iob_text_layer, GColorClear);
	text_layer_set_font(s_p_one_iob_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
	text_layer_set_text_alignment(s_p_one_iob_text_layer, GTextAlignmentLeft);
  //read iob from memory
  if(persist_exists(4)){
    persist_read_string(4, s_p_one_iob_text , sizeof(s_p_one_iob_text));
  } else {
    strncpy(s_p_one_iob_text, "-0.00u", sizeof(s_p_one_iob_text));
  }
  text_layer_set_text(s_p_one_iob_text_layer, s_p_one_iob_text);
  //add iob to holder
	layer_add_child(s_person_one_holder_layer, text_layer_get_layer(s_p_one_iob_text_layer));

  //create battery layer
	s_p_one_battery_text_layer = text_layer_create(GRect((person_one_holder_bounds.size.w/2)+10, person_one_holder_bounds.size.h-28, person_one_holder_bounds.size.w, 26));
	text_layer_set_text_color(s_p_one_battery_text_layer, GColorBlack);
	text_layer_set_background_color(s_p_one_battery_text_layer, GColorClear);
	text_layer_set_font(s_p_one_battery_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
	text_layer_set_text_alignment(s_p_one_battery_text_layer, GTextAlignmentLeft);
  //read battery from memory
  if(persist_exists(5)){
    persist_read_string(5, s_p_one_battery_text , sizeof(s_p_one_battery_text));
  } else {
    strncpy(s_p_one_battery_text, "000%", sizeof(s_p_one_battery_text));
  }
  text_layer_set_text(s_p_one_battery_text_layer, s_p_one_battery_text);
  // add battery to holder
	layer_add_child(s_person_one_holder_layer, text_layer_get_layer(s_p_one_battery_text_layer));

  //create time ago layer
	s_p_one_time_ago_text_layer = text_layer_create(GRect((person_one_holder_bounds.size.w/2), -2, person_one_holder_bounds.size.w, 22));
	text_layer_set_text_color(s_p_one_time_ago_text_layer, GColorBlack);
	text_layer_set_background_color(s_p_one_time_ago_text_layer, GColorClear);
	text_layer_set_font(s_p_one_time_ago_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
	text_layer_set_text_alignment(s_p_one_time_ago_text_layer, GTextAlignmentLeft);
  // read time ago from memory
  if(persist_exists(7)){
    s_p_one_ago_int = persist_read_int(7);
  } else {
    s_p_one_ago_int = 0;
  }
  updateTimeAgo(0);

  // add dateTime to holder
	layer_add_child(s_person_one_holder_layer, text_layer_get_layer(s_p_one_time_ago_text_layer));

  //add person holding layer to root window
  layer_add_child(window_layer, s_person_one_holder_layer);

 //build clear graph layer
  s_person_one_graph_layer = layer_create(person_one_holder_bounds);
  layer_set_update_proc(s_person_one_graph_layer, person_one_draw_graph_update_proc);
	layer_add_child(window_layer, s_person_one_graph_layer);


  //***********************************
  //******* PERSON TWO ****************
  //***********************************

  //build holding layer
  GRect person_two_holder_bounds = GRect(0,(bounds.size.h/2)+12,bounds.size.w,(bounds.size.h/2)-12);
  s_person_two_holder_layer = layer_create(person_two_holder_bounds);
  layer_set_update_proc(s_person_two_holder_layer, person_two_update_proc);

  //add directional icon first
  s_p_two_icon_layer = bitmap_layer_create(GRect((person_two_holder_bounds.size.w/2)+30, 20, 30, 30));
	layer_add_child(s_person_two_holder_layer, bitmap_layer_get_layer(s_p_two_icon_layer));
  if(persist_exists(16)){
    if (s_p_two_icon_bitmap) {
      gbitmap_destroy(s_p_two_icon_bitmap);
    }
    s_p_two_icon_bitmap = gbitmap_create_with_resource(ICONS[persist_read_int(6)]);
    bitmap_layer_set_bitmap(s_p_two_icon_layer, s_p_two_icon_bitmap);
  } else {
    if (s_p_two_icon_bitmap) {
      gbitmap_destroy(s_p_two_icon_bitmap);
    }
    s_p_two_icon_bitmap = gbitmap_create_with_resource(ICONS[4]);
    bitmap_layer_set_bitmap(s_p_two_icon_layer, s_p_two_icon_bitmap);
  }

  //create name layer
  s_p_two_name_text_layer = text_layer_create(GRect(5, -2, person_two_holder_bounds.size.w, 22));
	text_layer_set_text_color(s_p_two_name_text_layer, GColorBlack);
	text_layer_set_background_color(s_p_two_name_text_layer, GColorClear);
	text_layer_set_font(s_p_two_name_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
	text_layer_set_text_alignment(s_p_two_name_text_layer, GTextAlignmentLeft);
  //read name from memory
  if(persist_exists(11)){
    persist_read_string(11, s_p_two_name_text, sizeof(s_p_two_name_text));
  } else {
    strncpy(s_p_two_name_text, "loading", sizeof(s_p_two_name_text));
  }
  text_layer_set_text(s_p_two_name_text_layer, s_p_two_name_text);
  //add name text to holder
	layer_add_child(s_person_two_holder_layer, text_layer_get_layer(s_p_two_name_text_layer));

  //create sgv layer
	s_p_two_sgv_text_layer = text_layer_create(GRect(3, 15, person_two_holder_bounds.size.w/2, 36));
	text_layer_set_text_color(s_p_two_sgv_text_layer, GColorBlack);
	text_layer_set_background_color(s_p_two_sgv_text_layer, GColorClear);
	text_layer_set_font(s_p_two_sgv_text_layer, fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK));
	text_layer_set_text_alignment(s_p_two_sgv_text_layer, GTextAlignmentLeft);
  //read sgv from memory
  if(persist_exists(12)){
    persist_read_string(12, s_p_two_sgv_text, sizeof(s_p_two_sgv_text));
  } else {
    strncpy(s_p_two_sgv_text, "000", sizeof(s_p_two_sgv_text));
  }
  text_layer_set_text(s_p_two_sgv_text_layer, s_p_two_sgv_text);
  //add sgv to holder
	layer_add_child(s_person_two_holder_layer, text_layer_get_layer(s_p_two_sgv_text_layer));

  //create delta layer
	s_p_two_delta_text_layer = text_layer_create(GRect(72, 18, person_two_holder_bounds.size.w/2, 26));
	text_layer_set_text_color(s_p_two_delta_text_layer, GColorBlack);
	text_layer_set_background_color(s_p_two_delta_text_layer, GColorClear);
	text_layer_set_font(s_p_two_delta_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
	text_layer_set_text_alignment(s_p_two_delta_text_layer, GTextAlignmentLeft);
  //read delta from memory
  if(persist_exists(13)){
    persist_read_string(13, s_p_two_delta_text, sizeof(s_p_two_delta_text));
  } else {
    strncpy(s_p_two_delta_text, "+0", sizeof(s_p_two_delta_text));
  }
  text_layer_set_text(s_p_two_delta_text_layer, s_p_two_delta_text);
  //add delta to holder
	layer_add_child(s_person_two_holder_layer, text_layer_get_layer(s_p_two_delta_text_layer));

  //create iob layer
	s_p_two_iob_text_layer = text_layer_create(GRect(20, person_two_holder_bounds.size.h-28, person_two_holder_bounds.size.w, 26));
	text_layer_set_text_color(s_p_two_iob_text_layer, GColorBlack);
	text_layer_set_background_color(s_p_two_iob_text_layer, GColorClear);
	text_layer_set_font(s_p_two_iob_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
	text_layer_set_text_alignment(s_p_two_iob_text_layer, GTextAlignmentLeft);
  //read iob from memory
  if(persist_exists(14)){
    persist_read_string(14, s_p_two_iob_text , sizeof(s_p_two_iob_text));
  } else {
    strncpy(s_p_two_iob_text, "-0.00u", sizeof(s_p_two_iob_text));
  }
  text_layer_set_text(s_p_two_iob_text_layer, s_p_two_iob_text);
  //add iob to holder
	layer_add_child(s_person_two_holder_layer, text_layer_get_layer(s_p_two_iob_text_layer));

  //create battery layer
	s_p_two_battery_text_layer = text_layer_create(GRect((person_two_holder_bounds.size.w/2)+10, person_two_holder_bounds.size.h-28, person_two_holder_bounds.size.w, 26));
	text_layer_set_text_color(s_p_two_battery_text_layer, GColorBlack);
	text_layer_set_background_color(s_p_two_battery_text_layer, GColorClear);
	text_layer_set_font(s_p_two_battery_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
	text_layer_set_text_alignment(s_p_two_battery_text_layer, GTextAlignmentLeft);

  //read battery from memory
  if(persist_exists(15)){
    persist_read_string(15, s_p_two_battery_text , sizeof(s_p_two_battery_text));
  } else {
    strncpy(s_p_two_battery_text, "000%", sizeof(s_p_two_battery_text));
  }
  text_layer_set_text(s_p_two_battery_text_layer, s_p_two_battery_text);
  // add battery to holder
	layer_add_child(s_person_two_holder_layer, text_layer_get_layer(s_p_two_battery_text_layer));

  //create time ago layer
	s_p_two_time_ago_text_layer = text_layer_create(GRect((person_two_holder_bounds.size.w/2), -2, person_two_holder_bounds.size.w, 22));
	text_layer_set_text_color(s_p_two_time_ago_text_layer, GColorBlack);
	text_layer_set_background_color(s_p_two_time_ago_text_layer, GColorClear);
	text_layer_set_font(s_p_two_time_ago_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
	text_layer_set_text_alignment(s_p_two_time_ago_text_layer, GTextAlignmentLeft);
  // read time ago from memory
  if(persist_exists(17)){
    s_p_two_ago_int = persist_read_int(17);
  } else {
    s_p_two_ago_int = 0;
  }
  updateTimeAgo(1);

  // add time ago to holder
	layer_add_child(s_person_two_holder_layer, text_layer_get_layer(s_p_two_time_ago_text_layer));

  //add person holding layer to root window
  layer_add_child(window_layer, s_person_two_holder_layer);

  //build clear graph layer
  s_person_two_graph_layer = layer_create(person_two_holder_bounds);
  layer_set_update_proc(s_person_two_graph_layer, person_two_draw_graph_update_proc);
	layer_add_child(window_layer, s_person_two_graph_layer);

  // Register with TickTimerService
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);

  //register with tap handler service
  accel_tap_service_subscribe(accel_tap_handler);

  //update the time when the watchface loads
  update_time();

}

static void prv_window_unload(Window *window) {
  //destroy everything I guess
  text_layer_destroy(s_p_one_name_text_layer); 
  text_layer_destroy(s_p_one_sgv_text_layer); 
  text_layer_destroy(s_p_one_delta_text_layer); 
  text_layer_destroy(s_p_one_time_ago_text_layer); 
  text_layer_destroy(s_p_one_iob_text_layer); 
  text_layer_destroy(s_p_one_battery_text_layer);
  text_layer_destroy(s_p_two_name_text_layer); 
  text_layer_destroy(s_p_two_sgv_text_layer); 
  text_layer_destroy(s_p_two_delta_text_layer); 
  text_layer_destroy(s_p_two_time_ago_text_layer); 
  text_layer_destroy(s_p_two_iob_text_layer); 
  text_layer_destroy(s_p_two_battery_text_layer);
  text_layer_destroy(s_time_layer);
  text_layer_destroy(s_background_layer);
  s_p_one_icon_bitmap = NULL;
  s_p_two_icon_bitmap = NULL;
  bluetooth_layer_destroy(s_bluetooth_layer);
  battery_bar_layer_destroy(s_battery_layer);
  layer_destroy(s_person_one_holder_layer);
  layer_destroy(s_person_two_holder_layer);
  layer_destroy(s_person_one_graph_layer);
  layer_destroy(s_person_two_graph_layer);

}

static void prv_init(void) {
  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });
  const bool animated = true;
  window_stack_push(s_window, animated);

  //instantiate appmessages
  app_message_register_inbox_received(in_received_handler);
  app_message_register_inbox_dropped(in_dropped_handler);
  app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());

}

static void prv_deinit(void) {
  window_destroy(s_window);
}

int main(void) {
  prv_init();
  app_event_loop();
  prv_deinit();
}
