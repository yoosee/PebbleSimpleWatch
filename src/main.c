#include <pebble.h>
#include <ctype.h>
#include "main.h"

static Window *s_main_window;
static Layer *s_bg_layer, *s_date_layer, *s_hands_layer;
// static TextLayer *s_time_layer;
static TextLayer *s_weather_layer;
static TextLayer *s_day_label, *s_num_label;
static BitmapLayer *s_background_layer;

TextLayer *health_tlayer;

static GPath *s_tick_paths[NUM_CLOCK_TICKS];
static GPath *s_minute_arrow, *s_hour_arrow;
static char s_num_buffer[4], s_day_buffer[12];

static GBitmap *s_background_bitmap;
static GFont s_weather_font;
static GFont s_steps_font;
static GFont s_date_font;

 
#if defined(PBL_HEALTH)
void update_health()
{
    /* Create a long-lived buffer */
    static char buffer[] = MAX_HEALTH_STR;

    time_t start = time_start_of_today();
    time_t end = time(NULL);  /* Now */

    /* Check data is available */
    HealthServiceAccessibilityMask result = health_service_metric_accessible(HealthMetricStepCount, start, end);
    if(result & HealthServiceAccessibilityMaskAvailable)
    {
        HealthValue steps = health_service_sum(HealthMetricStepCount, start, end);

        APP_LOG(APP_LOG_LEVEL_INFO, "Steps today: %d", (int)steps);
        snprintf(buffer, sizeof(buffer), HEALTH_FMT_STR, (int) steps);
    }
    else
    {
        APP_LOG(APP_LOG_LEVEL_ERROR, "No data available!");
        strcpy(buffer, "");
    }
    text_layer_set_text(health_tlayer, buffer);
}

static void health_handler(HealthEventType event, void *context)
{
    // Which type of event occured?
    switch(event)
    {
        case HealthEventSignificantUpdate:
            APP_LOG(APP_LOG_LEVEL_INFO, "New HealthService HealthEventSignificantUpdate event");
            //break;
        case HealthEventMovementUpdate:
            APP_LOG(APP_LOG_LEVEL_INFO, "New HealthService HealthEventMovementUpdate event");
            update_health();
            break;
        case HealthEventSleepUpdate:
            APP_LOG(APP_LOG_LEVEL_INFO, "New HealthService HealthEventSleepUpdate event");
            break;
    }
}
#endif /* PBL_HEALTH */

static void bg_update_proc(Layer *layer, GContext *ctx) {
  //graphics_context_set_fill_color(ctx, GColorBlack);
  //graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);
  graphics_context_set_fill_color(ctx, GColorWhite);
  for (int i = 0; i < NUM_CLOCK_TICKS; ++i) {
    const int x_offset = PBL_IF_ROUND_ELSE(18, 0);
    const int y_offset = PBL_IF_ROUND_ELSE(6, 0);
    gpath_move_to(s_tick_paths[i], GPoint(x_offset, y_offset));
    gpath_draw_filled(ctx, s_tick_paths[i]);
  }
}

static void hands_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  GPoint center = grect_center_point(&bounds);
  
  const int16_t second_hand_length = PBL_IF_ROUND_ELSE((bounds.size.w / 2) - 19, bounds.size.w / 2);
  
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  
  int32_t second_angle = TRIG_MAX_ANGLE * t->tm_sec / 60;
  GPoint second_hand = {
    .x = (int16_t)(sin_lookup(second_angle) * (int32_t)second_hand_length / TRIG_MAX_RATIO) + center.x,
    .y = (int16_t)(-cos_lookup(second_angle) * (int32_t)second_hand_length / TRIG_MAX_RATIO) + center.y,
  };
  
  // second hand
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_draw_line(ctx, second_hand, center);
  
  // minute hand
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  
  gpath_rotate_to(s_minute_arrow, TRIG_MAX_ANGLE * t->tm_min / 60);
  gpath_draw_filled(ctx, s_minute_arrow);
  gpath_draw_outline(ctx, s_minute_arrow);
  
  // hour hand
  graphics_context_set_fill_color(ctx, GColorJazzberryJam);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  
  gpath_rotate_to(s_hour_arrow, (TRIG_MAX_ANGLE * (((t->tm_hour % 12) * 6) + (t->tm_min / 10))) / (12 * 6));
  gpath_draw_filled(ctx, s_hour_arrow);
  gpath_draw_outline(ctx, s_hour_arrow);
  
  // dot in the middle
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, GRect(bounds.size.w / 2 - 1, bounds.size.h / 2 - 1, 3, 3), 0, GCornerNone);
}

