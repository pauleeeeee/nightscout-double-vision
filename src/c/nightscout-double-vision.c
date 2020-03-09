#include <pebble.h>
#include <pebble-battery-bar/pebble-battery-bar.h>
#include <pebble-bluetooth-icon/pebble-bluetooth-icon.h>
#include "./nightscout-double-vision.h"

static Window *s_window;
static BluetoothLayer *s_bluetooth_layer;
static BatteryBarLayer *s_battery_layer;
static Layer *s_person_one_holder_layer, *s_person_two_holder_layer;
static TextLayer *s_time_layer, *s_background_layer;
static TextLayer *s_p_one_name_text_layer, *s_p_one_sgv_text_layer, *s_p_one_delta_text_layer, *s_p_one_time_ago_text_layer, *s_p_one_iob_text_layer, *s_p_one_battery_text_layer;
static TextLayer *s_p_two_name_text_layer, *s_p_two_sgv_text_layer, *s_p_two_delta_text_layer, *s_p_two_time_ago_text_layer, *s_p_two_iob_text_layer, *s_p_two_battery_text_layer;
static char s_p_one_name_text[24], s_p_one_sgv_text[8], s_p_one_delta_text[8], s_p_one_time_ago_text[24], s_p_one_iob_text[16], s_p_one_battery_text[8];
static char s_p_two_name_text[24], s_p_two_sgv_text[8], s_p_two_delta_text[8], s_p_two_time_ago_text[24], s_p_two_iob_text[16], s_p_two_battery_text[8];
static int s_p_one_ago_int, s_p_two_ago_int;
static GBitmap *s_p_one_icon_bitmap = NULL;
static GBitmap *s_p_two_icon_bitmap = NULL;
static BitmapLayer *s_p_one_icon_layer, *s_p_two_icon_layer;
static AppTimer *timer;
static int CurrentPerson = 0;


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
  strftime(s_buffer, sizeof(s_buffer), "%l:%M %P", tick_time);

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

//this function is called every time the pebble ticks a minute
static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time();
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

  //assign current person to an int. It's important to pipe data from within this function to stable, outside variables. Otherwise data could be lost as a new appmessage comes in
  if (person_id_tuple) {
    CurrentPerson = person_id_tuple->value->int32;
  }

  if (CurrentPerson == 0){
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

  } else if (CurrentPerson == 1){
  
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
  }

}


static void in_dropped_handler(AppMessageResult reason, void *context){
  //handle failed message - probably won't happen
}

//this function draws the two white rectangles for each holder layer
static void person_update_proc(Layer *layer, GContext *ctx) {
  //set fill color to white
  graphics_context_set_fill_color(ctx, GColorWhite);
  //define bounds of the rectangle which is passed through from the update_proc assignment
  GRect rectangle = layer_get_bounds(layer);
  //create filled rectangle, rounding all corners with 5px radius
  graphics_fill_rect(ctx, rectangle, 5, GCornersAll);
}

//main jam that draws everything
static void prv_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  //create time text layer
  s_time_layer = text_layer_create(GRect(0,(bounds.size.h/2)-12,bounds.size.w,24));
  text_layer_set_background_color(s_time_layer, GColorBlack);
  text_layer_set_text(s_time_layer, "00:00 AM");
  text_layer_set_text_color(s_time_layer, GColorWhite);
  text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_time_layer));

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

  //***********************************
  //******* PERSON ONE ****************
  //***********************************

  //build holding layer
  GRect person_one_holder_bounds = GRect(0,0,bounds.size.w,(bounds.size.h/2)-12);
  s_person_one_holder_layer = layer_create(person_one_holder_bounds);
  layer_set_update_proc(s_person_one_holder_layer, person_update_proc);

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
    strncpy(s_p_one_iob_text, "-0.00", sizeof(s_p_one_iob_text));
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
    strncpy(s_p_one_battery_text, "%000", sizeof(s_p_one_battery_text));
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



  //***********************************
  //******* PERSON TWO ****************
  //***********************************

  //build holding layer
  GRect person_two_holder_bounds = GRect(0,(bounds.size.h/2)+12,bounds.size.w,(bounds.size.h/2)-12);
  s_person_two_holder_layer = layer_create(person_two_holder_bounds);
  layer_set_update_proc(s_person_two_holder_layer, person_update_proc);

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
    strncpy(s_p_two_iob_text, "-0.00", sizeof(s_p_two_iob_text));
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
    strncpy(s_p_two_battery_text, "%000", sizeof(s_p_two_battery_text));
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

  // add dateTime to holder
	layer_add_child(s_person_two_holder_layer, text_layer_get_layer(s_p_two_time_ago_text_layer));

  //add person holding layer to root window
  layer_add_child(window_layer, s_person_two_holder_layer);

  // Register with TickTimerService
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);

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
