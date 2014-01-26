/*
   Big Time Plus watch

   a) checks units_changed to update only minute or hour part of screen
   b) Am is white face, pm is inverted black face inspired by apple clock
 */

#include "pebble_os.h"
#include "pebble_app.h"
#include "pebble_fonts.h"

#include "resource_ids.auto.h"

#define MY_UUID { 0x74, 0x57, 0x4D, 0x0F, 0x54, 0x2D, 0x4E, 0x5A, 0xAD, 0x47, 0x50, 0xAF, 0x4B, 0x35, 0x5C, 0xB8 }
PBL_APP_INFO(MY_UUID, "Big Time Plus", "Darshan Sonde", 
        1, 0, RESOURCE_ID_IMAGE_MENU_ICON, APP_INFO_WATCH_FACE);

Window window;

//
// There's only enough memory to load about 6 of 10 required images
// so we have to swap them in & out...
//
// We have one "slot" per digit location on screen.
//
// Because layers can only have one parent we load a digit for each
// slot--even if the digit image is already in another slot.
//
// Slot on-screen layout:
//     0 1
//     2 3
//
#define TOTAL_IMAGE_SLOTS 4

#define NUMBER_OF_IMAGES 10

// These images are 72 x 84 pixels (i.e. a quarter of the display),
// black and white with the digit character centered in the image.
// (As generated by the `fonttools/font2png.py` script.)
const int IMAGE_RESOURCE_IDS[NUMBER_OF_IMAGES] = {
  RESOURCE_ID_IMAGE_NUM_0, RESOURCE_ID_IMAGE_NUM_1, RESOURCE_ID_IMAGE_NUM_2,
  RESOURCE_ID_IMAGE_NUM_3, RESOURCE_ID_IMAGE_NUM_4, RESOURCE_ID_IMAGE_NUM_5,
  RESOURCE_ID_IMAGE_NUM_6, RESOURCE_ID_IMAGE_NUM_7, RESOURCE_ID_IMAGE_NUM_8,
  RESOURCE_ID_IMAGE_NUM_9
};

BmpContainer image_containers[TOTAL_IMAGE_SLOTS];

#define EMPTY_SLOT -1

// The state is either "empty" or the digit of the image currently in
// the slot--which was going to be used to assist with de-duplication
// but we're not doing that due to the one parent-per-layer
// restriction mentioned above.
int image_slot_state[TOTAL_IMAGE_SLOTS] = {EMPTY_SLOT, EMPTY_SLOT, EMPTY_SLOT, EMPTY_SLOT};

void unload_digit_image_from_slot(int slot_number) {
  /*

     Removes the digit from the display and unloads the image resource
     to free up RAM.

     Can handle being called on an already empty slot.

   */

  if (image_slot_state[slot_number] != EMPTY_SLOT) {
    layer_remove_from_parent(&image_containers[slot_number].layer.layer);
    bmp_deinit_container(&image_containers[slot_number]);
    image_slot_state[slot_number] = EMPTY_SLOT;
  }

}

void load_digit_image_into_slot(int slot_number, int digit_value, bool isAm) {
  /*

     Loads the digit image from the application's resources and
     displays it on-screen in the correct location.

     Each slot is a quarter of the screen.

   */

  // TODO: Signal these error(s)?

  if ((slot_number < 0) || (slot_number >= TOTAL_IMAGE_SLOTS)) {
    return;
  }

  if ((digit_value < 0) || (digit_value > 9)) {
    return;
  }

  if (image_slot_state[slot_number] != EMPTY_SLOT) {
    return;
  }


  image_slot_state[slot_number] = digit_value;
  bmp_init_container(IMAGE_RESOURCE_IDS[digit_value], &image_containers[slot_number]);
  bitmap_layer_set_compositing_mode(&image_containers[slot_number].layer,isAm ? GCompOpAssignInverted:GCompOpAssign);
  image_containers[slot_number].layer.layer.frame.origin.x = (slot_number % 2) * 72;
  image_containers[slot_number].layer.layer.frame.origin.y = (slot_number / 2) * 84;
  layer_add_child(&window.layer, &image_containers[slot_number].layer.layer);
}


void display_value(unsigned short value, unsigned short row_number, bool isAm) {
  /*

     Displays a numeric value between 0 and 99 on screen.

     Rows are ordered on screen as:

       Row 0
       Row 1

   */
  value = value % 100; // Maximum of two digits per row.

  // Column order is: | Column 0 | Column 1 |
  int slot_number = (row_number*2);
  unload_digit_image_from_slot(slot_number);
  if (value<10 && row_number==0) {
      load_digit_image_into_slot(slot_number, value % 10, isAm);
  } else {
      load_digit_image_into_slot(slot_number, value / 10, isAm);
  }

  slot_number = (row_number * 2) + 1;
  unload_digit_image_from_slot(slot_number);
  if (!(value<10 && row_number == 0)) {
      load_digit_image_into_slot(slot_number, value % 10, isAm);
  } else {
      unload_digit_image_from_slot(slot_number);
  }
}


unsigned short get_display_hour(unsigned short hour) {

  if (clock_is_24h_style()) {
    return hour;
  }

  unsigned short display_hour = hour % 12;

  // Converts "0" to "12"
  return display_hour ? display_hour : 12;

}


void display_time(PebbleTickEvent *e) {
  
    bool isAm = e->tick_time->tm_hour < 11;
    if(isAm) {
        window_set_background_color(&window,GColorWhite);
        layer_mark_dirty(&window.layer);
    } else {
        window_set_background_color(&window,GColorBlack);
        layer_mark_dirty(&window.layer);
    }

  if(e->units_changed && HOUR_UNIT) {
      display_value(get_display_hour(e->tick_time->tm_hour), 0, isAm);
  } 

  if(e->units_changed && MINUTE_UNIT) {
      display_value(e->tick_time->tm_min, 1, isAm);
  }
}


void handle_minute_tick(AppContextRef ctx, PebbleTickEvent *t) {

  display_time(t);
}


void handle_init(AppContextRef ctx) {

  window_init(&window, "Big Time Plus");
  window_stack_push(&window, true);
  window_set_background_color(&window, GColorBlack);

  resource_init_current_app(&APP_RESOURCES);

  // Avoids a blank screen on watch start.
  PebbleTickEvent e;
  e.units_changed = HOUR_UNIT | MINUTE_UNIT;

  get_time(e.tick_time);
  display_time(&e);
}


void handle_deinit(AppContextRef ctx) {

  for (int i = 0; i < TOTAL_IMAGE_SLOTS; i++) {
    unload_digit_image_from_slot(i);
  }

}


void pbl_main(void *params) {
  PebbleAppHandlers handlers = {
    .init_handler = &handle_init,
    .deinit_handler = &handle_deinit,

    .tick_info = {
      .tick_handler = &handle_minute_tick,
      .tick_units = MINUTE_UNIT | HOUR_UNIT
    }

  };
  app_event_loop(params, &handlers);
}