static void date_update_proc(Layer *layer, GContext *ctx) {
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  
  strftime(s_day_buffer, sizeof(s_day_buffer), "%a %d", t);
  text_layer_set_text(s_day_label, s_day_buffer);
}

static void handle_second_tick(struct tm *tick_time, TimeUnits units_changed) {
  layer_mark_dirty(window_get_root_layer(s_main_window));
}

static void handle_minute_tick(struct tm *tick_time, TimeUnits units_changed) {
  // Update Health Steps every minute
  update_health();
  
  // Get weather update every 30 minutes
  if(tick_time->tm_min % 30 == 0) { 
    // Begin dictionary
    DictionaryIterator *iter;
    app_message_outbox_begin(&iter);
  
    // Add a key-value pair
    dict_write_uint8(iter, 0, 0);
  
    // Send the message!
    app_message_outbox_send();
  }
}

/*
static void update_time() {
  // Get a tm structure
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);
  
  // Write the current hours and minutes into a buffer
  static char s_buffer[8];
  strftime(s_buffer, sizeof(s_buffer), clock_is_24h_style() ? 
           "%H:%M" : "%I:%M", tick_time);
  
  // Display this time on the TextLayer
  text_layer_set_text(s_time_layer, s_buffer);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
//  update_time();
  // Get weather update every 30 minutes
  if(tick_time->tm_min % 30 == 0) { 
    // Begin dictionary
    DictionaryIterator *iter;
    app_message_outbox_begin(&iter);
  
    // Add a key-value pair
    dict_write_uint8(iter, 0, 0);
  
    // Send the message!
    app_message_outbox_send();
  }
}
*/

static void main_window_load(Window *window) {
  // Get information about the Window
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  // Create GBitmap and add it to Background Layer
  s_background_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BACKGROUND);
  s_background_layer = bitmap_layer_create(bounds);
  bitmap_layer_set_bitmap(s_background_layer, s_background_bitmap);
  layer_add_child(window_layer, bitmap_layer_get_layer(s_background_layer));
  
  // Create Background and add to main layer
  s_bg_layer = layer_create(bounds);
  layer_set_update_proc(s_bg_layer, bg_update_proc);
  layer_add_child(window_layer, s_bg_layer);
  
  // Create date layer and add to main layer
  
  s_minute_arrow = gpath_create(&MINUTE_HAND_POINTS);
  
  s_date_layer = layer_create(bounds);
  layer_set_update_proc(s_date_layer, date_update_proc);
  layer_add_child(window_layer, s_date_layer);
  
  s_day_label = text_layer_create(PBL_IF_BW_ELSE(
    GRect(110, 62, 40, 20),
    GRect(110, 82, 40, 20)));
  text_layer_set_text(s_day_label, s_day_buffer);
  text_layer_set_background_color(s_day_label, GColorClear);
  text_layer_set_text_color(s_day_label, GColorWhite);
  s_date_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_DATE_14));
  text_layer_set_font(s_day_label, s_date_font);
  layer_add_child(s_date_layer, text_layer_get_layer(s_day_label));

  
  // Create weather and templature layer
  s_weather_layer = text_layer_create(
  GRect(0, PBL_IF_ROUND_ELSE(110,105), bounds.size.w, 50));
  
  text_layer_set_background_color(s_weather_layer, GColorClear);
  text_layer_set_text_color(s_weather_layer, GColorTiffanyBlue);
  text_layer_set_text_alignment(s_weather_layer, GTextAlignmentCenter);
  text_layer_set_text(s_weather_layer, "LOADING");
  
  s_weather_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_INFOTEXT_14));
  text_layer_set_font(s_weather_layer, s_weather_font);
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_weather_layer));
  
  // Create Health Steps layer
  health_tlayer = text_layer_create(
  GRect(0, PBL_IF_ROUND_ELSE(45, 35), bounds.size.w, 20));
  
  text_layer_set_background_color(health_tlayer, GColorClear);
  text_layer_set_text_color(health_tlayer, GColorIndigo);
  text_layer_set_text_alignment(health_tlayer, GTextAlignmentCenter);
  s_steps_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_INFOTEXT_14));
  text_layer_set_font(health_tlayer, s_steps_font);
  text_layer_set_text(health_tlayer, "00000");
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(health_tlayer));
      
  // Create Hands Layer and add it to the top of Root Window
  s_hands_layer = layer_create(bounds);
  layer_set_update_proc(s_hands_layer, hands_update_proc);
  layer_add_child(window_layer, s_hands_layer);
  
}

