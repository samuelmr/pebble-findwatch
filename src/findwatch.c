#include <pebble.h>
static Window *window;
static TextLayer *text_layer;
static AppTimer *timer;
static const uint16_t timer_interval_ms = 1000;
static const uint32_t MINUTES_KEY = 1 << 1;
static const uint32_t SECONDS_KEY = 1 << 2;
static const uint32_t VIBES_KEY = 1 << 3;
static const uint32_t FLASHES_KEY = 1 << 4;
int minutes;
int seconds;
char *vibes;
char *flashes;
int default_minutes;
int default_seconds;
char *default_vibes;
char *default_flashes;
char *time_key;
char *vibes_key;
char *flashes_key;
int running;

static void toggle_screen(void) {
  static char body_text[9];
  snprintf(body_text, sizeof(body_text), "\n%d\n%02d", minutes, seconds);
  text_layer_set_text(text_layer, body_text);
  if (vibes) {
    vibes_short_pulse();
  }
  if (flashes) {
    if (seconds % 2) {
      text_layer_set_background_color(text_layer, GColorBlack);
      text_layer_set_text_color(text_layer, GColorWhite);
    }
    else {
      text_layer_set_background_color(text_layer, GColorWhite);
      text_layer_set_text_color(text_layer, GColorBlack);
    }
  }
}

static void timer_callback(void *data) {

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Timer: %02d:%02d", minutes, seconds);

  seconds--;
  if (seconds < 0) {
    minutes--;
    seconds = 59;
  }
  if (minutes < 0) {
    app_timer_cancel(timer);
    running = 0;
    minutes = 0;
    seconds = 0;
  }
  else {
    timer = app_timer_register(timer_interval_ms, timer_callback, NULL);
  }
  toggle_screen();
}

static void reset(void) {
  running = 0;
  minutes = default_minutes;
  seconds = default_seconds;
  vibes = default_vibes;
  flashes = default_flashes;
  toggle_screen();
}

void in_received_handler(DictionaryIterator *received, void *context) {
  Tuple *msg_type = dict_read_first(received);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Got message from phone: %s", msg_type->value->cstring);
  if (strcmp(msg_type->value->cstring, time_key) == 0) {
    Tuple *mins = dict_read_next(received);
    default_minutes = mins->value->int8;
    Tuple *secs = dict_read_next(received);
    default_seconds = secs->value->int8;
	APP_LOG(APP_LOG_LEVEL_DEBUG, "New config: %02d:%02d", minutes, seconds);
  }
  else {
    Tuple *val = dict_read_next(received);
    if (strcmp(msg_type->value->cstring, vibes_key) == 0) {
      strcpy(default_vibes, val->value->cstring);
      APP_LOG(APP_LOG_LEVEL_DEBUG, "Set vibes to: %s", default_vibes);
    }
    else if (strcmp(msg_type->value->cstring, flashes_key) == 0) {
      strcpy(default_flashes, val->value->cstring);
      APP_LOG(APP_LOG_LEVEL_DEBUG, "Set flashes to: %s", default_flashes);
    }
  }
  reset();
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Start: %02d:%02d", minutes, seconds);
  running = 1;
  timer = app_timer_register(timer_interval_ms, timer_callback, NULL);
  toggle_screen();
  vibes_short_pulse();
  seconds--;
}

void in_dropped_handler(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Message from phone dropped: %d", reason);
}

static void click_handler(ClickRecognizerRef recognizer, void *context) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Stop: %02d:%02d", minutes, seconds);
  if (running) {
    app_timer_cancel(timer);
    reset();
    running = 0;
  }
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, click_handler);
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_frame(window_layer);
  text_layer = text_layer_create(GRect(0, 0, bounds.size.w, bounds.size.h));
  text_layer_set_font(text_layer, fonts_get_system_font(FONT_KEY_ROBOTO_BOLD_SUBSET_49));
  text_layer_set_text_alignment(text_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(text_layer));
  reset();
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Start: %02d:%02d", minutes, seconds);
  running = 1;
  timer = app_timer_register(timer_interval_ms, timer_callback, NULL);
  toggle_screen();
  vibes_short_pulse();
  seconds--;
}

static void window_unload(Window *window) {
  text_layer_destroy(text_layer);
}

static void init(void) {
  app_message_register_inbox_received(in_received_handler);
  app_message_register_inbox_dropped(in_dropped_handler);
  const uint32_t inbound_size = 128;
  const uint32_t outbound_size = 128;
  app_message_open(inbound_size, outbound_size);

  default_minutes = persist_exists(MINUTES_KEY) ? persist_read_int(MINUTES_KEY) : 5;
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Initialised minutes to: %d", default_minutes);
  default_seconds = persist_exists(SECONDS_KEY) ? persist_read_int(SECONDS_KEY) : 0;
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Initialised seconds to: %d", default_seconds);
  default_vibes = "yes";
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Initialised vibes to: %s", default_vibes);
  if (persist_exists(VIBES_KEY)) {
    persist_read_string(VIBES_KEY, default_vibes, 256);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Initialised persisted vibes to: %s", default_vibes);
  }
  default_flashes = "yes";
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Initialised flashes to: %s", default_flashes);
  if (persist_exists(FLASHES_KEY)) {
    persist_read_string(FLASHES_KEY, default_flashes, 256);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Initialised persisted flashes to: %s", default_flashes);
  }
  time_key = "time";
  vibes_key = "vibes";
  flashes_key = "flashes";

  window = window_create();
  window_set_click_config_provider(window, click_config_provider);
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  const bool animated = true;
  window_stack_push(window, animated);
}

static void deinit(void) {
  persist_write_int(MINUTES_KEY, default_minutes);
  persist_write_int(SECONDS_KEY, default_seconds);
  persist_write_string(VIBES_KEY, default_vibes);
  persist_write_string(FLASHES_KEY, default_flashes);
  window_destroy(window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
