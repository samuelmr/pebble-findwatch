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
int vibes;
int flashes;
int running_length;
int default_vibes;
int default_flashes;
char *time_key;
char *vibes_key;
char *flashes_key;
int running;

static void toggle_screen(void) {
  static char body_text[9];
  snprintf(body_text, sizeof(body_text), "\n%d\n%02d", minutes, seconds);
  text_layer_set_text(text_layer, body_text);
  if (vibes != 0) {
    vibes_short_pulse();
  }
  if (flashes != 0) {
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
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Vibes: %d, flashes: %d", vibes, flashes);

  seconds++;
  if (seconds > 59) {
    minutes++;
    seconds = 0;
  }
  if (minutes >= running_length) {
    app_timer_cancel(timer);
  }
  else {
    timer = app_timer_register(timer_interval_ms, timer_callback, NULL);
  }
  toggle_screen();
}

static void reset(void) {
  running = 0;
  minutes = 0;
  seconds = 0;
  vibes = default_vibes;
  flashes = default_flashes;
  toggle_screen();
}

void restart() {
  if (running) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Stop: %02d:%02d", minutes, seconds);
    app_timer_cancel(timer);
    reset();
  }
  reset();
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Start: %02d:%02d", minutes, seconds);
  running = 1;
  timer = app_timer_register(timer_interval_ms, timer_callback, NULL);
  toggle_screen();
}

void in_received_handler(DictionaryIterator *received, void *context) {
  Tuple *msg_type = dict_read_first(received);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Got message from phone: %s", msg_type->value->cstring);
  if (strcmp(msg_type->value->cstring, time_key) == 0) {
    Tuple *mins = dict_read_next(received);
    running_length = mins->value->int8;
    APP_LOG(APP_LOG_LEVEL_DEBUG, "New config: %2d minutes", running_length);
  }
  else {
    Tuple *val = dict_read_next(received);
    if (strcmp(msg_type->value->cstring, vibes_key) == 0) {
      default_vibes = val->value->int8;
      APP_LOG(APP_LOG_LEVEL_DEBUG, "Set vibes to: %d", default_vibes);
    }
    else if (strcmp(msg_type->value->cstring, flashes_key) == 0) {
      default_flashes = val->value->int8;
      APP_LOG(APP_LOG_LEVEL_DEBUG, "Set flashes to: %d", default_flashes);
    }
  }
  restart();
  APP_LOG(APP_LOG_LEVEL_DEBUG, "New config: vibes: %d, flashes: %d", vibes, flashes);
}

void in_dropped_handler(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Message from phone dropped: %d", reason);
}

static void click_handler(ClickRecognizerRef recognizer, void *context) {
  if (running) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Stop: %02d:%02d", minutes, seconds);
    app_timer_cancel(timer);
    reset();
  }
  else {
    restart();
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
  text_layer_set_background_color(text_layer, GColorWhite);
  text_layer_set_text_color(text_layer, GColorBlack);
  text_layer_set_text_alignment(text_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(text_layer));
  restart();
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

  running_length = persist_exists(MINUTES_KEY) ? persist_read_int(MINUTES_KEY) : 0;
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Initialised running length to: %d", running_length);
  default_vibes = persist_exists(VIBES_KEY) ? persist_read_int(VIBES_KEY) : 1;
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Initialised vibes to: %d", default_vibes);
  default_flashes = persist_exists(FLASHES_KEY) ? persist_read_int(FLASHES_KEY) : 1;
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Initialised flashes to: %d", default_flashes);
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
  persist_write_int(MINUTES_KEY, running_length);
  persist_write_int(VIBES_KEY, default_vibes);
  persist_write_int(FLASHES_KEY, default_flashes);
  window_destroy(window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