static void main_window_unload(Window *window) {  
  layer_destroy(s_bg_layer);
  layer_destroy(s_date_layer);
  text_layer_destroy(s_day_label);
  text_layer_destroy(s_num_label);
  
  gbitmap_destroy(s_background_bitmap);
  bitmap_layer_destroy(s_background_layer);  
  
  /*
  text_layer_destroy(s_time_layer);
  fonts_unload_custom_font(s_time_font);
  */
  
  text_layer_destroy(s_weather_layer);
  fonts_unload_custom_font(s_weather_font);
  
  layer_destroy(s_hands_layer);  
}

static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
  // Store incoming information
  static char temperature_buffer[8];
  static char conditions_buffer[32];
  static char weather_layer_buffer[32];
  
  // Read tuples for data
  Tuple *temp_tuple = dict_find(iterator, KEY_TEMPERATURE);
  Tuple *conditions_tuple = dict_find(iterator, KEY_CONDITIONS);

  // If all data is available, use it
  if(temp_tuple && conditions_tuple) {
    snprintf(temperature_buffer, sizeof(temperature_buffer), "%dC", (int)temp_tuple->value->int32);
    snprintf(conditions_buffer, sizeof(conditions_buffer), "%s", conditions_tuple->value->cstring);
  }
  // Assemble full string and display
  snprintf(weather_layer_buffer, sizeof(weather_layer_buffer), "%s\n%s", conditions_buffer, temperature_buffer);
  text_layer_set_text(s_weather_layer, weather_layer_buffer);
}
static void inbox_dropped_callback(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped!");
}
static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed!");
}
static void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Outbox send success!");
}

static void init() {
  // Create main Window element and assign to pointer
  s_main_window = window_create();
  window_set_background_color(s_main_window, GColorBlack);

  // Register callbacks
  app_message_register_inbox_received(inbox_received_callback);
  app_message_register_inbox_dropped(inbox_dropped_callback);
  app_message_register_outbox_failed(outbox_failed_callback);
  app_message_register_outbox_sent(outbox_sent_callback);
  
  // Open AppMessage
  const int inbox_size = 128;
  const int outbox_size = 128;
  app_message_open(inbox_size, outbox_size);

  // Set handlers to manage the elements inside the Window
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload  
  });
  // Show the Window on the watch, with animated=true
  window_stack_push(s_main_window, true);

  s_day_buffer[0] = '\0';
  s_num_buffer[0] = '\0';
  s_minute_arrow = gpath_create(&MINUTE_HAND_POINTS);
  s_hour_arrow = gpath_create(&HOUR_HAND_POINTS);
  
  Layer *window_layer = window_get_root_layer(s_main_window);
  GRect bounds = layer_get_bounds(window_layer);
  GPoint center = grect_center_point(&bounds);
  gpath_move_to(s_minute_arrow, center);
  gpath_move_to(s_hour_arrow, center);
  
  for(int i = 0; i < NUM_CLOCK_TICKS; ++i) {
    s_tick_paths[i] = gpath_create(&ANALOG_BG_POINTS[i]);
  }
  tick_timer_service_subscribe(SECOND_UNIT, handle_second_tick);
  tick_timer_service_subscribe(MINUTE_UNIT, handle_minute_tick);

//  // Make sure the time is displayed from the start
//  update_time();
}

static void deinit() {
  gpath_destroy(s_minute_arrow);
  gpath_destroy(s_hour_arrow);
  for(int i = 0; i < NUM_CLOCK_TICKS; ++i) {
    gpath_destroy(s_tick_paths[i]);
  }
  tick_timer_service_unsubscribe();
  window_destroy(s_main_window);
}


int main(void) {
  init();
  app_event_loop();
  deinit();
  return 0;
}