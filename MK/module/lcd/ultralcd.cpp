/**
 * MK & MK4due 3D Printer Firmware
 *
 * Based on Marlin, Sprinter and grbl
 * Copyright (C) 2011 Camiel Gubbels / Erik van der Zalm
 * Copyright (C) 2013 - 2016 Alberto Cotronei @MagoKimbra
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "../../base.h"
#include "ultralcd.h"

#if ENABLED(ULTRA_LCD)

int plaPreheatHotendTemp;
int plaPreheatHPBTemp;
int plaPreheatFanSpeed;

int absPreheatHotendTemp;
int absPreheatHPBTemp;
int absPreheatFanSpeed;

int gumPreheatHotendTemp;
int gumPreheatHPBTemp;
int gumPreheatFanSpeed;

#if HAS(LCD_FILAMENT_SENSOR) || HAS(LCD_POWER_SENSOR)
  millis_t previous_lcd_status_ms = 0;
#endif

#if HAS(LCD_POWER_SENSOR)
  millis_t print_millis = 0;
#endif

uint8_t lcd_status_message_level;
char lcd_status_message[3 * (LCD_WIDTH) + 1] = WELCOME_MSG; // worst case is kana with up to 3*LCD_WIDTH+1

#if ENABLED(DOGLCD)
  #include "dogm_lcd_implementation.h"
  #define LCD_Printpos(x, y)  u8g.setPrintPos(x + 5, (y + 1) * (DOG_CHAR_HEIGHT + 2))
#else
  #include "ultralcd_implementation_hitachi_HD44780.h"
  #define LCD_Printpos(x, y)  lcd.setCursor(x, y)
#endif

#if ENABLED(LASERBEAM)
  static void lcd_laser_focus_menu();
  static void lcd_laser_menu();
  static void lcd_laser_test_fire_menu();
  static void laser_test_fire(uint8_t power, int dwell);
  static void laser_set_focus(float f_length);
  static void action_laser_focus_custom();
  static void action_laser_focus_1mm();
  static void action_laser_focus_2mm();
  static void action_laser_focus_3mm();
  static void action_laser_focus_4mm();
  static void action_laser_focus_5mm();
  static void action_laser_focus_6mm();
  static void action_laser_focus_7mm();
  static void action_laser_test_20_50ms();
  static void action_laser_test_20_100ms();
  static void action_laser_test_100_50ms();
  static void action_laser_test_100_100ms();
  static void action_laser_test_warm();
  static void action_laser_acc_on();
  static void action_laser_acc_off();
#endif

// The main status screen
static void lcd_status_screen();

millis_t next_lcd_update_ms;

enum LCDViewAction {
  LCDVIEW_NONE,
  LCDVIEW_REDRAW_NOW,
  LCDVIEW_CALL_REDRAW_NEXT,
  LCDVIEW_CLEAR_CALL_REDRAW,
  LCDVIEW_CALL_NO_REDRAW
};

uint8_t lcdDrawUpdate = LCDVIEW_CLEAR_CALL_REDRAW; // Set when the LCD needs to draw, decrements after every draw. Set to 2 in LCD routines so the LCD gets at least 1 full redraw (first redraw is partial)

#if ENABLED(ULTIPANEL)

  // place-holders for Ki and Kd edits
  float raw_Ki, raw_Kd;

  /**
   * INVERT_ROTARY_SWITCH
   *
   * To reverse the menu direction we need a general way to reverse
   * the direction of the encoder everywhere. So encoderDirection is
   * added to allow the encoder to go the other way.
   *
   * This behavior is limited to scrolling Menus and SD card listings,
   * and is disabled in other contexts.
   */
  #if ENABLED(INVERT_ROTARY_SWITCH)
    int8_t encoderDirection = 1;
    #define ENCODER_DIRECTION_NORMAL() (encoderDirection = 1)
    #define ENCODER_DIRECTION_MENUS() (encoderDirection = -1)
  #else
    #define ENCODER_DIRECTION_NORMAL() ;
    #define ENCODER_DIRECTION_MENUS() ;
  #endif

  int8_t encoderDiff; // updated from interrupt context and added to encoderPosition every LCD update

  int8_t manual_move_axis = (int8_t)NO_AXIS;
  millis_t manual_move_start_time = 0;

  bool encoderRateMultiplierEnabled;
  int32_t lastEncoderMovementMillis;

  #if HAS(POWER_SWITCH)
    extern bool powersupply;
  #endif
  static float manual_feedrate[] = MANUAL_FEEDRATE;
  static void lcd_main_menu();
  static void lcd_tune_menu();
  static void lcd_prepare_menu();
  static void lcd_move_menu();
  static void lcd_control_menu();
  static void lcd_stats_menu();
  #if DISABLED(LASERBEAM)
    static void lcd_control_temperature_menu();
    static void lcd_control_temperature_preheat_pla_settings_menu();
    static void lcd_control_temperature_preheat_abs_settings_menu();
    static void lcd_control_temperature_preheat_gum_settings_menu();
  #endif
  static void lcd_control_motion_menu();

  #if DISABLED(LASERBEAM)
    static void lcd_control_volumetric_menu();
  #endif

  #if ENABLED(FILAMENT_CHANGE_FEATURE)
    static void lcd_filament_change_option_menu();
    static void lcd_filament_change_init_message();
    static void lcd_filament_change_unload_message();
    static void lcd_filament_change_insert_message();
    static void lcd_filament_change_load_message();
    static void lcd_filament_change_extrude_message();
    static void lcd_filament_change_resume_message();
  #endif 

  #if ENABLED(HAS_LCD_CONTRAST)
    static void lcd_set_contrast();
  #endif

  #if ENABLED(FWRETRACT)
    static void lcd_control_retract_menu();
  #endif

  #if MECH(DELTA)
    static void lcd_delta_calibrate_menu();
  #endif

  // Function pointer to menu functions.
  typedef void (*screenFunc_t)();

  // Different types of actions that can be used in menu items.
  static void menu_action_back();
  static void menu_action_submenu(screenFunc_t data);
  static void menu_action_gcode(const char* pgcode);
  static void menu_action_function(screenFunc_t data);
  static void menu_action_setting_edit_bool(const char* pstr, bool* ptr);
  static void menu_action_setting_edit_int3(const char* pstr, int* ptr, int minValue, int maxValue);
  static void menu_action_setting_edit_float3(const char* pstr, float* ptr, float minValue, float maxValue);
  static void menu_action_setting_edit_float32(const char* pstr, float* ptr, float minValue, float maxValue);
  static void menu_action_setting_edit_float43(const char* pstr, float* ptr, float minValue, float maxValue);
  static void menu_action_setting_edit_float5(const char* pstr, float* ptr, float minValue, float maxValue);
  static void menu_action_setting_edit_float51(const char* pstr, float* ptr, float minValue, float maxValue);
  static void menu_action_setting_edit_float52(const char* pstr, float* ptr, float minValue, float maxValue);
  static void menu_action_setting_edit_long5(const char* pstr, unsigned long* ptr, unsigned long minValue, unsigned long maxValue);
  static void menu_action_setting_edit_callback_bool(const char* pstr, bool* ptr, screenFunc_t callbackFunc);
  static void menu_action_setting_edit_callback_int3(const char* pstr, int* ptr, int minValue, int maxValue, screenFunc_t callbackFunc);
  static void menu_action_setting_edit_callback_float3(const char* pstr, float* ptr, float minValue, float maxValue, screenFunc_t callbackFunc);
  static void menu_action_setting_edit_callback_float32(const char* pstr, float* ptr, float minValue, float maxValue, screenFunc_t callbackFunc);
  static void menu_action_setting_edit_callback_float43(const char* pstr, float* ptr, float minValue, float maxValue, screenFunc_t callbackFunc);
  static void menu_action_setting_edit_callback_float5(const char* pstr, float* ptr, float minValue, float maxValue, screenFunc_t callbackFunc);
  static void menu_action_setting_edit_callback_float51(const char* pstr, float* ptr, float minValue, float maxValue, screenFunc_t callbackFunc);
  static void menu_action_setting_edit_callback_float52(const char* pstr, float* ptr, float minValue, float maxValue, screenFunc_t callbackFunc);
  static void menu_action_setting_edit_callback_long5(const char* pstr, unsigned long* ptr, unsigned long minValue, unsigned long maxValue, screenFunc_t callbackFunc);

  #if ENABLED(SDSUPPORT)
    static void lcd_sdcard_menu();
    static void menu_action_sdfile(const char* longFilename);
    static void menu_action_sddirectory(const char* longFilename);
  #endif

  #define ENCODER_FEEDRATE_DEADZONE 10

  #if DISABLED(LCD_I2C_VIKI)
    #if DISABLED(ENCODER_STEPS_PER_MENU_ITEM)
      #define ENCODER_STEPS_PER_MENU_ITEM 5
    #endif
    #if DISABLED(ENCODER_PULSES_PER_STEP)
      #define ENCODER_PULSES_PER_STEP 1
    #endif
  #else
    #if DISABLED(ENCODER_STEPS_PER_MENU_ITEM)
      #define ENCODER_STEPS_PER_MENU_ITEM 2 // VIKI LCD rotary encoder uses a different number of steps per rotation
    #endif
    #if DISABLED(ENCODER_PULSES_PER_STEP)
      #define ENCODER_PULSES_PER_STEP 1
    #endif
  #endif


  /* Helper macros for menus */

  /**
   * START_MENU generates the init code for a menu function
   */
  #if ENABLED(BTN_BACK) && BTN_BACK > 0
    #define START_MENU() do { \
      ENCODER_DIRECTION_MENUS(); \
      encoderRateMultiplierEnabled = false; \
      if (encoderPosition > 0x8000) encoderPosition = 0; \
      uint8_t encoderLine = encoderPosition / ENCODER_STEPS_PER_MENU_ITEM; \
      NOMORE(currentMenuViewOffset, encoderLine); \
      uint8_t _lineNr = currentMenuViewOffset, _menuItemNr; \
      bool wasClicked = LCD_CLICKED, itemSelected; \
      bool wasBackClicked = LCD_BACK_CLICKED; \
      if (wasBackClicked) { \
        lcd_quick_feedback(); \
        menu_action_back(); \
        return; } \
      for (uint8_t _drawLineNr = 0; _drawLineNr < LCD_HEIGHT; _drawLineNr++, _lineNr++) { \
        _menuItemNr = 0;
  #else
    #define START_MENU() do { \
      ENCODER_DIRECTION_MENUS(); \
      encoderRateMultiplierEnabled = false; \
      if (encoderPosition > 0x8000) encoderPosition = 0; \
      uint8_t encoderLine = encoderPosition / ENCODER_STEPS_PER_MENU_ITEM; \
      NOMORE(currentMenuViewOffset, encoderLine); \
      uint8_t _lineNr = currentMenuViewOffset, _menuItemNr; \
      bool wasClicked = LCD_CLICKED, itemSelected; \
      for (uint8_t _drawLineNr = 0; _drawLineNr < LCD_HEIGHT; _drawLineNr++, _lineNr++) { \
        _menuItemNr = 0;
  #endif

  /**
   * MENU_ITEM generates draw & handler code for a menu item, potentially calling:
   *
   *   lcd_implementation_drawmenu_[type](sel, row, label, arg3...)
   *   menu_action_[type](arg3...)
   *
   * Examples:
   *   MENU_ITEM(back, MSG_WATCH)
   *     lcd_implementation_drawmenu_back(sel, row, PSTR(MSG_WATCH))
   *     menu_action_back()
   *
   *   MENU_ITEM(function, MSG_PAUSE_PRINT, lcd_sdcard_pause)
   *     lcd_implementation_drawmenu_function(sel, row, PSTR(MSG_PAUSE_PRINT), lcd_sdcard_pause)
   *     menu_action_function(lcd_sdcard_pause)
   *
   *   MENU_ITEM_EDIT(int3, MSG_SPEED, &feedrate_multiplier, 10, 999)
   *   MENU_ITEM(setting_edit_int3, MSG_SPEED, PSTR(MSG_SPEED), &feedrate_multiplier, 10, 999)
   *     lcd_implementation_drawmenu_setting_edit_int3(sel, row, PSTR(MSG_SPEED), PSTR(MSG_SPEED), &feedrate_multiplier, 10, 999)
   *     menu_action_setting_edit_int3(PSTR(MSG_SPEED), &feedrate_multiplier, 10, 999)
   *
   */
  #define _MENU_ITEM_PART_1(type, label, args...) \
    if (_menuItemNr == _lineNr) { \
      itemSelected = encoderLine == _menuItemNr; \
      if (lcdDrawUpdate) \
        lcd_implementation_drawmenu_ ## type(itemSelected, _drawLineNr, PSTR(label), ## args); \
      if (wasClicked && itemSelected) { \
        lcd_quick_feedback()

  #define _MENU_ITEM_PART_2(type, args...) \
        menu_action_ ## type(args); \
        return; \
      } \
    } \
    _menuItemNr++

  #define MENU_ITEM(type, label, args...) do { \
      _MENU_ITEM_PART_1(type, label, ## args); \
      _MENU_ITEM_PART_2(type, ## args); \
    } while(0)

  #if ENABLED(ENCODER_RATE_MULTIPLIER)

    //#define ENCODER_RATE_MULTIPLIER_DEBUG  // If defined, output the encoder steps per second value

    /**
     * MENU_MULTIPLIER_ITEM generates drawing and handling code for a multiplier menu item
     */
    #define MENU_MULTIPLIER_ITEM(type, label, args...) do { \
        _MENU_ITEM_PART_1(type, label, ## args); \
        encoderRateMultiplierEnabled = true; \
        lastEncoderMovementMillis = 0; \
        _MENU_ITEM_PART_2(type, ## args); \
      } while(0)

  #endif // ENCODER_RATE_MULTIPLIER

  #define MENU_ITEM_DUMMY() do { _menuItemNr++; } while(0)
  #define MENU_ITEM_EDIT(type, label, args...) MENU_ITEM(setting_edit_ ## type, label, PSTR(label), ## args)
  #define MENU_ITEM_EDIT_CALLBACK(type, label, args...) MENU_ITEM(setting_edit_callback_ ## type, label, PSTR(label), ## args)
  #if ENABLED(ENCODER_RATE_MULTIPLIER)
    #define MENU_MULTIPLIER_ITEM_EDIT(type, label, args...) MENU_MULTIPLIER_ITEM(setting_edit_ ## type, label, PSTR(label), ## args)
    #define MENU_MULTIPLIER_ITEM_EDIT_CALLBACK(type, label, args...) MENU_MULTIPLIER_ITEM(setting_edit_callback_ ## type, label, PSTR(label), ## args)
  #else // !ENCODER_RATE_MULTIPLIER
    #define MENU_MULTIPLIER_ITEM_EDIT(type, label, args...) MENU_ITEM(setting_edit_ ## type, label, PSTR(label), ## args)
    #define MENU_MULTIPLIER_ITEM_EDIT_CALLBACK(type, label, args...) MENU_ITEM(setting_edit_callback_ ## type, label, PSTR(label), ## args)
  #endif // !ENCODER_RATE_MULTIPLIER
  #define END_MENU() \
      if (encoderLine >= _menuItemNr) { encoderPosition = _menuItemNr * (ENCODER_STEPS_PER_MENU_ITEM) - 1; encoderLine = _menuItemNr - 1; }\
      if (encoderLine >= currentMenuViewOffset + LCD_HEIGHT) { currentMenuViewOffset = encoderLine - (LCD_HEIGHT) + 1; lcdDrawUpdate = LCDVIEW_CALL_REDRAW_NEXT; _lineNr = currentMenuViewOffset - 1; _drawLineNr = -1; } \
      } } while(0)

  /** Used variables to keep track of the menu */
  volatile uint8_t buttons;  //the last checked buttons in a bit array.
  #if ENABLED(REPRAPWORLD_KEYPAD)
    volatile uint8_t buttons_reprapworld_keypad; // to store the keypad shift register values
  #endif

  #if ENABLED(LCD_HAS_SLOW_BUTTONS)
    volatile uint8_t slow_buttons; // Bits of the pressed buttons.
  #endif
  uint8_t currentMenuViewOffset;              /* scroll offset in the current menu */
  millis_t next_button_update_ms;
  uint8_t lastEncoderBits;
  uint32_t encoderPosition;
  #if PIN_EXISTS(SD_DETECT)
    uint8_t lcd_sd_status;
  #endif

  typedef struct {
    screenFunc_t menu_function;
    uint32_t encoder_position;
  } menuPosition;

  screenFunc_t currentScreen = lcd_status_screen; // pointer to the currently active menu handler

  menuPosition screen_history[10];
  uint8_t screen_history_depth = 0;

  bool ignore_click = false;
  bool wait_for_unclick;
  bool defer_return_to_status = false;

  // Variables used when editing values.
  const char* editLabel;
  void* editValue;
  int32_t minEditValue, maxEditValue;
  screenFunc_t callbackFunc;              // call this after editing

  /**
   * General function to go directly to a menu
   * Remembers the previous position
   */
  static void lcd_goto_screen(screenFunc_t screen, const bool feedback = false, const uint32_t encoder = 0) {
    if (currentScreen != screen) {
      currentScreen = screen;
      lcdDrawUpdate = LCDVIEW_CLEAR_CALL_REDRAW;
      #if ENABLED(NEWPANEL)
        encoderPosition = encoder;
        if (feedback) lcd_quick_feedback();
      #endif
      if (screen == lcd_status_screen) {
        defer_return_to_status = false;
        screen_history_depth = 0;
      }
      #if ENABLED(LCD_PROGRESS_BAR)
        // For LCD_PROGRESS_BAR re-initialize custom characters
        lcd_set_custom_characters(screen == lcd_status_screen);
      #endif
    }
  }

  static void lcd_return_to_status() { lcd_goto_screen(lcd_status_screen); }

  inline void lcd_save_previous_menu() {
    if (screen_history_depth < COUNT(screen_history)) {
      screen_history[screen_history_depth].menu_function = currentScreen;
      #if ENABLED(ULTIPANEL)
        screen_history[screen_history_depth].encoder_position = encoderPosition;
      #endif
      ++screen_history_depth;
    }
  }

  static void lcd_goto_previous_menu(bool feedback=false) {
    if (screen_history_depth > 0) {
      --screen_history_depth;
      lcd_goto_screen(screen_history[screen_history_depth].menu_function, feedback
        #if ENABLED(ULTIPANEL)
          , screen_history[screen_history_depth].encoder_position
        #endif
      );
    }
    else
      lcd_return_to_status();
  }

  void lcd_ignore_click(bool b) {
    ignore_click = b;
    wait_for_unclick = false;
  }

#endif // ULTIPANEL

/**
 *
 * "Info Screen"
 *
 * This is very display-dependent, so the lcd implementation draws this.
 */

static void lcd_status_screen() {

  #if ENABLED(ULTIPANEL)
    ENCODER_DIRECTION_NORMAL();
    encoderRateMultiplierEnabled = false;
  #endif

  #if ENABLED(LCD_PROGRESS_BAR)
    millis_t ms = millis();
    #if DISABLED(PROGRESS_MSG_ONCE)
      if (ELAPSED(ms, progress_bar_ms + PROGRESS_BAR_MSG_TIME + PROGRESS_BAR_BAR_TIME)) {
        progress_bar_ms = ms;
      }
    #endif
    #if PROGRESS_MSG_EXPIRE > 0
      // Handle message expire
      if (expire_status_ms > 0) {
        #if ENABLED(SDSUPPORT)
          if (card.isFileOpen()) {
            // Expire the message when printing is active
            if (IS_SD_PRINTING) {
              if (ELAPSED(ms, expire_status_ms)) {
                lcd_status_message[0] = '\0';
                expire_status_ms = 0;
              }
            }
            else {
              expire_status_ms += LCD_UPDATE_INTERVAL;
            }
          }
          else {
            expire_status_ms = 0;
          }
        #else
          expire_status_ms = 0;
        #endif // SDSUPPORT
      }
    #endif
  #endif // LCD_PROGRESS_BAR

  lcd_implementation_status_screen();

  #if HAS(LCD_POWER_SENSOR)
    if (ELAPSED(millis(), print_millis + 2000UL)) print_millis = millis();
  #endif

  #if HAS(LCD_FILAMENT_SENSOR) || HAS(LCD_POWER_SENSOR)
    #if HAS(LCD_FILAMENT_SENSOR) && HAS(LCD_POWER_SENSOR)
      if (ELAPSED(millis(), previous_lcd_status_ms + 15000UL))
    #else
      if (ELAPSED(millis(), previous_lcd_status_ms + 10000UL))
    #endif
    {
      previous_lcd_status_ms = millis();
    }
  #endif

  #if ENABLED(ULTIPANEL)

    bool current_click = LCD_CLICKED;

    if (ignore_click) {
      if (wait_for_unclick) {
        if (!current_click)
          ignore_click = wait_for_unclick = false;
        else
          current_click = false;
      }
      else if (current_click) {
        lcd_quick_feedback();
        wait_for_unclick = true;
        current_click = false;
      }
    }

    if (current_click) {
      lcd_goto_screen(lcd_main_menu, true);
      lcd_implementation_init( // to maybe revive the LCD if static electricity killed it.
        #if ENABLED(LCD_PROGRESS_BAR) && ENABLED(ULTIPANEL)
          currentScreen == lcd_status_screen
        #endif
      );
      #if HAS(LCD_FILAMENT_SENSOR) || HAS(LCD_POWER_SENSOR)
        previous_lcd_status_ms = millis();  // get status message to show up for a while
      #endif
    }

    #if ENABLED(ULTIPANEL_FEEDMULTIPLY)
      int new_frm = feedrate_multiplier + (int32_t)encoderPosition;
      // Dead zone at 100% feedrate
      if ((feedrate_multiplier < 100 && new_frm > 100) || (feedrate_multiplier > 100 && new_frm < 100)) {
        feedrate_multiplier = 100;
        encoderPosition = 0;
      }
      else if (feedrate_multiplier == 100) {
        if ((int32_t)encoderPosition > ENCODER_FEEDRATE_DEADZONE) {
          feedrate_multiplier += (int32_t)encoderPosition - (ENCODER_FEEDRATE_DEADZONE);
          encoderPosition = 0;
        }
        else if ((int32_t)encoderPosition < -(ENCODER_FEEDRATE_DEADZONE)) {
          feedrate_multiplier += (int32_t)encoderPosition + ENCODER_FEEDRATE_DEADZONE;
          encoderPosition = 0;
        }
      }
      else {
        feedrate_multiplier = new_frm;
        encoderPosition = 0;
      }
    #endif // ULTIPANEL_FEEDMULTIPLY

    feedrate_multiplier = constrain(feedrate_multiplier, 10, 999);

  #endif // ULTIPANEL
}

#if ENABLED(ULTIPANEL)

  inline void line_to_current(AxisEnum axis) {
    #if MECH(DELTA)
      calculate_delta(current_position);
      planner.buffer_line(delta[X_AXIS], delta[Y_AXIS], delta[Z_AXIS], current_position[E_AXIS], manual_feedrate[axis]/60, active_extruder, active_driver);
    #else // !DELTA
      planner.buffer_line(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS], current_position[E_AXIS], manual_feedrate[axis]/60, active_extruder, active_driver);
    #endif // !DELTA
  }

  #if ENABLED(SDSUPPORT)

  static void lcd_sdcard_pause() {
    card.pausePrint();
    print_job_counter.pause();
  }

    static void lcd_sdcard_resume() {
      card.startPrint();
      print_job_counter.start();
    }

    static void lcd_sdcard_stop() {
      quickStop();
      #if NOMECH(DELTA) && NOMECH(SCARA)
        set_current_position_from_planner();
      #endif
      card.sdprinting = false;
      card.closeFile();
      print_job_counter.stop();
      autotempShutdown();
      cancel_heatup = true;
      lcd_setstatus(MSG_PRINT_ABORTED, true);
    }

    static void lcd_sdcard_stop_save() {
      card.sdprinting = false;
      print_job_counter.stop();
      quickStop();
      card.closeFile(true);
      autotempShutdown();
      cancel_heatup = true;
    }

  #endif // SDSUPPORT

  /**
   *
   * "Main" menu
   *
   */

  static void lcd_main_menu() {
    START_MENU();
    MENU_ITEM(back, MSG_WATCH);

    #if ENABLED(LASERBEAM)
     if (!(planner.movesplanned() || IS_SD_PRINTING)) {
       MENU_ITEM(submenu, "Laser Functions", lcd_laser_menu);
     }
    #endif

    if (planner.movesplanned() || IS_SD_PRINTING) {
      MENU_ITEM(submenu, MSG_TUNE, lcd_tune_menu);
    }
    else {
      MENU_ITEM(submenu, MSG_PREPARE, lcd_prepare_menu);
      #if MECH(DELTA)
        MENU_ITEM(submenu, MSG_DELTA_CALIBRATE, lcd_delta_calibrate_menu);
      #endif
    }
    MENU_ITEM(submenu, MSG_CONTROL, lcd_control_menu);
    MENU_ITEM(submenu, MSG_STATS, lcd_stats_menu);

    #if ENABLED(SDSUPPORT)
      if (card.cardOK) {
        if (card.isFileOpen()) {
          if (card.sdprinting)
            MENU_ITEM(function, MSG_PAUSE_PRINT, lcd_sdcard_pause);
          else
            MENU_ITEM(function, MSG_RESUME_PRINT, lcd_sdcard_resume);
          MENU_ITEM(function, MSG_STOP_PRINT, lcd_sdcard_stop);
          MENU_ITEM(function, MSG_STOP_SAVE_PRINT, lcd_sdcard_stop_save);
        }
        else {
          MENU_ITEM(submenu, MSG_CARD_MENU, lcd_sdcard_menu);
          #if !PIN_EXISTS(SD_DETECT)
            MENU_ITEM(gcode, MSG_CNG_SDCARD, PSTR("M21"));  // SD-card changed by user
          #endif
        }
      }
      else {
        MENU_ITEM(submenu, MSG_NO_CARD, lcd_sdcard_menu);
        #if !PIN_EXISTS(SD_DETECT)
          MENU_ITEM(gcode, MSG_INIT_SDCARD, PSTR("M21")); // Manually initialize the SD-card via user interface
        #endif
      }
    #endif // SDSUPPORT

    END_MENU();
  }

  /**
   *
   * "Tune" submenu items
   *
   */

  /**
   * Set the home offset based on the current_position
   */
  void lcd_set_home_offsets() {
    // M428 Command
    enqueue_and_echo_commands_P(PSTR("M428"));
    lcd_return_to_status();
  }

  #if ENABLED(BABYSTEPPING)

    int babysteps_done = 0;

    static void _lcd_babystep(const AxisEnum axis, const char* msg) {
      ENCODER_DIRECTION_NORMAL();
      if (encoderPosition) {
        int distance = (int32_t)encoderPosition * BABYSTEP_MULTIPLICATOR;
        encoderPosition = 0;
        lcdDrawUpdate = LCDVIEW_REDRAW_NOW;
        #if MECH(COREXY) || MECH(COREYX)|| MECH(COREXZ) || MECH(COREZX)
          #if ENABLED(BABYSTEP_XY)
            switch(axis) {
              case CORE_AXIS_1: // X on CoreXY, Core YX, CoreXZ and CoreZZ
                babystepsTodo[CORE_AXIS_1] += distance * 2;
                babystepsTodo[CORE_AXIS_2] += distance * 2;
                break;
              case CORE_AXIS_2: // Y on CoreXY and CoreYX, Z on CoreXZ and CoreZX
                babystepsTodo[CORE_AXIS_1] += distance * 2;
                babystepsTodo[CORE_AXIS_2] -= distance * 2;
                break;
              case NORMAL_AXIS: // Z on CoreXY and CoreYX, Y on CoreXZ and CoreZX
                babystepsTodo[NORMAL_AXIS] += distance;
                break;
            }
          #elif MECH(COREXZ) || MECH(COREZX)
            babystepsTodo[CORE_AXIS_1] += distance * 2;
            babystepsTodo[CORE_AXIS_2] -= distance * 2;
          #else
            babystepsTodo[Z_AXIS] += distance;
          #endif
        #else
          babystepsTodo[axis] += distance;
        #endif

        babysteps_done += distance;
      }
      if (lcdDrawUpdate) lcd_implementation_drawedit(msg, itostr3sign(babysteps_done));
      if (LCD_CLICKED) lcd_goto_previous_menu(true);
    }

    #if ENABLED(BABYSTEP_XY)
      static void _lcd_babystep_x() { _lcd_babystep(X_AXIS, PSTR(MSG_BABYSTEPPING_X)); }
      static void _lcd_babystep_y() { _lcd_babystep(Y_AXIS, PSTR(MSG_BABYSTEPPING_Y)); }
      static void lcd_babystep_x() { babysteps_done = 0; lcd_goto_screen(_lcd_babystep_x); }
      static void lcd_babystep_y() { babysteps_done = 0; lcd_goto_screen(_lcd_babystep_y); }
    #endif
    static void _lcd_babystep_z() { _lcd_babystep(Z_AXIS, PSTR(MSG_BABYSTEPPING_Z)); }
    static void lcd_babystep_z() { babysteps_done = 0; lcd_goto_screen(_lcd_babystep_z); }

  #endif // BABYSTEPPING

  static void lcd_tune_fixstep() {
    #if MECH(DELTA)
      enqueue_and_echo_commands_P(PSTR("G28 B"));
    #else
      enqueue_and_echo_commands_P(PSTR("G28 X Y B"));
    #endif
  }

  /**
   * Watch temperature callbacks
   */
    #if ENABLED(THERMAL_PROTECTION_HOTENDS)
    #if TEMP_SENSOR_0 != 0
      void watch_temp_callback_E0() { start_watching_heater(0); }
    #endif
    #if HOTENDS > 1 && TEMP_SENSOR_1 != 0
      void watch_temp_callback_E1() { start_watching_heater(1); }
    #endif // HOTENDS > 1
    #if HOTENDS > 2 && TEMP_SENSOR_2 != 0
      void watch_temp_callback_E2() { start_watching_heater(2); }
    #endif // HOTENDS > 2
    #if HOTENDS > 3 && TEMP_SENSOR_3 != 0
      void watch_temp_callback_E3() { start_watching_heater(3); }
    #endif // HOTENDS > 3
  #else
    #if TEMP_SENSOR_0 != 0
      void watch_temp_callback_E0() {}
    #endif
    #if HOTENDS > 1 && TEMP_SENSOR_1 != 0
      void watch_temp_callback_E1() {}
    #endif // HOTENDS > 1
    #if HOTENDS > 2 && TEMP_SENSOR_2 != 0
      void watch_temp_callback_E2() {}
    #endif // HOTENDS > 2
    #if HOTENDS > 3 && TEMP_SENSOR_3 != 0
      void watch_temp_callback_E3() {}
    #endif // HOTENDS > 3
  #endif

  #if ENABLED(THERMAL_PROTECTION_BED)
    #if TEMP_SENSOR_BED != 0
      void watch_temp_callback_bed() { start_watching_bed(); }
    #endif
  #else
    #if TEMP_SENSOR_BED != 0
      void watch_temp_callback_bed() {}
    #endif
  #endif

  #if ENABLED(THERMAL_PROTECTION_CHAMBER)
    #if TEMP_SENSOR_CHAMBER != 0
      void watch_temp_callback_chamber() { start_watching_chamber(); }
    #endif
  #else
    #if TEMP_SENSOR_CHAMBER != 0
      void watch_temp_callback_chamber() {}
    #endif
  #endif

  #if ENABLED(THERMAL_PROTECTION_COOLER)
    #if TEMP_SENSOR_COOLER != 0
      void watch_temp_callback_cooler() { start_watching_cooler(); }
    #endif
  #else
    #if TEMP_SENSOR_COOLER != 0
      void watch_temp_callback_cooler() {}
    #endif
  #endif

  /**
   *
   * "Tune" submenu
   *
   */
  static void lcd_tune_menu() {
    START_MENU();

    //
    // ^ Main
    //
    MENU_ITEM(back, MSG_MAIN);

    //
    // Speed:
    //
    MENU_ITEM_EDIT(int3, MSG_SPEED, &feedrate_multiplier, 10, 999);

    // Manual bed leveling, Bed Z:
    #if ENABLED(MANUAL_BED_LEVELING)
      MENU_ITEM_EDIT(float43, MSG_BED_Z, &mbl.z_offset, -1, 1);
    #endif

    //
    // Nozzle:
    // Nozzle [1-4]:
    //
    #if HOTENDS == 1
      #if TEMP_SENSOR_0 != 0
        MENU_MULTIPLIER_ITEM_EDIT_CALLBACK(int3, MSG_NOZZLE, &target_temperature[0], 0, HEATER_0_MAXTEMP - 15, watch_temp_callback_E0);
      #endif
    #else // HOTENDS > 1
      #if TEMP_SENSOR_0 != 0
        MENU_MULTIPLIER_ITEM_EDIT_CALLBACK(int3, MSG_NOZZLE " 0", &target_temperature[0], 0, HEATER_0_MAXTEMP - 15, watch_temp_callback_E0);
      #endif
      #if TEMP_SENSOR_1 != 0
        MENU_MULTIPLIER_ITEM_EDIT_CALLBACK(int3, MSG_NOZZLE " 1", &target_temperature[1], 0, HEATER_1_MAXTEMP - 15, watch_temp_callback_E1);
      #endif
      #if HOTENDS > 2
        #if TEMP_SENSOR_2 != 0
          MENU_MULTIPLIER_ITEM_EDIT_CALLBACK(int3, MSG_NOZZLE " 2", &target_temperature[2], 0, HEATER_2_MAXTEMP - 15, watch_temp_callback_E2);
        #endif
        #if HOTENDS > 3
          #if TEMP_SENSOR_3 != 0
            MENU_MULTIPLIER_ITEM_EDIT_CALLBACK(int3, MSG_NOZZLE " 3", &target_temperature[3], 0, HEATER_3_MAXTEMP - 15, watch_temp_callback_E3);
          #endif
        #endif // HOTENDS > 3
      #endif // HOTENDS > 2
    #endif // HOTENDS > 1

    //
    // Bed:
    //
    #if TEMP_SENSOR_BED != 0
      MENU_MULTIPLIER_ITEM_EDIT_CALLBACK(int3, MSG_BED, &target_temperature_bed, 0, BED_MAXTEMP - 15, watch_temp_callback_bed);
    #endif

    //
    // Chamber
    //
    #if TEMP_SENSOR_CHAMBER != 0
      MENU_MULTIPLIER_ITEM_EDIT_CALLBACK(int3, MSG_CHAMBER, &target_temperature_chamber, 0, CHAMBER_MAXTEMP - 15, watch_temp_callback_chamber);
    #endif

    //
    // Cooler
    //
    #if TEMP_SENSOR_COOLER != 0
      MENU_MULTIPLIER_ITEM_EDIT_CALLBACK(int3, MSG_COOLER, &target_temperature_cooler, 0, COOLER_MAXTEMP - 15, watch_temp_callback_cooler);
    #endif

    //
    // Fan Speed:
    //
    MENU_MULTIPLIER_ITEM_EDIT(int3, MSG_FAN_SPEED, &fanSpeed, 0, 255);

    //
    // Flow:
    // Flow 1:
    // Flow 2:
    // Flow 3:
    // Flow 4:
    //
    #if EXTRUDERS == 1
      MENU_ITEM_EDIT(int3, MSG_FLOW, &extruder_multiplier[0], 10, 999);
    #else // EXTRUDERS > 1
      MENU_ITEM_EDIT(int3, MSG_FLOW, &extruder_multiplier[active_extruder], 10, 999);
      MENU_ITEM_EDIT(int3, MSG_FLOW " 0", &extruder_multiplier[0], 10, 999);
      MENU_ITEM_EDIT(int3, MSG_FLOW " 1", &extruder_multiplier[1], 10, 999);
      #if EXTRUDERS > 2
        MENU_ITEM_EDIT(int3, MSG_FLOW " 2", &extruder_multiplier[2], 10, 999);
        #if EXTRUDERS > 3
          MENU_ITEM_EDIT(int3, MSG_FLOW " 3", &extruder_multiplier[3], 10, 999);
        #endif // EXTRUDERS > 3
      #endif // EXTRUDERS > 2
    #endif // EXTRUDERS > 1

    //
    // Babystep X:
    // Babystep Y:
    // Babystep Z:
    //
    #if ENABLED(BABYSTEPPING)
      #if ENABLED(BABYSTEP_XY)
        MENU_ITEM(submenu, MSG_BABYSTEP_X, lcd_babystep_x);
        MENU_ITEM(submenu, MSG_BABYSTEP_Y, lcd_babystep_y);
      #endif // BABYSTEP_XY
      MENU_ITEM(submenu, MSG_BABYSTEP_Z, lcd_babystep_z);
    #endif

    MENU_ITEM(function, MSG_FIX_LOSE_STEPS, lcd_tune_fixstep);

    //
    // Change filament
    //
    #if ENABLED(FILAMENT_CHANGE_FEATURE)
       MENU_ITEM(gcode, MSG_FILAMENT_CHANGE, PSTR("M600"));
    #endif

    END_MENU();
  }

  #if ENABLED(EASY_LOAD)
    static void lcd_extrude(float length, float feedrate) {
      current_position[E_AXIS] += length;
      #if MECH(DELTA)
        calculate_delta(current_position);
        planner.buffer_line(delta[X_AXIS], delta[Y_AXIS], delta[Z_AXIS], current_position[E_AXIS], feedrate, active_extruder, active_driver);
      #else
        planner.buffer_line(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS], current_position[E_AXIS], feedrate, active_extruder, active_driver);
      #endif
    }
    static void lcd_purge() { lcd_extrude(LCD_PURGE_LENGTH, LCD_PURGE_FEEDRATE); }
    static void lcd_retract() { lcd_extrude(-LCD_RETRACT_LENGTH, LCD_RETRACT_FEEDRATE); }
    static void lcd_easy_load() {
      allow_lengthy_extrude_once = true;
      lcd_extrude(BOWDEN_LENGTH, LCD_LOAD_FEEDRATE);
      lcd_return_to_status();
    }
    static void lcd_easy_unload() {
      allow_lengthy_extrude_once = true;
      lcd_extrude(-BOWDEN_LENGTH, LCD_UNLOAD_FEEDRATE);
      lcd_return_to_status();
    }
  #endif // EASY_LOAD

  /**
   *
   * "Prepare" submenu items
   *
   */
  void _lcd_preheat(int endnum, const float temph, const float tempb, const int fan) {
    if (temph > 0) setTargetHotend(temph, endnum);
   #if TEMP_SENSOR_BED != 0
     setTargetBed(tempb);
   #else
     UNUSED(tempb);
   #endif
   fanSpeed = fan;
   lcd_return_to_status();
 }

  #if TEMP_SENSOR_0 != 0
    void lcd_preheat_pla0() { _lcd_preheat(0, plaPreheatHotendTemp, plaPreheatHPBTemp, plaPreheatFanSpeed); }
    void lcd_preheat_abs0() { _lcd_preheat(0, absPreheatHotendTemp, absPreheatHPBTemp, absPreheatFanSpeed); }
    void lcd_preheat_gum0() { _lcd_preheat(0, gumPreheatHotendTemp, gumPreheatHPBTemp, gumPreheatFanSpeed); }
  #endif

  #if HOTENDS > 1
    void lcd_preheat_pla1() { _lcd_preheat(1, plaPreheatHotendTemp, plaPreheatHPBTemp, plaPreheatFanSpeed); }
    void lcd_preheat_abs1() { _lcd_preheat(1, absPreheatHotendTemp, absPreheatHPBTemp, absPreheatFanSpeed); }
    void lcd_preheat_gum1() { _lcd_preheat(1, gumPreheatHotendTemp, gumPreheatHPBTemp, gumPreheatFanSpeed); }
    #if HOTENDS > 2
      void lcd_preheat_pla2() { _lcd_preheat(2, plaPreheatHotendTemp, plaPreheatHPBTemp, plaPreheatFanSpeed); }
      void lcd_preheat_abs2() { _lcd_preheat(2, absPreheatHotendTemp, absPreheatHPBTemp, absPreheatFanSpeed); }
      void lcd_preheat_gum2() { _lcd_preheat(2, gumPreheatHotendTemp, gumPreheatHPBTemp, gumPreheatFanSpeed); }
      #if HOTENDS > 3
        void lcd_preheat_pla3() { _lcd_preheat(3, plaPreheatHotendTemp, plaPreheatHPBTemp, plaPreheatFanSpeed); }
        void lcd_preheat_abs3() { _lcd_preheat(3, absPreheatHotendTemp, absPreheatHPBTemp, absPreheatFanSpeed); }
        void lcd_preheat_gum3() { _lcd_preheat(3, gumPreheatHotendTemp, gumPreheatHPBTemp, gumPreheatFanSpeed); }
      #endif
    #endif

    void lcd_preheat_pla0123() {
      #if HOTENDS > 1
        setTargetHotend(plaPreheatHotendTemp, 1);
        #if HOTENDS > 2
          setTargetHotend(plaPreheatHotendTemp, 2);
          #if HOTENDS > 3
            setTargetHotend(plaPreheatHotendTemp, 3);
          #endif
        #endif
      #endif
      lcd_preheat_pla0();
    }
    void lcd_preheat_abs0123() {
      #if HOTENDS > 1
        setTargetHotend(absPreheatHotendTemp, 1);
        #if HOTENDS > 2
          setTargetHotend(absPreheatHotendTemp, 2);
          #if HOTENDS > 3
            setTargetHotend(absPreheatHotendTemp, 3);
          #endif
        #endif
      #endif
      lcd_preheat_abs0();
    }
    void lcd_preheat_gum0123() {
      #if HOTENDS > 1
        setTargetHotend(gumPreheatHotendTemp, 1);
        #if HOTENDS > 2
          setTargetHotend(gumPreheatHotendTemp, 2);
          #if HOTENDS > 3
            setTargetHotend(gumPreheatHotendTemp, 3);
          #endif
        #endif
      #endif
      lcd_preheat_gum0();
    }

  #endif // HOTENDS > 1

  #if TEMP_SENSOR_BED != 0
    void lcd_preheat_pla_bedonly() { _lcd_preheat(0, 0, plaPreheatHPBTemp, plaPreheatFanSpeed); }
    void lcd_preheat_abs_bedonly() { _lcd_preheat(0, 0, absPreheatHPBTemp, absPreheatFanSpeed); }
    void lcd_preheat_gum_bedonly() { _lcd_preheat(0, 0, gumPreheatHPBTemp, gumPreheatFanSpeed); }
  #endif

  #if TEMP_SENSOR_0 != 0 && (TEMP_SENSOR_1 != 0 || TEMP_SENSOR_2 != 0 || TEMP_SENSOR_3 != 0 || TEMP_SENSOR_BED != 0)

    static void lcd_preheat_pla_menu() {
      START_MENU();
      MENU_ITEM(back, MSG_PREPARE);
      #if HOTENDS == 1
        MENU_ITEM(function, MSG_PREHEAT_PLA, lcd_preheat_pla0);
      #else
        MENU_ITEM(function, MSG_PREHEAT_PLA " 0", lcd_preheat_pla0);
        MENU_ITEM(function, MSG_PREHEAT_PLA " 1", lcd_preheat_pla1);
        #if HOTENDS > 2
          MENU_ITEM(function, MSG_PREHEAT_PLA " 2", lcd_preheat_pla2);
          #if HOTENDS > 3
          MENU_ITEM(function, MSG_PREHEAT_PLA " 3", lcd_preheat_pla3);
          #endif
        #endif
        MENU_ITEM(function, MSG_PREHEAT_PLA_ALL, lcd_preheat_pla0123);
      #endif
      #if TEMP_SENSOR_BED != 0
        MENU_ITEM(function, MSG_PREHEAT_PLA_BEDONLY, lcd_preheat_pla_bedonly);
      #endif
      END_MENU();
    }

    static void lcd_preheat_abs_menu() {
      START_MENU();
      MENU_ITEM(back, MSG_PREPARE);
      #if HOTENDS == 1
        MENU_ITEM(function, MSG_PREHEAT_ABS, lcd_preheat_abs0);
      #else
        MENU_ITEM(function, MSG_PREHEAT_ABS " 0", lcd_preheat_abs0);
        MENU_ITEM(function, MSG_PREHEAT_ABS " 1", lcd_preheat_abs1);
        #if HOTENDS > 2
          MENU_ITEM(function, MSG_PREHEAT_ABS " 2", lcd_preheat_abs2);
          #if HOTENDS > 3
          MENU_ITEM(function, MSG_PREHEAT_ABS " 3", lcd_preheat_abs3);
          #endif
        #endif
        MENU_ITEM(function, MSG_PREHEAT_ABS_ALL, lcd_preheat_abs0123);
      #endif
      #if TEMP_SENSOR_BED != 0
        MENU_ITEM(function, MSG_PREHEAT_ABS_BEDONLY, lcd_preheat_abs_bedonly);
      #endif
      END_MENU();
    }

    static void lcd_preheat_gum_menu() {
      START_MENU();
      MENU_ITEM(back, MSG_PREPARE);
      #if HOTENDS == 1
        MENU_ITEM(function, MSG_PREHEAT_GUM, lcd_preheat_gum0);
      #else
        MENU_ITEM(function, MSG_PREHEAT_GUM " 0", lcd_preheat_gum0);
        MENU_ITEM(function, MSG_PREHEAT_GUM " 1", lcd_preheat_gum1);
        #if HOTENDS > 2
          MENU_ITEM(function, MSG_PREHEAT_GUM " 2", lcd_preheat_gum2);
          #if HOTENDS > 3
            MENU_ITEM(function, MSG_PREHEAT_GUM " 3", lcd_preheat_gum3);
          #endif
        #endif
        MENU_ITEM(function, MSG_PREHEAT_GUM_ALL, lcd_preheat_gum0123);
      #endif
      #if TEMP_SENSOR_BED != 0
        MENU_ITEM(function, MSG_PREHEAT_GUM_BEDONLY, lcd_preheat_gum_bedonly);
      #endif
      END_MENU();
    }

  #endif // TEMP_SENSOR_0 && (TEMP_SENSOR_1 || TEMP_SENSOR_2 || TEMP_SENSOR_3 || TEMP_SENSOR_BED)

  void lcd_cooldown() {
    disable_all_heaters();
    disable_all_coolers();
    fanSpeed = 0;
    lcd_return_to_status();
  }

  #if ENABLED(SDSUPPORT) && ENABLED(MENU_ADDAUTOSTART)
    static void lcd_autostart_sd() {
      card.checkautostart(true);
    }
  #endif

  #if ENABLED(MANUAL_BED_LEVELING)

    /**
     *
     * "Prepare" > "Bed Leveling" handlers
     *
     */

    static uint8_t _lcd_level_bed_position;

    // Utility to go to the next mesh point
    // A raise is added between points if MIN_Z_HEIGHT_FOR_HOMING is in use
    // Note: During Manual Bed Leveling the homed Z position is MESH_HOME_SEARCH_Z
    // Z position will be restored with the final action, a G28
    inline void _mbl_goto_xy(float x, float y) {
      current_position[Z_AXIS] = MESH_HOME_SEARCH_Z
        #if MIN_Z_HEIGHT_FOR_HOMING > 0
          + MIN_Z_HEIGHT_FOR_HOMING
        #endif
      ;
      line_to_current(Z_AXIS);
      current_position[X_AXIS] = x + home_offset[X_AXIS];
      current_position[Y_AXIS] = y + home_offset[Y_AXIS];
      line_to_current(manual_feedrate[X_AXIS] <= manual_feedrate[Y_AXIS] ? X_AXIS : Y_AXIS);
      #if MIN_Z_HEIGHT_FOR_HOMING > 0
        current_position[Z_AXIS] = MESH_HOME_SEARCH_Z;
        line_to_current(Z_AXIS);
      #endif
      st_synchronize();
    }

    static void _lcd_level_goto_next_point();

    static void _lcd_level_bed_done() {
      if (lcdDrawUpdate) lcd_implementation_drawedit(PSTR(MSG_LEVEL_BED_DONE));
      lcdDrawUpdate =
        #if ENABLED(DOGLCD)
          LCDVIEW_CALL_REDRAW_NEXT
        #else
          LCDVIEW_CALL_NO_REDRAW
        #endif
      ;
    }

    /**
     * Step 7: Get the Z coordinate, then goto next point or exit
     */
    static void _lcd_level_bed_get_z() {
      ENCODER_DIRECTION_NORMAL();

      // Encoder wheel adjusts the Z position
      if (encoderPosition) {
        refresh_cmd_timeout();
        current_position[Z_AXIS] += float((int32_t)encoderPosition) * (MBL_Z_STEP);
        NOLESS(current_position[Z_AXIS], 0);
        NOMORE(current_position[Z_AXIS], MESH_HOME_SEARCH_Z * 2);
        line_to_current(Z_AXIS);
        lcdDrawUpdate =
          #if ENABLED(DOGLCD)
            LCDVIEW_CALL_REDRAW_NEXT
          #else
            LCDVIEW_REDRAW_NOW
          #endif
        ;
        encoderPosition = 0;
      }

      static bool debounce_click = false;
      if (LCD_CLICKED) {
        if (!debounce_click) {
          debounce_click = true; // ignore multiple "clicks" in a row
          mbl.set_zigzag_z(_lcd_level_bed_position++, current_position[Z_AXIS]);
          if (_lcd_level_bed_position == (MESH_NUM_X_POINTS) * (MESH_NUM_Y_POINTS)) {
            lcd_goto_screen(_lcd_level_bed_done, true);

            current_position[Z_AXIS] = MESH_HOME_SEARCH_Z
              #if MIN_Z_HEIGHT_FOR_HOMING > 0
                + MIN_Z_HEIGHT_FOR_HOMING
              #endif
            ;
            line_to_current(Z_AXIS);
            st_synchronize();

            mbl.set_has_mesh(true);
            enqueue_and_echo_commands_P(PSTR("G28"));
            lcd_return_to_status();
            //LCD_MESSAGEPGM(MSG_LEVEL_BED_DONE);
            #if HAS(BUZZER)
              buzz(200, 659);
              buzz(200, 698);
            #endif
          }
          else {
            lcd_goto_screen(_lcd_level_goto_next_point, true);
          }
        }
      }
      else {
        debounce_click = false;
      }

      // Update on first display, then only on updates to Z position
      // Show message above on clicks instead
      if (lcdDrawUpdate) {
        float v = current_position[Z_AXIS] - MESH_HOME_SEARCH_Z;
        lcd_implementation_drawedit(PSTR(MSG_MOVE_Z), ftostr43sign(v + (v < 0 ? -0.0001 : 0.0001), '+'));
      }

    }

    /**
     * Step 6: Display "Next point: 1 / 9" while waiting for move to finish
     */
    static void _lcd_level_bed_moving() {
      if (lcdDrawUpdate) {
        char msg[10];
        sprintf_P(msg, PSTR("%i / %u"), (int)(_lcd_level_bed_position + 1), (MESH_NUM_X_POINTS) * (MESH_NUM_Y_POINTS));
        lcd_implementation_drawedit(PSTR(MSG_LEVEL_BED_NEXT_POINT), msg);
      }

      lcdDrawUpdate =
        #if ENABLED(DOGLCD)
          LCDVIEW_CALL_REDRAW_NEXT
        #else
          LCDVIEW_CALL_NO_REDRAW
        #endif
      ;
    }

    /**
     * Step 5: Initiate a move to the next point
     */
    static void _lcd_level_goto_next_point() {
      // Set the menu to display ahead of blocking call
      lcd_goto_screen(_lcd_level_bed_moving);

      // _mbl_goto_xy runs the menu loop until the move is done
      int8_t px, py;
      mbl.zigzag(_lcd_level_bed_position, px, py);
      _mbl_goto_xy(mbl.get_probe_x(px), mbl.get_probe_y(py));

      // After the blocking function returns, change menus
      lcd_goto_screen(_lcd_level_bed_get_z);
    }

    /**
     * Step 4: Display "Click to Begin", wait for click
     *         Move to the first probe position
     */
    static void _lcd_level_bed_homing_done() {
      if (lcdDrawUpdate) lcd_implementation_drawedit(PSTR(MSG_LEVEL_BED_WAITING));
      if (LCD_CLICKED) {
        _lcd_level_bed_position = 0;
        current_position[Z_AXIS] = MESH_HOME_SEARCH_Z;
        planner.set_position_mm(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS], current_position[E_AXIS]);
        lcd_goto_screen(_lcd_level_goto_next_point, true);
      }
    }

    /**
     * Step 3: Display "Homing XYZ" - Wait for homing to finish
     */
    static void _lcd_level_bed_homing() {
      if (lcdDrawUpdate) lcd_implementation_drawedit(PSTR(MSG_LEVEL_BED_HOMING), NULL);
      lcdDrawUpdate =
        #if ENABLED(DOGLCD)
          LCDVIEW_CALL_REDRAW_NEXT
        #else
          LCDVIEW_CALL_NO_REDRAW
        #endif
      ;
      if (axis_homed[X_AXIS] && axis_homed[Y_AXIS] && axis_homed[Z_AXIS])
        lcd_goto_screen(_lcd_level_bed_homing_done);
    }

    /**
     * Step 2: Continue Bed Leveling...
     */
    static void _lcd_level_bed_continue() {
      defer_return_to_status = true;
      axis_homed[X_AXIS] = axis_homed[Y_AXIS] = axis_homed[Z_AXIS] = false;
      mbl.reset();
      enqueue_and_echo_commands_P(PSTR("G28"));
      lcd_goto_screen(_lcd_level_bed_homing);
    }

    /**
     * Step 1: MBL entry-point: "Cancel" or "Level Bed"
     */
    static void lcd_level_bed() {
      START_MENU();
      MENU_ITEM(back, MSG_LEVEL_BED_CANCEL);
      MENU_ITEM(submenu, MSG_LEVEL_BED, _lcd_level_bed_continue);
      END_MENU();
    }

  #endif  // MANUAL_BED_LEVELING

  /**
   *
   * "Prepare" submenu
   *
   */

  static void lcd_prepare_menu() {
    START_MENU();

    //
    // ^ Main
    //
    MENU_ITEM(back, MSG_MAIN);

    //
    // Auto Home
    //
    #if ENABLED(LASERBEAM)
      MENU_ITEM(gcode, MSG_AUTO_HOME, PSTR("G28 X Y F2000"));
    #else
      MENU_ITEM(gcode, MSG_AUTO_HOME, PSTR("G28"));
      #if !MECH(DELTA)
        MENU_ITEM(gcode, MSG_AUTO_HOME_X, PSTR("G28 X"));
        MENU_ITEM(gcode, MSG_AUTO_HOME_Y, PSTR("G28 Y"));
        MENU_ITEM(gcode, MSG_AUTO_HOME_Z, PSTR("G28 Z"));
      #endif
    #endif

    //
    // Set Home Offsets
    //
    MENU_ITEM(function, MSG_SET_HOME_OFFSETS, lcd_set_home_offsets);
    //MENU_ITEM(gcode, MSG_SET_ORIGIN, PSTR("G92 X0 Y0 Z0"));

    //
    // Level Bed
    //
    #if ENABLED(AUTO_BED_LEVELING_FEATURE) && NOMECH(DELTA)
      MENU_ITEM(gcode, MSG_LEVEL_BED,
        axis_homed[X_AXIS] && axis_homed[Y_AXIS] ? PSTR("G29") : PSTR("G28\nG29")
      );
    #elif ENABLED(MANUAL_BED_LEVELING)
      MENU_ITEM(submenu, MSG_LEVEL_BED, lcd_level_bed);
    #endif

    //
    // Move Axis
    //
    MENU_ITEM(submenu, MSG_MOVE_AXIS, lcd_move_menu);

    //
    // Disable Steppers
    //
    MENU_ITEM(gcode, MSG_DISABLE_STEPPERS, PSTR("M84"));

    //
    // Preheat PLA
    // Preheat ABS
    // Preheat GUM
    //
    #if TEMP_SENSOR_0 != 0 && DISABLED(LASERBEAM)
      #if TEMP_SENSOR_1 != 0 || TEMP_SENSOR_2 != 0 || TEMP_SENSOR_3 != 0 || TEMP_SENSOR_BED != 0
        MENU_ITEM(submenu, MSG_PREHEAT_PLA, lcd_preheat_pla_menu);
        MENU_ITEM(submenu, MSG_PREHEAT_ABS, lcd_preheat_abs_menu);
        MENU_ITEM(submenu, MSG_PREHEAT_GUM, lcd_preheat_gum_menu);
      #else
        MENU_ITEM(function, MSG_PREHEAT_PLA, lcd_preheat_pla0);
        MENU_ITEM(function, MSG_PREHEAT_ABS, lcd_preheat_abs0);
        MENU_ITEM(function, MSG_PREHEAT_GUM, lcd_preheat_gum0);
      #endif
    #endif

    //
    // Easy Load
    //
    #if ENABLED(EASY_LOAD)
      MENU_ITEM(function, MSG_E_BOWDEN_LENGTH, lcd_easy_load);
      MENU_ITEM(function, MSG_R_BOWDEN_LENGTH, lcd_easy_unload);
      MENU_ITEM(function, MSG_PURGE_XMM, lcd_purge);
      MENU_ITEM(function, MSG_RETRACT_XMM, lcd_retract);
    #endif // EASY_LOAD

    //
    // Cooldown
    //
    MENU_ITEM(function, MSG_COOLDOWN, lcd_cooldown);

    //
    // Switch power on/off
    //
    #if HAS(POWER_SWITCH)
      if (powersupply)
        MENU_ITEM(gcode, MSG_SWITCH_PS_OFF, PSTR("M81"));
      else
        MENU_ITEM(gcode, MSG_SWITCH_PS_ON, PSTR("M80"));
    #endif

    //
    // Autostart
    //
    #if ENABLED(SDSUPPORT) && ENABLED(MENU_ADDAUTOSTART)
      MENU_ITEM(function, MSG_AUTOSTART, lcd_autostart_sd);
    #endif

    END_MENU();
  }

  #if MECH(DELTA)

    static void lcd_delta_calibrate_menu() {
      START_MENU();
      MENU_ITEM(back, MSG_MAIN);
      MENU_ITEM(gcode, MSG_AUTO_HOME, PSTR("G28"));
      MENU_ITEM(gcode, MSG_DELTA_CALIBRATE_X, PSTR("G0 F8000 X-77.94 Y-45 Z0"));
      MENU_ITEM(gcode, MSG_DELTA_CALIBRATE_Y, PSTR("G0 F8000 X77.94 Y-45 Z0"));
      MENU_ITEM(gcode, MSG_DELTA_CALIBRATE_Z, PSTR("G0 F8000 X0 Y90 Z0"));
      MENU_ITEM(gcode, MSG_DELTA_CALIBRATE_CENTER, PSTR("G0 F8000 X0 Y0 Z0"));
      END_MENU();
    }

  #endif // DELTA

  /**
   * If the most recent manual move hasn't been fed to the planner yet,
   * and the planner can accept one, send immediately
   */
  inline void manage_manual_move() {
    if (manual_move_axis != (int8_t)NO_AXIS && millis() >= manual_move_start_time && !planner.is_full()) {
      #if MECH(DELTA)
        calculate_delta(current_position);
        planner.buffer_line(delta[TOWER_1], delta[TOWER_2], delta[TOWER_3], current_position[E_AXIS], manual_feedrate[manual_move_axis]/60, active_extruder, active_driver);
      #else
        planner.buffer_line(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS], current_position[E_AXIS], manual_feedrate[manual_move_axis]/60, active_extruder, active_driver);
      #endif
      manual_move_axis = (int8_t)NO_AXIS;
    }
  }

  /**
   * Set a flag that lcd_update() should start a move
   * to "current_position" after a short delay.
   */
  inline void manual_move_to_current(AxisEnum axis) {
    manual_move_start_time = millis() + 500UL; // 1/2 second delay
    manual_move_axis = (int8_t)axis;
  }

  /**
   *
   * "Prepare" > "Move Axis" submenu
   *
   */

  float move_menu_scale;

  static void _lcd_move_xyz(const char* name, AxisEnum axis, float min, float max) {
    ENCODER_DIRECTION_NORMAL();
    if (encoderPosition) {
      refresh_cmd_timeout();
      current_position[axis] += float((int32_t)encoderPosition) * move_menu_scale;
      if (SOFTWARE_MIN_ENDSTOPS) NOLESS(current_position[axis], min);
      if (SOFTWARE_MAX_ENDSTOPS) NOMORE(current_position[axis], max);
      encoderPosition = 0;
      manual_move_to_current(axis);
      lcdDrawUpdate = LCDVIEW_REDRAW_NOW;
    }
    if (lcdDrawUpdate) lcd_implementation_drawedit(name, ftostr41sign(current_position[axis]));
    if (LCD_CLICKED) lcd_goto_previous_menu(true);
  }

  #if MECH(DELTA)
    static float delta_clip_radius_2 = (DELTA_PRINTABLE_RADIUS) * (DELTA_PRINTABLE_RADIUS);
    static int delta_clip( float a ) { return sqrt(delta_clip_radius_2 - a * a); }
    static void lcd_move_x() { int clip = delta_clip(current_position[Y_AXIS]); _lcd_move_xyz(PSTR(MSG_MOVE_X), X_AXIS, max(sw_endstop_min[X_AXIS], -clip), min(sw_endstop_max[X_AXIS], clip)); }
    static void lcd_move_y() { int clip = delta_clip(current_position[X_AXIS]); _lcd_move_xyz(PSTR(MSG_MOVE_Y), Y_AXIS, max(sw_endstop_min[Y_AXIS], -clip), min(sw_endstop_max[Y_AXIS], clip)); }
  #else
    static void lcd_move_x() { _lcd_move_xyz(PSTR(MSG_MOVE_X), X_AXIS, sw_endstop_min[X_AXIS], sw_endstop_max[X_AXIS]); }
    static void lcd_move_y() { _lcd_move_xyz(PSTR(MSG_MOVE_Y), Y_AXIS, sw_endstop_min[Y_AXIS], sw_endstop_max[Y_AXIS]); }
  #endif
  static void lcd_move_z() { _lcd_move_xyz(PSTR(MSG_MOVE_Z), Z_AXIS, sw_endstop_min[Z_AXIS], sw_endstop_max[Z_AXIS]); }
  static void lcd_move_e(
    #if EXTRUDERS > 1
      uint8_t e
    #endif
  ) {
    ENCODER_DIRECTION_NORMAL();
    #if EXTRUDERS > 1
      unsigned short original_active_extruder = active_extruder;
      active_extruder = e;
    #endif
    if (encoderPosition) {
      #if ENABLED(IDLE_OOZING_PREVENT)
        IDLE_OOZING_retract(false);
      #endif
      current_position[E_AXIS] += float((int32_t)encoderPosition) * move_menu_scale;
      encoderPosition = 0;
      manual_move_to_current(E_AXIS);
      lcdDrawUpdate = LCDVIEW_REDRAW_NOW;
    }
    if (lcdDrawUpdate) {
      PGM_P pos_label;
      #if EXTRUDERS == 1
        pos_label = PSTR(MSG_MOVE_E);
      #else
        switch (e) {
          case 0: pos_label = PSTR(MSG_MOVE_E "0"); break;
          case 1: pos_label = PSTR(MSG_MOVE_E "1"); break;
          #if EXTRUDERS > 2
            case 2: pos_label = PSTR(MSG_MOVE_E "2"); break;
            #if EXTRUDERS > 3
              case 3: pos_label = PSTR(MSG_MOVE_E "3"); break;
            #endif // EXTRUDERS > 3
          #endif // EXTRUDERS > 2
        }
      #endif // EXTRUDERS > 1
      lcd_implementation_drawedit(pos_label, ftostr41sign(current_position[E_AXIS]));
    }
    if (LCD_CLICKED) lcd_goto_previous_menu(true);
    #if EXTRUDERS > 1
      active_extruder = original_active_extruder;
    #endif
  }

  #if EXTRUDERS > 1
    static void lcd_move_e0() { lcd_move_e(0); }
    static void lcd_move_e1() { lcd_move_e(1); }
    #if EXTRUDERS > 2
      static void lcd_move_e2() { lcd_move_e(2); }
      #if EXTRUDERS > 3
        static void lcd_move_e3() { lcd_move_e(3); }
      #endif
    #endif
  #endif // EXTRUDERS > 1

  /**
   *
   * "Prepare" > "Move Xmm" > "Move XYZ" submenu
   *
   */

  #if MECH(DELTA) || MECH(SCARA)
    #define _MOVE_XYZ_ALLOWED (axis_homed[X_AXIS] && axis_homed[Y_AXIS] && axis_homed[Z_AXIS])
  #else
    #define _MOVE_XYZ_ALLOWED true
  #endif

  static void _lcd_move_menu_axis() {
    START_MENU();
    MENU_ITEM(back, MSG_MOVE_AXIS);

    if (_MOVE_XYZ_ALLOWED) {
      MENU_ITEM(submenu, MSG_MOVE_X, lcd_move_x);
      MENU_ITEM(submenu, MSG_MOVE_Y, lcd_move_y);
      MENU_ITEM(submenu, MSG_MOVE_Z, lcd_move_z);
    }
    if (move_menu_scale < 10.0) {
      #if EXTRUDERS == 1
        MENU_ITEM(submenu, MSG_MOVE_E, lcd_move_e);
      #else
        MENU_ITEM(submenu, MSG_MOVE_E "0", lcd_move_e0);
        MENU_ITEM(submenu, MSG_MOVE_E "1", lcd_move_e1);
        #if EXTRUDERS > 2
          MENU_ITEM(submenu, MSG_MOVE_E "2", lcd_move_e2);
          #if EXTRUDERS > 3
            MENU_ITEM(submenu, MSG_MOVE_E "3", lcd_move_e3);
          #endif
        #endif
      #endif // EXTRUDERS > 1
    }
    END_MENU();
  }

  static void lcd_move_menu_10mm() {
    move_menu_scale = 10.0;
    _lcd_move_menu_axis();
  }
  static void lcd_move_menu_1mm() {
    move_menu_scale = 1.0;
    _lcd_move_menu_axis();
  }
  static void lcd_move_menu_01mm() {
    move_menu_scale = 0.1;
    _lcd_move_menu_axis();
  }

  /**
   *
   * "Prepare" > "Move Axis" submenu
   *
   */

  static void lcd_move_menu() {
    START_MENU();
    MENU_ITEM(back, MSG_PREPARE);

    if (_MOVE_XYZ_ALLOWED)
      MENU_ITEM(submenu, MSG_MOVE_10MM, lcd_move_menu_10mm);

    MENU_ITEM(submenu, MSG_MOVE_1MM, lcd_move_menu_1mm);
    MENU_ITEM(submenu, MSG_MOVE_01MM, lcd_move_menu_01mm);
    // TODO:X,Y,Z,E
    END_MENU();
  }

  /**
   *
   * "Control" submenu
   *
   */

  static void lcd_control_menu() {
    START_MENU();
    MENU_ITEM(back, MSG_MAIN);
    #if DISABLED(LASERBEAM)
      MENU_ITEM(submenu, MSG_TEMPERATURE, lcd_control_temperature_menu);
    #endif
    MENU_ITEM(submenu, MSG_MOTION, lcd_control_motion_menu);
    #if DISABLED(LASERBEAM)
      MENU_ITEM(submenu, MSG_FILAMENT, lcd_control_volumetric_menu);
    #endif

    #if HAS(LCD_CONTRAST)
      //MENU_ITEM_EDIT(int3, MSG_CONTRAST, &lcd_contrast, 0, 63);
      MENU_ITEM(submenu, MSG_CONTRAST, lcd_set_contrast);
    #endif
    #if ENABLED(FWRETRACT)
      MENU_ITEM(submenu, MSG_RETRACT, lcd_control_retract_menu);
    #endif
    #if ENABLED(EEPROM_SETTINGS)
      MENU_ITEM(function, MSG_STORE_EPROM, Config_StoreSettings);
      MENU_ITEM(function, MSG_LOAD_EPROM, Config_RetrieveSettings);
    #endif
    MENU_ITEM(function, MSG_RESTORE_FAILSAFE, Config_ResetDefault);
    END_MENU();
  }

  /**
   *
   * "Statistics" submenu
   *
   */

  static void lcd_stats_menu() {
    char row[30];
    int day = print_job_counter.data.printer_usage_seconds / 60 / 60 / 24, hours = (print_job_counter.data.printer_usage_seconds / 60 / 60) % 24, minutes = (print_job_counter.data.printer_usage_seconds / 60) % 60;
    sprintf_P(row, PSTR(MSG_ONFOR " %id %ih %im"), day, hours, minutes);
    LCD_Printpos(0, 0); lcd_print(row);
    #if HAS(POWER_CONSUMPTION_SENSOR)
      sprintf_P(row, PSTR(MSG_PWRCONSUMED " %iWh"), power_consumption_hour);
      LCD_Printpos(0, 1); lcd_print(row);
    #endif
    char lung[30];
    unsigned int  kmeter = (long)print_job_counter.data.printer_usage_filament / 1000 / 1000,
                  meter = ((long)print_job_counter.data.printer_usage_filament / 1000) % 1000,
                  centimeter = ((long)print_job_counter.data.printer_usage_filament / 10) % 100,
                  millimeter = ((long)print_job_counter.data.printer_usage_filament) % 10;
    sprintf_P(lung, PSTR(MSG_FILCONSUMED "%i Km %i m %i cm %i mm"), kmeter, meter, centimeter, millimeter);
    LCD_Printpos(0, 2); lcd_print(lung);
    if (LCD_CLICKED) lcd_goto_screen(lcd_main_menu);
  }

  /**
   *
   * "Temperature" submenu
   *
   */

  #if ENABLED(PID_AUTOTUNE_MENU)

    #if ENABLED(PIDTEMP)
      int autotune_temp[HOTENDS] = ARRAY_BY_HOTENDS1(150);
      const int heater_maxtemp[HOTENDS] = ARRAY_BY_HOTENDS(HEATER_0_MAXTEMP, HEATER_1_MAXTEMP, HEATER_2_MAXTEMP, HEATER_3_MAXTEMP);
    #endif

    #if ENABLED(PIDTEMPBED)
      int autotune_temp_bed = 70;
    #endif

    static void _lcd_autotune(int h) {
      char cmd[30];
      sprintf_P(cmd, PSTR("M303 U1 H%i S%i"), h,
        #if HAS_PID_FOR_BOTH
          h < 0 ? autotune_temp_bed : autotune_temp[h]
        #elif ENABLED(PIDTEMPBED)
          autotune_temp_bed
        #else
          autotune_temp[h]
        #endif
      );
      enqueue_and_echo_command(cmd);
    }

  #endif //PID_AUTOTUNE_MENU

  #if ENABLED(PIDTEMP)

    // Helpers for editing PID Ki & Kd values
    // grab the PID value out of the temp variable; scale it; then update the PID driver
    void copy_and_scalePID_i(int h) {
      PID_PARAM(Ki, h) = scalePID_i(raw_Ki);
      updatePID();
    }
    void copy_and_scalePID_d(int h) {
      PID_PARAM(Kd, h) = scalePID_d(raw_Kd);
      updatePID();
    }
    #define _PIDTEMP_BASE_FUNCTIONS(hindex) \
      void copy_and_scalePID_i_H ## hindex() { copy_and_scalePID_i(hindex); } \
      void copy_and_scalePID_d_H ## hindex() { copy_and_scalePID_d(hindex); }

    #if ENABLED(PID_AUTOTUNE_MENU)
      #define _PIDTEMP_FUNCTIONS(hindex) \
        _PIDTEMP_BASE_FUNCTIONS(hindex); \
        void lcd_autotune_callback_H ## hindex() { _lcd_autotune(hindex); }
    #else
      #define _PIDTEMP_FUNCTIONS(hindex) _PIDTEMP_BASE_FUNCTIONS(hindex)
    #endif

    _PIDTEMP_FUNCTIONS(0);
    #if HOTENDS > 1
      _PIDTEMP_FUNCTIONS(1);
      #if HOTENDS > 2
        _PIDTEMP_FUNCTIONS(2);
        #if HOTENDS > 3
          _PIDTEMP_FUNCTIONS(3);
        #endif // HOTENDS > 3
      #endif // HOTENDS > 2
    #endif // HOTENDS > 1

  #endif // PIDTEMP

  #if DISABLED(LASERBEAM)

    /**
     *
     * "Control" > "Temperature" submenu
     *
     */
    static void lcd_control_temperature_menu() {
      START_MENU();

      //
      // ^ Control
      //
      MENU_ITEM(back, MSG_CONTROL);

      //
      // Nozzle:
      //
      #if HOTENDS == 1
        #if TEMP_SENSOR_0 != 0
          MENU_MULTIPLIER_ITEM_EDIT_CALLBACK(int3, MSG_NOZZLE, &target_temperature[0], 0, HEATER_0_MAXTEMP - 15, watch_temp_callback_E0);
        #endif
      #else // HOTENDS > 1
        #if TEMP_SENSOR_0 != 0
          MENU_MULTIPLIER_ITEM_EDIT_CALLBACK(int3, MSG_NOZZLE "0", &target_temperature[0], 0, HEATER_0_MAXTEMP - 15, watch_temp_callback_E0);
        #endif
        #if TEMP_SENSOR_1 != 0
          MENU_MULTIPLIER_ITEM_EDIT_CALLBACK(int3, MSG_NOZZLE "1", &target_temperature[1], 0, HEATER_1_MAXTEMP - 15, watch_temp_callback_E1);
        #endif
        #if HOTENDS > 2
          #if TEMP_SENSOR_2 != 0
            MENU_MULTIPLIER_ITEM_EDIT_CALLBACK(int3, MSG_NOZZLE "2", &target_temperature[2], 0, HEATER_2_MAXTEMP - 15, watch_temp_callback_E2);
          #endif
          #if HOTENDS > 3
            #if TEMP_SENSOR_3 != 0
              MENU_MULTIPLIER_ITEM_EDIT_CALLBACK(int3, MSG_NOZZLE "3", &target_temperature[3], 0, HEATER_3_MAXTEMP - 15, watch_temp_callback_E3);
            #endif
          #endif // HOTENDS > 3
        #endif // HOTENDS > 2
      #endif // HOTENDS > 1

      //
      // Bed:
      //
      #if TEMP_SENSOR_BED != 0
        MENU_MULTIPLIER_ITEM_EDIT(int3, MSG_BED, &target_temperature_bed, 0, BED_MAXTEMP - 15);
      #endif

      //
      // Chamber:
      //
      #if TEMP_SENSOR_CHAMBER != 0
        MENU_MULTIPLIER_ITEM_EDIT(int3, MSG_CHAMBER, &target_temperature_chamber, 0, CHAMBER_MAXTEMP - 15);
      #endif

      //
      // Cooler:
      //
      #if TEMP_SENSOR_COOLER != 0
        MENU_MULTIPLIER_ITEM_EDIT(int3, MSG_COOLER, &target_temperature_cooler, 0, COOLER_MAXTEMP - 15);
      #endif

      //
      // Fan Speed:
      //
      MENU_MULTIPLIER_ITEM_EDIT(int3, MSG_FAN_SPEED, &fanSpeed, 0, 255);

      //
      // Autotemp, Min, Max, Fact
      //
      #if ENABLED(AUTOTEMP) && (TEMP_SENSOR_0 != 0)
        MENU_ITEM_EDIT(bool, MSG_AUTOTEMP, &planner.autotemp_enabled);
        MENU_ITEM_EDIT(float3, MSG_MIN, &planner.autotemp_min, 0, HEATER_0_MAXTEMP - 15);
        MENU_ITEM_EDIT(float3, MSG_MAX, &planner.autotemp_max, 0, HEATER_0_MAXTEMP - 15);
        MENU_ITEM_EDIT(float32, MSG_FACTOR, &planner.autotemp_factor, 0.0, 1.0);
      #endif

      //
      // PID-P, PID-I, PID-D, PID-C, PID Autotune
      // PID-P H1, PID-I H1, PID-D H1, PID-C H1, PID Autotune H1
      // PID-P H2, PID-I H2, PID-D H2, PID-C H2, PID Autotune H2
      // PID-P H3, PID-I H3, PID-D H3, PID-C H3, PID Autotune H3
      // PID-P H4, PID-I H4, PID-D H4, PID-C H4, PID Autotune H4
      //
      #if ENABLED(PIDTEMP)
        #define _PID_BASE_MENU_ITEMS(HLABEL, hindex) \
          raw_Ki = unscalePID_i(PID_PARAM(Ki, hindex)); \
          raw_Kd = unscalePID_d(PID_PARAM(Kd, hindex)); \
          MENU_ITEM_EDIT(float52, MSG_PID_P HLABEL, &PID_PARAM(Kp, hindex), 1, 9990); \
          MENU_ITEM_EDIT_CALLBACK(float52, MSG_PID_I HLABEL, &raw_Ki, 0.01, 9990, copy_and_scalePID_i_H ## hindex); \
          MENU_ITEM_EDIT_CALLBACK(float52, MSG_PID_D HLABEL, &raw_Kd, 1, 9990, copy_and_scalePID_d_H ## hindex)

        #if ENABLED(PID_ADD_EXTRUSION_RATE)
          #define _PID_MENU_ITEMS(HLABEL, hindex) \
            _PID_BASE_MENU_ITEMS(HLABEL, hindex); \
            MENU_ITEM_EDIT(float3, MSG_PID_C HLABEL, &PID_PARAM(Kc, hindex), 1, 9990)
        #else
          #define _PID_MENU_ITEMS(HLABEL, hindex) _PID_BASE_MENU_ITEMS(HLABEL, hindex)
        #endif

        #if ENABLED(PID_AUTOTUNE_MENU)
          #define PID_MENU_ITEMS(HLABEL, hindex) \
            _PID_MENU_ITEMS(HLABEL, hindex); \
            MENU_MULTIPLIER_ITEM_EDIT_CALLBACK(int3, SERIAL_PID_AUTOTUNE HLABEL, &autotune_temp[hindex], 150, heater_maxtemp[hindex] - 15, lcd_autotune_callback_H ## hindex)
        #else
          #define PID_MENU_ITEMS(HLABEL, hindex) _PID_MENU_ITEMS(HLABEL, hindex)
        #endif

        PID_MENU_ITEMS("", 0);
        #if HOTENDS > 1
          PID_MENU_ITEMS(MSG_H1, 1);
          #if HOTENDS > 2
            PID_MENU_ITEMS(MSG_H2, 2);
            #if HOTENDS > 3
              PID_MENU_ITEMS(MSG_H3, 3);
            #endif // HOTENDS > 3
          #endif // HOTENDS > 2
        #endif // HOTENDS > 1
      #endif // PIDTEMP

      //
      // Idle oozing
      //
      #if ENABLED(IDLE_OOZING_PREVENT)
        MENU_ITEM_EDIT(bool, MSG_IDLEOOZING, &IDLE_OOZING_enabled);
      #endif

      //
      // Preheat PLA conf
      //
      MENU_ITEM(submenu, MSG_PREHEAT_PLA_SETTINGS, lcd_control_temperature_preheat_pla_settings_menu);

      //
      // Preheat ABS conf
      //
      MENU_ITEM(submenu, MSG_PREHEAT_ABS_SETTINGS, lcd_control_temperature_preheat_abs_settings_menu);

      //
      // Preheat GUM conf
      //
      MENU_ITEM(submenu, MSG_PREHEAT_GUM_SETTINGS, lcd_control_temperature_preheat_gum_settings_menu);
      END_MENU();
    }

    /**
     *
     * "Temperature" > "Preheat PLA conf" submenu
     *
     */
    static void lcd_control_temperature_preheat_pla_settings_menu() {
      START_MENU();
      MENU_ITEM(back, MSG_TEMPERATURE);
      MENU_ITEM_EDIT(int3, MSG_FAN_SPEED, &plaPreheatFanSpeed, 0, 255);
      #if TEMP_SENSOR_0 != 0
        MENU_ITEM_EDIT(int3, MSG_NOZZLE, &plaPreheatHotendTemp, HEATER_0_MINTEMP, HEATER_0_MAXTEMP - 15);
      #endif
      #if TEMP_SENSOR_BED != 0
        MENU_ITEM_EDIT(int3, MSG_BED, &plaPreheatHPBTemp, BED_MINTEMP, BED_MAXTEMP - 15);
      #endif
      #if ENABLED(EEPROM_SETTINGS)
        MENU_ITEM(function, MSG_STORE_EPROM, Config_StoreSettings);
      #endif
      END_MENU();
    }

    /**
     *
     * "Temperature" > "Preheat ABS conf" submenu
     *
     */
    static void lcd_control_temperature_preheat_abs_settings_menu() {
      START_MENU();
      MENU_ITEM(back, MSG_TEMPERATURE);
      MENU_ITEM_EDIT(int3, MSG_FAN_SPEED, &absPreheatFanSpeed, 0, 255);
      #if TEMP_SENSOR_0 != 0
        MENU_ITEM_EDIT(int3, MSG_NOZZLE, &absPreheatHotendTemp, HEATER_0_MINTEMP, HEATER_0_MAXTEMP - 15);
      #endif
      #if TEMP_SENSOR_BED != 0
        MENU_ITEM_EDIT(int3, MSG_BED, &absPreheatHPBTemp, BED_MINTEMP, BED_MAXTEMP - 15);
      #endif
      #if ENABLED(EEPROM_SETTINGS)
        MENU_ITEM(function, MSG_STORE_EPROM, Config_StoreSettings);
      #endif
      END_MENU();
    }

    /**
     *
     * "Temperature" > "Preheat GUM conf" submenu
     *
     */
    static void lcd_control_temperature_preheat_gum_settings_menu() {
      START_MENU();
      MENU_ITEM(back, MSG_TEMPERATURE);
      MENU_ITEM_EDIT(int3, MSG_FAN_SPEED, &gumPreheatFanSpeed, 0, 255);
      #if TEMP_SENSOR_0 != 0
        MENU_ITEM_EDIT(int3, MSG_NOZZLE, &gumPreheatHotendTemp, HEATER_0_MINTEMP, HEATER_0_MAXTEMP - 15);
      #endif
      #if TEMP_SENSOR_BED != 0
        MENU_ITEM_EDIT(int3, MSG_BED, &gumPreheatHPBTemp, BED_MINTEMP, BED_MAXTEMP - 15);
      #endif
      #if ENABLED(EEPROM_SETTINGS)
        MENU_ITEM(function, MSG_STORE_EPROM, Config_StoreSettings);
      #endif
      END_MENU();
    }

  #endif // !LASERBEAM

  /**
   *
   * "Control" > "Motion" submenu
   *
   */
  static void lcd_control_motion_menu() {
    START_MENU();
    MENU_ITEM(back, MSG_CONTROL);
    #if HAS(BED_PROBE)
      MENU_ITEM_EDIT(float32, MSG_ZPROBE_ZOFFSET, &zprobe_zoffset, Z_PROBE_OFFSET_RANGE_MIN, Z_PROBE_OFFSET_RANGE_MAX);
    #endif
    // Manual bed leveling, Bed Z:
    #if ENABLED(MANUAL_BED_LEVELING)
      MENU_ITEM_EDIT(float43, MSG_BED_Z, &mbl.z_offset, -1, 1);
    #endif
    MENU_ITEM_EDIT(float5, MSG_ACC, &planner.acceleration, 10, 99000);
    MENU_ITEM_EDIT(float3, MSG_VXY_JERK, &planner.max_xy_jerk, 1, 990);
    #if MECH(DELTA)
      MENU_ITEM_EDIT(float3, MSG_VZ_JERK, &planner.max_z_jerk, 1, 990);
    #else
      MENU_ITEM_EDIT(float52, MSG_VZ_JERK, &planner.max_z_jerk, 0.1, 990);
    #endif
    MENU_ITEM_EDIT(float3, MSG_VMAX MSG_X, &planner.max_feedrate[X_AXIS], 1, 999);
    MENU_ITEM_EDIT(float3, MSG_VMAX MSG_Y, &planner.max_feedrate[Y_AXIS], 1, 999);
    MENU_ITEM_EDIT(float3, MSG_VMAX MSG_Z, &planner.max_feedrate[Z_AXIS], 1, 999);
    MENU_ITEM_EDIT(float3, MSG_VMIN, &planner.min_feedrate, 0, 999);
    MENU_ITEM_EDIT(float3, MSG_VTRAV_MIN, &planner.min_travel_feedrate, 0, 999);
    MENU_ITEM_EDIT_CALLBACK(long5, MSG_AMAX MSG_X, &planner.max_acceleration_mm_per_s2[X_AXIS], 100, 99000, planner.reset_acceleration_rates);
    MENU_ITEM_EDIT_CALLBACK(long5, MSG_AMAX MSG_Y, &planner.max_acceleration_mm_per_s2[Y_AXIS], 100, 99000, planner.reset_acceleration_rates);
    MENU_ITEM_EDIT_CALLBACK(long5, MSG_AMAX MSG_Z, &planner.max_acceleration_mm_per_s2[Z_AXIS], 10, 99000, planner.reset_acceleration_rates);
    MENU_ITEM_EDIT(float5, MSG_A_TRAVEL, &planner.travel_acceleration, 100, 99000);
    MENU_ITEM_EDIT(float52, MSG_XSTEPS, &planner.axis_steps_per_mm[X_AXIS], 5, 9999);
    MENU_ITEM_EDIT(float52, MSG_YSTEPS, &planner.axis_steps_per_mm[Y_AXIS], 5, 9999);
    MENU_ITEM_EDIT(float51, MSG_ZSTEPS, &planner.axis_steps_per_mm[Z_AXIS], 5, 9999);
    #if EXTRUDERS > 0
      MENU_ITEM_EDIT(float3, MSG_VE_JERK MSG_E "0", &planner.max_e_jerk[0], 1, 990);
      MENU_ITEM_EDIT(float3, MSG_VMAX MSG_E "0", &planner.max_feedrate[E_AXIS], 1, 999);
      MENU_ITEM_EDIT(long5, MSG_AMAX MSG_E "0", &planner.max_acceleration_mm_per_s2[E_AXIS], 100, 99000);
      MENU_ITEM_EDIT(float5, MSG_A_RETRACT MSG_E "0", &planner.retract_acceleration[0], 100, 99000);
      MENU_ITEM_EDIT(float51, MSG_E0STEPS, &planner.axis_steps_per_mm[E_AXIS], 5, 9999);
      #if EXTRUDERS > 1
        MENU_ITEM_EDIT(float3, MSG_VE_JERK MSG_E "1", &planner.max_e_jerk[1], 1, 990);
        MENU_ITEM_EDIT(float3, MSG_VMAX MSG_E "1", &planner.max_feedrate[E_AXIS + 1], 1, 999);
        MENU_ITEM_EDIT(long5, MSG_AMAX MSG_E "1", &planner.max_acceleration_mm_per_s2[E_AXIS + 1], 100, 99000);
        MENU_ITEM_EDIT(float5, MSG_A_RETRACT MSG_E "1", &planner.retract_acceleration[1], 100, 99000);
        MENU_ITEM_EDIT(float51, MSG_E1STEPS, &planner.axis_steps_per_mm[E_AXIS + 1], 5, 9999);
        #if EXTRUDERS > 2
          MENU_ITEM_EDIT(float3, MSG_VE_JERK MSG_E "2", &planner.max_e_jerk[2], 1, 990);
          MENU_ITEM_EDIT(float3, MSG_VMAX MSG_E "2", &planner.max_feedrate[E_AXIS + 2], 1, 999);
          MENU_ITEM_EDIT(long5, MSG_AMAX MSG_E "2", &planner.max_acceleration_mm_per_s2[E_AXIS + 2], 100, 99000);
          MENU_ITEM_EDIT(float5, MSG_A_RETRACT MSG_E "2", &planner.retract_acceleration[2], 100, 99000);
          MENU_ITEM_EDIT(float51, MSG_E2STEPS, &planner.axis_steps_per_mm[E_AXIS + 2], 5, 9999);
          #if EXTRUDERS > 3
            MENU_ITEM_EDIT(float3, MSG_VE_JERK MSG_E  "3", &planner.max_e_jerk[3], 1, 990);
            MENU_ITEM_EDIT(float3, MSG_VMAX MSG_E "3", &planner.max_feedrate[E_AXIS + 3], 1, 999);
            MENU_ITEM_EDIT(long5, MSG_AMAX MSG_E "3", &planner.max_acceleration_mm_per_s2[E_AXIS + 3], 100, 99000);
            MENU_ITEM_EDIT(float5, MSG_A_RETRACT MSG_E "3", &planner.retract_acceleration[3], 100, 99000);
            MENU_ITEM_EDIT(float51, MSG_E3STEPS, &planner.axis_steps_per_mm[E_AXIS + 3], 5, 9999);
          #endif // EXTRUDERS > 3
        #endif // EXTRUDERS > 2
      #endif // EXTRUDERS > 1
    #endif // EXTRUDERS > 0

    #if ENABLED(ABORT_ON_ENDSTOP_HIT_FEATURE_ENABLED)
      MENU_ITEM_EDIT(bool, MSG_ENDSTOP_ABORT, &abort_on_endstop_hit);
    #endif
    #if MECH(SCARA)
      MENU_ITEM_EDIT(float74, MSG_XSCALE, &axis_scaling[X_AXIS], 0.5, 2);
      MENU_ITEM_EDIT(float74, MSG_YSCALE, &axis_scaling[Y_AXIS], 0.5, 2);
    #endif
    END_MENU();
  }

  /**
   *
   * "Control" > "Filament" submenu
   *
   */
  #if DISABLED(LASERBEAM)
    static void lcd_control_volumetric_menu() {
      START_MENU();
      MENU_ITEM(back, MSG_CONTROL);

      MENU_ITEM_EDIT_CALLBACK(bool, MSG_VOLUMETRIC_ENABLED, &volumetric_enabled, calculate_volumetric_multipliers);

      if (volumetric_enabled) {
        #if EXTRUDERS == 1
          MENU_MULTIPLIER_ITEM_EDIT_CALLBACK(float43, MSG_FILAMENT_SIZE_EXTRUDER, &filament_size[0], DEFAULT_NOMINAL_FILAMENT_DIA - .5, DEFAULT_NOMINAL_FILAMENT_DIA + .5, calculate_volumetric_multipliers);
        #else // EXTRUDERS > 1
          MENU_MULTIPLIER_ITEM_EDIT_CALLBACK(float43, MSG_FILAMENT_SIZE_EXTRUDER " 0", &filament_size[0], DEFAULT_NOMINAL_FILAMENT_DIA - .5, DEFAULT_NOMINAL_FILAMENT_DIA + .5, calculate_volumetric_multipliers);
          MENU_MULTIPLIER_ITEM_EDIT_CALLBACK(float43, MSG_FILAMENT_SIZE_EXTRUDER " 1", &filament_size[1], DEFAULT_NOMINAL_FILAMENT_DIA - .5, DEFAULT_NOMINAL_FILAMENT_DIA + .5, calculate_volumetric_multipliers);
          #if EXTRUDERS > 2
            MENU_MULTIPLIER_ITEM_EDIT_CALLBACK(float43, MSG_FILAMENT_SIZE_EXTRUDER " 2", &filament_size[2], DEFAULT_NOMINAL_FILAMENT_DIA - .5, DEFAULT_NOMINAL_FILAMENT_DIA + .5, calculate_volumetric_multipliers);
            #if EXTRUDERS > 3
              MENU_MULTIPLIER_ITEM_EDIT_CALLBACK(float43, MSG_FILAMENT_SIZE_EXTRUDER " 3", &filament_size[3], DEFAULT_NOMINAL_FILAMENT_DIA - .5, DEFAULT_NOMINAL_FILAMENT_DIA + .5, calculate_volumetric_multipliers);
            #endif // EXTRUDERS > 3
          #endif // EXTRUDERS > 2
        #endif // EXTRUDERS > 1
      }

      END_MENU();
    }
  #endif // !LASERBEAM

  /**
   *
   * "Control" > "Contrast" submenu
   *
   */
  #if HAS(LCD_CONTRAST)
    static void lcd_set_contrast() {
      ENCODER_DIRECTION_NORMAL();
      if (encoderPosition) {
        set_lcd_contrast(lcd_contrast + encoderPosition);
        encoderPosition = 0;
        lcdDrawUpdate = LCDVIEW_REDRAW_NOW;
      }
      if (lcdDrawUpdate) {
        lcd_implementation_drawedit(PSTR(MSG_CONTRAST),
          #if LCD_CONTRAST_MAX >= 100
            itostr3(lcd_contrast)
          #else
            itostr2(lcd_contrast)
          #endif
        );
      }
      if (LCD_CLICKED) lcd_goto_previous_menu(true);
    }
  #endif // HAS(LCD_CONTRAST)

  /**
   *
   * "Control" > "Retract" submenu
   *
   */
  #if ENABLED(FWRETRACT)
    static void lcd_control_retract_menu() {
      START_MENU();
      MENU_ITEM(back, MSG_CONTROL);
      MENU_ITEM_EDIT(bool, MSG_AUTORETRACT, &autoretract_enabled);
      MENU_ITEM_EDIT(float52, MSG_CONTROL_RETRACT, &retract_length, 0, 100);
      #if EXTRUDERS > 1
        MENU_ITEM_EDIT(float52, MSG_CONTROL_RETRACT_SWAP, &retract_length_swap, 0, 100);
      #endif
      MENU_ITEM_EDIT(float3, MSG_CONTROL_RETRACTF, &retract_feedrate, 1, 999);
      MENU_ITEM_EDIT(float52, MSG_CONTROL_RETRACT_ZLIFT, &retract_zlift, 0, 999);
      MENU_ITEM_EDIT(float52, MSG_CONTROL_RETRACT_RECOVER, &retract_recover_length, 0, 100);
      #if EXTRUDERS > 1
        MENU_ITEM_EDIT(float52, MSG_CONTROL_RETRACT_RECOVER_SWAP, &retract_recover_length_swap, 0, 100);
      #endif
      MENU_ITEM_EDIT(float3, MSG_CONTROL_RETRACT_RECOVERF, &retract_recover_feedrate, 1, 999);
      END_MENU();
    }
  #endif // FWRETRACT

  #if ENABLED(LASERBEAM)

    static void lcd_laser_menu() {
      START_MENU();
      MENU_ITEM(back, MSG_MAIN);
      MENU_ITEM(submenu, "Set Focus", lcd_laser_focus_menu);
      MENU_ITEM(submenu, "Test Fire", lcd_laser_test_fire_menu);
      #if ENABLED(LASER_PERIPHERALS)
        if (laser_peripherals_ok()) {
          MENU_ITEM(function, "Turn On Pumps/Fans", action_laser_acc_on);
        }
        else if (!(planner.movesplanned() || IS_SD_PRINTING)) {
          MENU_ITEM(function, "Turn Off Pumps/Fans", action_laser_acc_off);
        }
      #endif // LASER_PERIPHERALS
      END_MENU();
    }

    static void lcd_laser_test_fire_menu() {
      START_MENU();
       MENU_ITEM(back, "Laser Functions");
       MENU_ITEM(function, " 20%  50ms", action_laser_test_20_50ms);
       MENU_ITEM(function, " 20% 100ms", action_laser_test_20_100ms);
       MENU_ITEM(function, "100%  50ms", action_laser_test_100_50ms);
       MENU_ITEM(function, "100% 100ms", action_laser_test_100_100ms);
       MENU_ITEM(function, "Warm-up Laser 2sec", action_laser_test_warm);
       END_MENU();
    }

    static void action_laser_acc_on() { enqueue_and_echo_commands_P(PSTR("M80")); }
    static void action_laser_acc_off() { enqueue_and_echo_commands_P(PSTR("M81")); }
    static void action_laser_test_20_50ms() { laser_test_fire(20, 50); }
    static void action_laser_test_20_100ms() { laser_test_fire(20, 100); }
    static void action_laser_test_100_50ms() { laser_test_fire(100, 50); }
    static void action_laser_test_100_100ms() { laser_test_fire(100, 100); }
    static void action_laser_test_warm() { laser_test_fire(15, 2000); }

    static void laser_test_fire(uint8_t power, int dwell) {
      enqueue_and_echo_commands_P(PSTR("M80"));  // Enable laser accessories since we don't know if its been done (and there's no penalty for doing it again).
      laser_fire(power);
      delay(dwell);
      laser_extinguish();
    }

    float focalLength = 0;
    static void lcd_laser_focus_menu() {
      START_MENU();
      MENU_ITEM(back, "Laser Functions");
      MENU_ITEM(function, "1mm", action_laser_focus_1mm);
      MENU_ITEM(function, "2mm", action_laser_focus_2mm);
      MENU_ITEM(function, "3mm - 1/8in", action_laser_focus_3mm);
      MENU_ITEM(function, "4mm", action_laser_focus_4mm);
      MENU_ITEM(function, "5mm", action_laser_focus_5mm);
      MENU_ITEM(function, "6mm - 1/4in", action_laser_focus_6mm);
      MENU_ITEM(function, "7mm", action_laser_focus_7mm);
      MENU_ITEM_EDIT_CALLBACK(float32, "Custom", &focalLength, 0, LASER_FOCAL_HEIGHT, action_laser_focus_custom);
      END_MENU();
    }

    static void action_laser_focus_custom() { laser_set_focus(focalLength); }
    static void action_laser_focus_1mm() { laser_set_focus(1); }
    static void action_laser_focus_2mm() { laser_set_focus(2); }
    static void action_laser_focus_3mm() { laser_set_focus(3); }
    static void action_laser_focus_4mm() { laser_set_focus(4); }
    static void action_laser_focus_5mm() { laser_set_focus(5); }
    static void action_laser_focus_6mm() { laser_set_focus(6); }
    static void action_laser_focus_7mm() { laser_set_focus(7); }

    static void laser_set_focus(float f_length) {
      if (!axis_homed[Z_AXIS]) {
        enqueue_and_echo_commands_P(PSTR("G28 Z F150"));
      }
      focalLength = f_length;
      float focus = LASER_FOCAL_HEIGHT - f_length;
      char cmd[20];

      sprintf_P(cmd, PSTR("G0 Z%s F150"), ftostr52(focus));
      enqueue_and_echo_commands_P(cmd);
    }

  #endif // LASERBEAM

  #if ENABLED(SDSUPPORT)

    #if !PIN_EXISTS(SD_DETECT)
      static void lcd_sd_refresh() {
        card.mount();
        currentMenuViewOffset = 0;
      }
    #endif

    static void lcd_sd_updir() {
      card.updir();
      currentMenuViewOffset = 0;
    }

    /**
     *
     * "Print from SD" submenu
     *
     */
    void lcd_sdcard_menu() {
      ENCODER_DIRECTION_MENUS();
      if (lcdDrawUpdate == 0 && LCD_CLICKED == 0) return; // nothing to do (so don't thrash the SD card)
      uint16_t fileCnt = card.getnrfilenames();
      START_MENU();
      MENU_ITEM(back, MSG_MAIN);
      card.getWorkDirName();
      if (fullName[0] == '/') {
        #if !PIN_EXISTS(SD_DETECT)
          MENU_ITEM(function, LCD_STR_REFRESH MSG_REFRESH, lcd_sd_refresh);
        #endif
      }
      else {
        MENU_ITEM(function, LCD_STR_FOLDER "..", lcd_sd_updir);
      }

      for (uint16_t i = 0; i < fileCnt; i++) {
        if (_menuItemNr == _lineNr) {
          card.getfilename(
            #if ENABLED(SDCARD_RATHERRECENTFIRST)
              fileCnt-1 -
            #endif
            i
          );

          if (card.filenameIsDir)
            MENU_ITEM(sddirectory, MSG_CARD_MENU, fullName);
          else
            MENU_ITEM(sdfile, MSG_CARD_MENU, fullName);
        }
        else {
          MENU_ITEM_DUMMY();
        }
      }
      END_MENU();
    }

  #endif // SDSUPPORT

  #if ENABLED(FILAMENT_CHANGE_FEATURE)

    static void lcd_filament_change_nothing() {
    }

    static void lcd_filament_change_resume_print() {
      filament_change_menu_response = FILAMENT_CHANGE_RESPONSE_RESUME_PRINT;
    }

    static void lcd_filament_change_extrude_more() {
      filament_change_menu_response = FILAMENT_CHANGE_RESPONSE_EXTRUDE_MORE;
    }

    static void lcd_filament_change_option_menu() {
      START_MENU();
      MENU_ITEM(function, MSG_FILAMENT_CHANGE_OPTION_HEADER, lcd_filament_change_nothing);
      MENU_ITEM(function, MSG_FILAMENT_CHANGE_OPTION_RESUME, lcd_filament_change_resume_print);
      MENU_ITEM(function, MSG_FILAMENT_CHANGE_OPTION_EXTRUDE, lcd_filament_change_extrude_more);
      END_MENU();
    }

    static void lcd_filament_change_init_message() {
      START_MENU();
      MENU_ITEM(function, MSG_FILAMENT_CHANGE_HEADER, lcd_filament_change_nothing);
      MENU_ITEM(function, MSG_FILAMENT_CHANGE_INIT_1, lcd_filament_change_nothing);
      MENU_ITEM(function, MSG_FILAMENT_CHANGE_INIT_2, lcd_filament_change_nothing);
      MENU_ITEM(function, MSG_FILAMENT_CHANGE_INIT_3, lcd_filament_change_nothing);
      END_MENU();
    }

    static void lcd_filament_change_unload_message() {
      START_MENU();
      MENU_ITEM(function, MSG_FILAMENT_CHANGE_HEADER, lcd_filament_change_nothing);
      MENU_ITEM(function, MSG_FILAMENT_CHANGE_UNLOAD_1, lcd_filament_change_nothing);
      MENU_ITEM(function, MSG_FILAMENT_CHANGE_UNLOAD_2, lcd_filament_change_nothing);
      MENU_ITEM(function, MSG_FILAMENT_CHANGE_UNLOAD_3, lcd_filament_change_nothing);
      END_MENU();
    }

    static void lcd_filament_change_insert_message() {
      START_MENU();
      MENU_ITEM(function, MSG_FILAMENT_CHANGE_HEADER, lcd_filament_change_nothing);
      MENU_ITEM(function, MSG_FILAMENT_CHANGE_INSERT_1, lcd_filament_change_nothing);
      MENU_ITEM(function, MSG_FILAMENT_CHANGE_INSERT_2, lcd_filament_change_nothing);
      MENU_ITEM(function, MSG_FILAMENT_CHANGE_INSERT_3, lcd_filament_change_nothing);
      END_MENU();
    }

    static void lcd_filament_change_load_message() {
      START_MENU();
      MENU_ITEM(function, MSG_FILAMENT_CHANGE_HEADER, lcd_filament_change_nothing);
      MENU_ITEM(function, MSG_FILAMENT_CHANGE_LOAD_1, lcd_filament_change_nothing);
      MENU_ITEM(function, MSG_FILAMENT_CHANGE_LOAD_2, lcd_filament_change_nothing);
      MENU_ITEM(function, MSG_FILAMENT_CHANGE_LOAD_3, lcd_filament_change_nothing);
      END_MENU();
    }

    static void lcd_filament_change_extrude_message() {
      START_MENU();
      MENU_ITEM(function, MSG_FILAMENT_CHANGE_HEADER, lcd_filament_change_nothing);
      MENU_ITEM(function, MSG_FILAMENT_CHANGE_EXTRUDE_1, lcd_filament_change_nothing);
      MENU_ITEM(function, MSG_FILAMENT_CHANGE_EXTRUDE_2, lcd_filament_change_nothing);
      MENU_ITEM(function, MSG_FILAMENT_CHANGE_EXTRUDE_3, lcd_filament_change_nothing);
      END_MENU();
    }

    static void lcd_filament_change_resume_message() {
      START_MENU();
      MENU_ITEM(function, MSG_FILAMENT_CHANGE_HEADER, lcd_filament_change_nothing);
      MENU_ITEM(function, MSG_FILAMENT_CHANGE_RESUME_1, lcd_filament_change_nothing);
      MENU_ITEM(function, MSG_FILAMENT_CHANGE_RESUME_2, lcd_filament_change_nothing);
      MENU_ITEM(function, MSG_FILAMENT_CHANGE_RESUME_3, lcd_filament_change_nothing);
      END_MENU();
    }

    void lcd_filament_change_show_message(FilamentChangeMessage message) {
      switch (message) {
        case FILAMENT_CHANGE_MESSAGE_INIT:
          defer_return_to_status = true;
          lcdDrawUpdate = LCDVIEW_CLEAR_CALL_REDRAW;
          lcd_goto_screen(lcd_filament_change_init_message);
          break;
        case FILAMENT_CHANGE_MESSAGE_UNLOAD:
          lcd_goto_screen(lcd_filament_change_unload_message);
          break;
        case FILAMENT_CHANGE_MESSAGE_INSERT:
          lcd_goto_screen(lcd_filament_change_insert_message);
          break;
        case FILAMENT_CHANGE_MESSAGE_LOAD:
          lcd_goto_screen(lcd_filament_change_load_message);
          break;
        case FILAMENT_CHANGE_MESSAGE_EXTRUDE:
          lcd_goto_screen(lcd_filament_change_extrude_message);
          break;
        case FILAMENT_CHANGE_MESSAGE_OPTION:
          while (lcd_clicked()) idle(true);
          filament_change_menu_response = FILAMENT_CHANGE_RESPONSE_WAIT_FOR;
          lcd_goto_screen(lcd_filament_change_option_menu);
          break;
        case FILAMENT_CHANGE_MESSAGE_RESUME:
          lcd_goto_screen(lcd_filament_change_resume_message);
          break;
        case FILAMENT_CHANGE_MESSAGE_STATUS:
          lcd_implementation_clear();
          lcd_return_to_status();
          break;
      }
    }

  #endif // FILAMENT_CHANGE_FEATURE

  /**
   *
   * Functions for editing single values
   *
   * The "menu_edit_type" macro generates the functions needed to edit a numerical value.
   *
   * For example, menu_edit_type(int, int3, itostr3, 1) expands into these functions:
   *
   *   bool _menu_edit_int3();
   *   void menu_edit_int3(); // edit int (interactively)
   *   void menu_edit_callback_int3(); // edit int (interactively) with callback on completion
   *   static void _menu_action_setting_edit_int3(const char* pstr, int* ptr, int minValue, int maxValue);
   *   static void menu_action_setting_edit_int3(const char* pstr, int* ptr, int minValue, int maxValue);
   *   static void menu_action_setting_edit_callback_int3(const char* pstr, int* ptr, int minValue, int maxValue, screenFunc_t callback); // edit int with callback
   *
   * You can then use one of the menu macros to present the edit interface:
   *   MENU_ITEM_EDIT(int3, MSG_SPEED, &feedrate_multiplier, 10, 999)
   *
   * This expands into a more primitive menu item:
   *   MENU_ITEM(setting_edit_int3, MSG_SPEED, PSTR(MSG_SPEED), &feedrate_multiplier, 10, 999)
   *
   *
   * Also: MENU_MULTIPLIER_ITEM_EDIT, MENU_ITEM_EDIT_CALLBACK, and MENU_MULTIPLIER_ITEM_EDIT_CALLBACK
   *
   *       menu_action_setting_edit_int3(PSTR(MSG_SPEED), &feedrate_multiplier, 10, 999)
   */
  #define menu_edit_type(_type, _name, _strFunc, scale) \
    bool _menu_edit_ ## _name () { \
      ENCODER_DIRECTION_NORMAL(); \
      bool isClicked = LCD_CLICKED; \
      if ((int32_t)encoderPosition < 0) encoderPosition = 0; \
      if ((int32_t)encoderPosition > maxEditValue) encoderPosition = maxEditValue; \
      if (lcdDrawUpdate) \
        lcd_implementation_drawedit(editLabel, _strFunc(((_type)((int32_t)encoderPosition + minEditValue)) / scale)); \
      if (isClicked) { \
        *((_type*)editValue) = ((_type)((int32_t)encoderPosition + minEditValue)) / scale; \
        lcd_goto_previous_menu(true); \
      } \
      return isClicked; \
    } \
    void menu_edit_ ## _name () { _menu_edit_ ## _name(); } \
    void menu_edit_callback_ ## _name () { if (_menu_edit_ ## _name ()) (*callbackFunc)(); } \
    static void _menu_action_setting_edit_ ## _name (const char* pstr, _type* ptr, _type minValue, _type maxValue) { \
      lcd_save_previous_menu(); \
      \
      lcdDrawUpdate = LCDVIEW_CLEAR_CALL_REDRAW; \
      \
      editLabel = pstr; \
      editValue = ptr; \
      minEditValue = minValue * scale; \
      maxEditValue = maxValue * scale - minEditValue; \
      encoderPosition = (*ptr) * scale - minEditValue; \
    } \
    static void menu_action_setting_edit_ ## _name (const char* pstr, _type* ptr, _type minValue, _type maxValue) { \
      _menu_action_setting_edit_ ## _name(pstr, ptr, minValue, maxValue); \
      currentScreen = menu_edit_ ## _name; \
    }\
    static void menu_action_setting_edit_callback_ ## _name (const char* pstr, _type* ptr, _type minValue, _type maxValue, screenFunc_t callback) { \
      _menu_action_setting_edit_ ## _name(pstr, ptr, minValue, maxValue); \
      currentScreen = menu_edit_callback_ ## _name; \
      callbackFunc = callback; \
    }
  menu_edit_type(int, int3, itostr3, 1);
  menu_edit_type(float, float3, ftostr3, 1);
  menu_edit_type(float, float32, ftostr32, 100);
  menu_edit_type(float, float43, ftostr43sign, 1000);
  menu_edit_type(float, float5, ftostr5rj, 0.01);
  menu_edit_type(float, float51, ftostr51sign, 10);
  menu_edit_type(float, float52, ftostr52sign, 100);
  menu_edit_type(unsigned long, long5, ftostr5rj, 0.01);

  /**
   *
   * Handlers for RepRap World Keypad input
   *
   */
  #if ENABLED(REPRAPWORLD_KEYPAD)
    static void reprapworld_keypad_move_z_up() {
      encoderPosition = 1;
      move_menu_scale = REPRAPWORLD_KEYPAD_MOVE_STEP;
      lcd_move_z();
    }
    static void reprapworld_keypad_move_z_down() {
      encoderPosition = -1;
      move_menu_scale = REPRAPWORLD_KEYPAD_MOVE_STEP;
      lcd_move_z();
    }
    static void reprapworld_keypad_move_x_left() {
      encoderPosition = -1;
      move_menu_scale = REPRAPWORLD_KEYPAD_MOVE_STEP;
      lcd_move_x();
    }
    static void reprapworld_keypad_move_x_right() {
      encoderPosition = 1;
      move_menu_scale = REPRAPWORLD_KEYPAD_MOVE_STEP;
      lcd_move_x();
    }
    static void reprapworld_keypad_move_y_down() {
      encoderPosition = 1;
      move_menu_scale = REPRAPWORLD_KEYPAD_MOVE_STEP;
      lcd_move_y();
    }
    static void reprapworld_keypad_move_y_up() {
      encoderPosition = -1;
      move_menu_scale = REPRAPWORLD_KEYPAD_MOVE_STEP;
      lcd_move_y();
    }
    static void reprapworld_keypad_move_home() {
      enqueue_and_echo_commands_P(PSTR("G28")); // move all axis home
    }
  #endif // REPRAPWORLD_KEYPAD


  /**
   *
   * Audio feedback for controller clicks
   *
   */

  #if ENABLED(LCD_USE_I2C_BUZZER)
    void lcd_buzz(long duration, uint16_t freq) { // called from buzz() in Marlin_main.cpp where lcd is unknown
      lcd.buzz(duration, freq);
    }
  #endif

  void lcd_quick_feedback() {
    lcdDrawUpdate = LCDVIEW_CLEAR_CALL_REDRAW;
    next_button_update_ms = millis() + 500;

    #if ENABLED(LCD_USE_I2C_BUZZER)
      #if DISABLED(LCD_FEEDBACK_FREQUENCY_HZ)
        #define LCD_FEEDBACK_FREQUENCY_HZ 100
      #endif
      #if DISABLED(LCD_FEEDBACK_FREQUENCY_DURATION_MS)
        #define LCD_FEEDBACK_FREQUENCY_DURATION_MS (1000/6)
      #endif
      lcd.buzz(LCD_FEEDBACK_FREQUENCY_DURATION_MS, LCD_FEEDBACK_FREQUENCY_HZ);
    #elif HAS(BUZZER)
      #if DISABLED(LCD_FEEDBACK_FREQUENCY_HZ)
        #define LCD_FEEDBACK_FREQUENCY_HZ 5000
      #endif
      #if DISABLED(LCD_FEEDBACK_FREQUENCY_DURATION_MS)
        #define LCD_FEEDBACK_FREQUENCY_DURATION_MS 2
      #endif
      buzz(LCD_FEEDBACK_FREQUENCY_DURATION_MS, LCD_FEEDBACK_FREQUENCY_HZ);
    #else
      #if DISABLED(LCD_FEEDBACK_FREQUENCY_DURATION_MS)
        #define LCD_FEEDBACK_FREQUENCY_DURATION_MS 2
      #endif
      HAL::delayMilliseconds(LCD_FEEDBACK_FREQUENCY_DURATION_MS);
    #endif
  }

  /**
   *
   * Menu actions
   *
   */
  static void menu_action_back() { lcd_goto_previous_menu(); }
  static void menu_action_submenu(screenFunc_t func) { lcd_save_previous_menu(); lcd_goto_screen(func); }
  static void menu_action_gcode(const char* pgcode) { enqueue_and_echo_commands_P(pgcode); }
  static void menu_action_function(screenFunc_t func) { (*func)(); }

  #if ENABLED(SDSUPPORT)

    static void menu_action_sdfile(const char* longFilename) {
      char cmd[30];
      char* c;
      sprintf_P(cmd, PSTR("M23 %s"), longFilename);
      for (c = &cmd[4]; *c; c++) *c = tolower(*c);
      enqueue_and_echo_command(cmd);
      enqueue_and_echo_commands_P(PSTR("M24"));
      lcd_return_to_status();
    }

    static void menu_action_sddirectory(const char* longFilename) {
      card.chdir(longFilename);
      encoderPosition = 0;
    }

  #endif // SDSUPPORT

  static void menu_action_setting_edit_bool(const char* pstr, bool* ptr) {UNUSED(pstr); *ptr = !(*ptr); }
  static void menu_action_setting_edit_callback_bool(const char* pstr, bool* ptr, screenFunc_t callback) {
    menu_action_setting_edit_bool(pstr, ptr);
    (*callback)();
  }

#endif // ULTIPANEL

/** LCD API **/
void lcd_init() {

  lcd_implementation_init();

  #if ENABLED(NEWPANEL)
    #if BUTTON_EXISTS(EN1)
      SET_INPUT(BTN_EN1);
      PULLUP(BTN_EN1, HIGH);
    #endif

    #if BUTTON_EXISTS(EN2)
      SET_INPUT(BTN_EN2);
      PULLUP(BTN_EN2, HIGH);
    #endif

    #if BUTTON_EXISTS(ENC)
      SET_INPUT(BTN_ENC);
      PULLUP(BTN_ENC, HIGH);
    #endif

    #if ENABLED(REPRAPWORLD_KEYPAD)
      pinMode(SHIFT_CLK, OUTPUT);
      pinMode(SHIFT_LD, OUTPUT);
      pinMode(SHIFT_OUT, INPUT);
      PULLUP(SHIFT_OUT, HIGH);
      WRITE(SHIFT_LD, HIGH);
    #endif

    #if BUTTON_EXISTS(UP)
      SET_INPUT(BTN_UP);
    #endif
    #if BUTTON_EXISTS(DWN)
      SET_INPUT(BTN_DWN);
    #endif
    #if BUTTON_EXISTS(LFT)
      SET_INPUT(BTN_LFT);
    #endif
    #if BUTTON_EXISTS(RT)
      SET_INPUT(BTN_RT);
    #endif

  #else  // Not NEWPANEL

    #if ENABLED(SR_LCD_2W_NL) // Non latching 2 wire shift register
      pinMode(SR_DATA_PIN, OUTPUT);
      pinMode(SR_CLK_PIN, OUTPUT);
    #elif ENABLED(SHIFT_CLK)
      pinMode(SHIFT_CLK, OUTPUT);
      pinMode(SHIFT_LD, OUTPUT);
      pinMode(SHIFT_EN, OUTPUT);
      pinMode(SHIFT_OUT, INPUT);
      PULLUP(SHIFT_OUT, HIGH);
      WRITE(SHIFT_LD, HIGH);
      WRITE(SHIFT_EN, LOW);
    #endif // SR_LCD_2W_NL

  #endif // !NEWPANEL

  #if ENABLED(SDSUPPORT) && PIN_EXISTS(SD_DETECT)
    SET_INPUT(SD_DETECT_PIN);
    PULLUP(SD_DETECT_PIN, HIGH);
    lcd_sd_status = 2; // UNKNOWN
  #endif

  #if ENABLED(LCD_HAS_SLOW_BUTTONS)
    slow_buttons = 0;
  #endif

  lcd_buttons_update();

  #if ENABLED(ULTIPANEL)
    encoderDiff = 0;
  #endif
}

int lcd_strlen(const char* s) {
  int i = 0, j = 0;
  while (s[i]) {
    if ((s[i] & 0xc0) != 0x80) j++;
    i++;
  }
  return j;
}

int lcd_strlen_P(const char* s) {
  int j = 0;
  while (pgm_read_byte(s)) {
    if ((pgm_read_byte(s) & 0xc0) != 0x80) j++;
    s++;
  }
  return j;
}

bool lcd_blink() {
  static uint8_t blink = 0;
  static millis_t next_blink_ms = 0;
  millis_t ms = millis();
  if (ELAPSED(ms, next_blink_ms)) {
    blink ^= 0xFF;
    next_blink_ms = ms + 1000 - LCD_UPDATE_INTERVAL / 2;
  }
  return blink != 0;
}

/**
 * Update the LCD, read encoder buttons, etc.
 *   - Read button states
 *   - Check the SD Card slot state
 *   - Act on RepRap World keypad input
 *   - Update the encoder position
 *   - Apply acceleration to the encoder position
 *   - Set lcdDrawUpdate = LCDVIEW_CALL_REDRAW_NOW on controller events
 *   - Reset the Info Screen timeout if there's any input
 *   - Update status indicators, if any
 *
 *   Run the current LCD menu handler callback function:
 *   - Call the handler only if lcdDrawUpdate != LCDVIEW_NONE
 *   - Before calling the handler, LCDVIEW_CALL_NO_REDRAW => LCDVIEW_NONE
 *   - Call the menu handler. Menu handlers should do the following:
 *     - If a value changes, set lcdDrawUpdate to LCDVIEW_REDRAW_NOW and draw the value
 *       (Encoder events automatically set lcdDrawUpdate for you.)
 *     - if (lcdDrawUpdate) { redraw }
 *     - Before exiting the handler set lcdDrawUpdate to:
 *       - LCDVIEW_CLEAR_CALL_REDRAW to clear screen and set LCDVIEW_CALL_REDRAW_NEXT.
 *       - LCDVIEW_REDRAW_NOW or LCDVIEW_NONE to keep drawingm but only in this loop.
 *       - LCDVIEW_REDRAW_NEXT to keep drawing and draw on the next loop also.
 *       - LCDVIEW_CALL_NO_REDRAW to keep drawing (or start drawing) with no redraw on the next loop.
 *     - NOTE: For graphical displays menu handlers may be called 2 or more times per loop,
 *             so don't change lcdDrawUpdate without considering this.
 *
 *   After the menu handler callback runs (or not):
 *   - Clear the LCD if lcdDrawUpdate == LCDVIEW_CLEAR_CALL_REDRAW
 *   - Update lcdDrawUpdate for the next loop (i.e., move one state down, usually)
 *
 * No worries. This function is only called from the main thread.
 */
void lcd_update() {

  #if ENABLED(ULTIPANEL)
    static millis_t return_to_status_ms = 0;
    manage_manual_move();
  #endif

  lcd_buttons_update();

  #if ENABLED(SDSUPPORT) && PIN_EXISTS(SD_DETECT)

    bool sd_status = IS_SD_INSERTED;
    if (sd_status != lcd_sd_status && lcd_detected()) {
      lcdDrawUpdate = LCDVIEW_CLEAR_CALL_REDRAW;
      lcd_implementation_init( // to maybe revive the LCD if static electricity killed it.
        #if ENABLED(LCD_PROGRESS_BAR) && ENABLED(ULTIPANEL)
          currentScreen == lcd_status_screen
        #endif
      );

      if (sd_status) {
        card.mount();
        if (lcd_sd_status != 2) LCD_MESSAGEPGM(MSG_SD_INSERTED);
      }
      else {
        card.unmount();
        if (lcd_sd_status != 2) LCD_MESSAGEPGM(MSG_SD_REMOVED);
      }

      lcd_sd_status = sd_status;
    }

  #endif // SDSUPPORT && SD_DETECT_PIN

  millis_t ms = millis();
  if (ELAPSED(ms, next_lcd_update_ms)) {

    next_lcd_update_ms = ms + LCD_UPDATE_INTERVAL;

    #if ENABLED(LCD_HAS_STATUS_INDICATORS)
      lcd_implementation_update_indicators();
    #endif

    #if ENABLED(LCD_HAS_SLOW_BUTTONS)
      slow_buttons = lcd_implementation_read_slow_buttons(); // buttons which take too long to read in interrupt context
    #endif

    #if ENABLED(ULTIPANEL)

      #if ENABLED(REPRAPWORLD_KEYPAD)

        #if MECH(DELTA) || MECH(SCARA)
          #define _KEYPAD_MOVE_ALLOWED (axis_homed[X_AXIS] && axis_homed[Y_AXIS] && axis_homed[Z_AXIS])
        #else
          #define _KEYPAD_MOVE_ALLOWED true
        #endif

        if (REPRAPWORLD_KEYPAD_MOVE_HOME)       reprapworld_keypad_move_home();
        if (_KEYPAD_MOVE_ALLOWED) {
          if (REPRAPWORLD_KEYPAD_MOVE_Z_UP)     reprapworld_keypad_move_z_up();
          if (REPRAPWORLD_KEYPAD_MOVE_Z_DOWN)   reprapworld_keypad_move_z_down();
          if (REPRAPWORLD_KEYPAD_MOVE_X_LEFT)   reprapworld_keypad_move_x_left();
          if (REPRAPWORLD_KEYPAD_MOVE_X_RIGHT)  reprapworld_keypad_move_x_right();
          if (REPRAPWORLD_KEYPAD_MOVE_Y_DOWN)   reprapworld_keypad_move_y_down();
          if (REPRAPWORLD_KEYPAD_MOVE_Y_UP)     reprapworld_keypad_move_y_up();
        }
      #endif

      bool encoderPastThreshold = (abs(encoderDiff) >= ENCODER_PULSES_PER_STEP);
      if (encoderPastThreshold || LCD_CLICKED) {
        if (encoderPastThreshold) {
          int32_t encoderMultiplier = 1;

          #if ENABLED(ENCODER_RATE_MULTIPLIER)

            if (encoderRateMultiplierEnabled) {
              int32_t encoderMovementSteps = abs(encoderDiff) / ENCODER_PULSES_PER_STEP;

              if (lastEncoderMovementMillis != 0) {
                // Note that the rate is always calculated between to passes through the
                // loop and that the abs of the encoderDiff value is tracked.
                float encoderStepRate = (float)(encoderMovementSteps) / ((float)(ms - lastEncoderMovementMillis)) * 1000.0;

                if (encoderStepRate >= ENCODER_100X_STEPS_PER_SEC)     encoderMultiplier = 100;
                else if (encoderStepRate >= ENCODER_10X_STEPS_PER_SEC) encoderMultiplier = 10;

                #if ENABLED(ENCODER_RATE_MULTIPLIER_DEBUG)
                  ECHO_SMV(DB, "Enc Step Rate: ", encoderStepRate);
                  ECHO_MV("  Multiplier: ", encoderMultiplier);
                  ECHO_MV("  ENCODER_10X_STEPS_PER_SEC: ", ENCODER_10X_STEPS_PER_SEC);
                  ECHO_EMV("  ENCODER_100X_STEPS_PER_SEC: ", ENCODER_100X_STEPS_PER_SEC);
                #endif
              }

              lastEncoderMovementMillis = ms;
            } // encoderRateMultiplierEnabled
          #endif // ENCODER_RATE_MULTIPLIER

          encoderPosition += (encoderDiff * encoderMultiplier) / ENCODER_PULSES_PER_STEP;
          encoderDiff = 0;
        }
        return_to_status_ms = ms + LCD_TIMEOUT_TO_STATUS;
        lcdDrawUpdate = LCDVIEW_REDRAW_NOW;
      }
    #endif // ULTIPANEL

    // We arrive here every ~100ms when idling often enough.
    // Instead of tracking the changes simply redraw the Info Screen ~1 time a second.
    static int8_t lcd_status_update_delay = 1; // first update one loop delayed
    if (
      #if ENABLED(ULTIPANEL)
        currentScreen == lcd_status_screen &&
      #endif
        !lcd_status_update_delay--) {
      lcd_status_update_delay = 9;
      lcdDrawUpdate = LCDVIEW_REDRAW_NOW;
    }

    if (lcdDrawUpdate) {

      switch (lcdDrawUpdate) {
        case LCDVIEW_CALL_NO_REDRAW:
          lcdDrawUpdate = LCDVIEW_NONE;
          break;
        case LCDVIEW_CLEAR_CALL_REDRAW: // set by handlers, then altered after (rarely occurs here)
        case LCDVIEW_CALL_REDRAW_NEXT:  // set by handlers, then altered after (never occurs here?)
          lcdDrawUpdate = LCDVIEW_REDRAW_NOW;
        case LCDVIEW_REDRAW_NOW:        // set above, or by a handler through LCDVIEW_CALL_REDRAW_NEXT
        case LCDVIEW_NONE:
          break;
      }

      #if ENABLED(DOGLCD)  // Changes due to different driver architecture of the DOGM display
        static int8_t dot_color = 0;
        dot_color = 1 - dot_color;
        u8g.firstPage();
        do {
          lcd_setFont(FONT_MENU);
          u8g.setPrintPos(125, 0);
          u8g.setColorIndex(dot_color); // Set color for the alive dot
          u8g.drawPixel(127, 63); // draw alive dot
          u8g.setColorIndex(1); // black on white
          (*currentScreen)();
        } while (u8g.nextPage());
      #elif ENABLED(ULTIPANEL)
        (*currentScreen)();
      #else
        lcd_status_screen();
      #endif
    }

    #if ENABLED(ULTIPANEL)

      // Return to Status Screen after a timeout
      if (currentScreen == lcd_status_screen || defer_return_to_status)
        return_to_status_ms = ms + LCD_TIMEOUT_TO_STATUS;
      else if (ELAPSED(ms, return_to_status_ms))
        lcd_return_to_status();

    #endif // ULTIPANEL

    switch (lcdDrawUpdate) {
      case LCDVIEW_CLEAR_CALL_REDRAW:
        lcd_implementation_clear();
      case LCDVIEW_CALL_REDRAW_NEXT:
        lcdDrawUpdate = LCDVIEW_REDRAW_NOW;
        break;
      case LCDVIEW_REDRAW_NOW:
        lcdDrawUpdate = LCDVIEW_NONE;
        break;
      case LCDVIEW_NONE:
        break;
    }
  }
}

void lcd_finishstatus(bool persist = false) {
  #if !(ENABLED(LCD_PROGRESS_BAR) && (PROGRESS_MSG_EXPIRE > 0))
    UNUSED(persist);
  #endif

  #if ENABLED(LCD_PROGRESS_BAR)
    progress_bar_ms = millis();
    #if PROGRESS_MSG_EXPIRE > 0
      expire_status_ms = persist ? 0 : progress_bar_ms + PROGRESS_MSG_EXPIRE;
    #endif
  #endif
  lcdDrawUpdate = LCDVIEW_CLEAR_CALL_REDRAW;

  #if HAS(LCD_FILAMENT_SENSOR) || HAS(LCD_POWER_SENSOR)
    previous_lcd_status_ms = millis();  //get status message to show up for a while
  #endif
}

#if ENABLED(LCD_PROGRESS_BAR) && PROGRESS_MSG_EXPIRE > 0
  void dontExpireStatus() { expire_status_ms = 0; }
#endif

void set_utf_strlen(char* s, uint8_t n) {
  uint8_t i = 0, j = 0;
  while (s[i] && (j < n)) {
    if ((s[i] & 0xc0u) != 0x80u) j++;
    i++;
  }
  while (j++ < n) s[i++] = ' ';
  s[i] = '\0';
}

bool lcd_hasstatus() { return (lcd_status_message[0] != '\0'); }

void lcd_setstatus(const char* message, bool persist) {
  if (lcd_status_message_level > 0) return;
  strncpy(lcd_status_message, message, 3 * (LCD_WIDTH));
  set_utf_strlen(lcd_status_message, LCD_WIDTH);
  lcd_finishstatus(persist);
}

void lcd_setstatuspgm(const char* message, uint8_t level) {
  if (level >= lcd_status_message_level) {
    strncpy_P(lcd_status_message, message, 3 * (LCD_WIDTH));
    set_utf_strlen(lcd_status_message, LCD_WIDTH);
    lcd_status_message_level = level;
    lcd_finishstatus(level > 0);
  }
}

void lcd_setalertstatuspgm(const char* message) {
  lcd_setstatuspgm(message, 1);
  #if ENABLED(ULTIPANEL)
    lcd_return_to_status();
  #endif
}

void lcd_reset_alert_level() { lcd_status_message_level = 0; }

#if HAS(LCD_CONTRAST)
  void set_lcd_contrast(int value) {
    lcd_contrast = constrain(value, LCD_CONTRAST_MIN, LCD_CONTRAST_MAX);
    u8g.setContrast(lcd_contrast);
  }
#endif

#if ENABLED(ULTIPANEL)

  /**
   * Setup Rotary Encoder Bit Values (for two pin encoders to indicate movement)
   * These values are independent of which pins are used for EN_A and EN_B indications
   * The rotary encoder part is also independent to the chipset used for the LCD
   */
  #if defined(EN_A) && defined(EN_B)
    #define encrot0 0
    #define encrot1 2
    #define encrot2 3
    #define encrot3 1
  #endif

  #define GET_BUTTON_STATES(DST) \
    uint8_t new_##DST = 0; \
    WRITE(SHIFT_LD, LOW); \
    WRITE(SHIFT_LD, HIGH); \
    for (int8_t i = 0; i < 8; i++) { \
      new_##DST >>= 1; \
      if (READ(SHIFT_OUT)) SBI(new_##DST, 7); \
      WRITE(SHIFT_CLK, HIGH); \
      WRITE(SHIFT_CLK, LOW); \
    } \
    DST = ~new_##DST; //invert it, because a pressed switch produces a logical 0


  /**
   * Read encoder buttons from the hardware registers
   * Warning: This function is called from interrupt context!
   */
  void lcd_buttons_update() {
    #if ENABLED(NEWPANEL)
      uint8_t newbutton = 0;
      #if BUTTON_EXISTS(EN1)
        if (BUTTON_PRESSED(EN1)) newbutton |= EN_A;
      #endif
      #if BUTTON_EXISTS(EN2)
        if (BUTTON_PRESSED(EN2)) newbutton |= EN_B;
      #endif
      #if LCD_HAS_DIRECTIONAL_BUTTONS || BUTTON_EXISTS(ENC)
        millis_t now = millis();
      #endif

      #if LCD_HAS_DIRECTIONAL_BUTTONS
        if (ELAPSED(now, next_button_update_ms)) {
          if (false) {
            // for the else-ifs below
          }
          #if BUTTON_EXISTS(UP)
            else if (BUTTON_PRESSED(UP)) {
              encoderDiff = -(ENCODER_STEPS_PER_MENU_ITEM);
              next_button_update_ms = now + 300;
            }
          #endif
          #if BUTTON_EXISTS(DWN)
            else if (BUTTON_PRESSED(DWN)) {
              encoderDiff = ENCODER_STEPS_PER_MENU_ITEM;
              next_button_update_ms = now + 300;
            }
          #endif
          #if BUTTON_EXISTS(LFT)
            else if (BUTTON_PRESSED(LFT)) {
              encoderDiff = -(ENCODER_PULSES_PER_STEP);
              next_button_update_ms = now + 300;
            }
          #endif
          #if BUTTON_EXISTS(RT)
            else if (BUTTON_PRESSED(RT)) {
              encoderDiff = ENCODER_PULSES_PER_STEP;
              next_button_update_ms = now + 300;
            }
          #endif
        }
      #endif

      #if BUTTON_EXISTS(ENC)
        if (ELAPSED(now, next_button_update_ms) && BUTTON_PRESSED(ENC)) newbutton |= EN_C;
      #endif

      buttons = newbutton;
      #if ENABLED(LCD_HAS_SLOW_BUTTONS)
        buttons |= slow_buttons;
      #endif
      #if ENABLED(REPRAPWORLD_KEYPAD)
        GET_BUTTON_STATES(buttons_reprapworld_keypad);
      #endif
    #else
      GET_BUTTON_STATES(buttons);
    #endif //!NEWPANEL

    #if ENABLED(INVERT_ROTARY_SWITCH)
      #define ENCODER_DIFF_CW  (encoderDiff += encoderDirection)
      #define ENCODER_DIFF_CCW (encoderDiff -= encoderDirection)
    #else
      #define ENCODER_DIFF_CW  (encoderDiff++)
      #define ENCODER_DIFF_CCW (encoderDiff--)
    #endif
    #define ENCODER_SPIN(_E1, _E2) switch (lastEncoderBits) { case _E1: ENCODER_DIFF_CW; break; case _E2: ENCODER_DIFF_CCW; }

    //manage encoder rotation
    uint8_t enc = 0;
    if (buttons & EN_A) enc |= B01;
    if (buttons & EN_B) enc |= B10;
    if (enc != lastEncoderBits) {
      switch (enc) {
        case encrot0: ENCODER_SPIN(encrot3, encrot1); break;
        case encrot1: ENCODER_SPIN(encrot0, encrot2); break;
        case encrot2: ENCODER_SPIN(encrot1, encrot3); break;
        case encrot3: ENCODER_SPIN(encrot2, encrot0); break;
      }
    }
    lastEncoderBits = enc;
  }

  bool lcd_detected(void) {
    #if (ENABLED(LCD_I2C_TYPE_MCP23017) || ENABLED(LCD_I2C_TYPE_MCP23008)) && ENABLED(DETECT_DEVICE)
      return lcd.LcdDetected() == 1;
    #else
      return true;
    #endif
  }

  bool lcd_clicked() { return LCD_CLICKED; }

#endif // ULTIPANEL

/*********************************/
/** Number to string conversion **/
/*********************************/

#define DIGIT(n) ('0' + (n))
#define DIGIMOD(n) DIGIT((n) % 10)

char conv[8];

// Convert float to rj string with 123 or -12 format
char *ftostr3(const float& x) { return itostr3((int)x); }

// Convert float to rj string with _123, -123, _-12, or __-1 format
char *ftostr4sign(const float& x) { return itostr4sign((int)x); }

// Convert unsigned int to string with 12 format
char* itostr2(const uint8_t& x) {
  //sprintf(conv,"%5.1f",x);
  int xx = x;
  conv[0] = DIGIMOD(xx / 10);
  conv[1] = DIGIMOD(xx);
  conv[2] = '\0';
  return conv;
}

// Convert float to string with +123.4 / -123.4 format
char* ftostr41sign(const float& x) {
  int xx = int(abs(x * 10)) % 10000;
  conv[0] = x >= 0 ? '+' : '-';
  conv[1] = DIGIMOD(xx / 1000);
  conv[2] = DIGIMOD(xx / 100);
  conv[3] = DIGIMOD(xx / 10);
  conv[4] = '.';
  conv[5] = DIGIMOD(xx);
  conv[6] = '\0';
  return conv;
}

// Convert signed float to string with 023.45 / -23.45 format
char *ftostr32(const float& x) {
  long xx = abs(x * 100);
  conv[0] = x >= 0 ? DIGIMOD(xx / 10000) : '-';
  conv[1] = DIGIMOD(xx / 1000);
  conv[2] = DIGIMOD(xx / 100);
  conv[3] = '.';
  conv[4] = DIGIMOD(xx / 10);
  conv[5] = DIGIMOD(xx);
  conv[6] = '\0';
  return conv;
}

// Convert signed float to string (6 digit) with -1.234 / _0.000 / +1.234 format
char* ftostr43sign(const float& x, char plus/*=' '*/) {
  long xx = x * 1000;
  if (xx == 0)
    conv[0] = ' ';
  else if (xx > 0)
    conv[0] = plus;
  else {
    xx = -xx;
    conv[0] = '-';
  }
  conv[1] = DIGIMOD(xx / 1000);
  conv[2] = '.';
  conv[3] = DIGIMOD(xx / 100);
  conv[4] = DIGIMOD(xx / 10);
  conv[5] = DIGIMOD(xx);
  conv[6] = '\0';
  return conv;
}

// Convert unsigned float to string with 1.23 format
char* ftostr12ns(const float& x) {
  long xx = x * 100;
  xx = abs(xx);
  conv[0] = DIGIMOD(xx / 100);
  conv[1] = '.';
  conv[2] = DIGIMOD(xx / 10);
  conv[3] = DIGIMOD(xx);
  conv[4] = '\0';
  return conv;
}

// Convert signed int to lj string with +012 / -012 format
char* itostr3sign(const int& x) {
  int xx;
  if (x >= 0) {
    conv[0] = '+';
    xx = x;
  }
  else {
    conv[0] = '-';
    xx = -x;
  }
  conv[1] = DIGIMOD(xx / 100);
  conv[2] = DIGIMOD(xx / 10);
  conv[3] = DIGIMOD(xx);
  conv[4] = '.';
  conv[5] = '0';
  conv[6] = '\0';
  return conv;
}

// Convert signed int to rj string with 123 or -12 format
char* itostr3(const int& x) {
  int xx = x;
  if (xx < 0) {
    conv[0] = '-';
    xx = -xx;
  }
  else
    conv[0] = xx >= 100 ? DIGIMOD(xx / 100) : ' ';

  conv[1] = xx >= 10 ? DIGIMOD(xx / 10) : ' ';
  conv[2] = DIGIMOD(xx);
  conv[3] = '\0';
  return conv;
}

// Convert unsigned int to lj string with 123 format
char* itostr3left(const int& xx) {
  if (xx >= 100) {
    conv[0] = DIGIMOD(xx / 100);
    conv[1] = DIGIMOD(xx / 10);
    conv[2] = DIGIMOD(xx);
    conv[3] = '\0';
  }
  else if (xx >= 10) {
    conv[0] = DIGIMOD(xx / 10);
    conv[1] = DIGIMOD(xx);
    conv[2] = '\0';
  }
  else {
    conv[0] = DIGIMOD(xx);
    conv[1] = '\0';
  }
  return conv;
}

// Convert signed int to rj string with _123, -123, _-12, or __-1 format
char *itostr4sign(const int& x) {
  int xx = abs(x);
  int sign = 0;
  if (xx >= 100) {
    conv[1] = DIGIMOD(xx / 100);
    conv[2] = DIGIMOD(xx / 10);
  }
  else if (xx >= 10) {
    conv[0] = ' ';
    sign = 1;
    conv[2] = DIGIMOD(xx / 10);
  }
  else {
    conv[0] = ' ';
    conv[1] = ' ';
    sign = 2;
  }
  conv[sign] = x < 0 ? '-' : ' ';
  conv[3] = DIGIMOD(xx);
  conv[4] = '\0';
  return conv;
}

// Convert unsigned float to rj string with 12345 format
char* ftostr5rj(const float& x) {
  long xx = abs(x);
  conv[0] = xx >= 10000 ? DIGIMOD(xx / 10000) : ' ';
  conv[1] = xx >= 1000 ? DIGIMOD(xx / 1000) : ' ';
  conv[2] = xx >= 100 ? DIGIMOD(xx / 100) : ' ';
  conv[3] = xx >= 10 ? DIGIMOD(xx / 10) : ' ';
  conv[4] = DIGIMOD(xx);
  conv[5] = '\0';
  return conv;
}

// Convert signed float to string with +1234.5 format
char* ftostr51sign(const float& x) {
  long xx = abs(x * 10);
  conv[0] = (x >= 0) ? '+' : '-';
  conv[1] = DIGIMOD(xx / 10000);
  conv[2] = DIGIMOD(xx / 1000);
  conv[3] = DIGIMOD(xx / 100);
  conv[4] = DIGIMOD(xx / 10);
  conv[5] = '.';
  conv[6] = DIGIMOD(xx);
  conv[7] = '\0';
  return conv;
}

// Convert signed float to string with +123.45 format
char* ftostr52sign(const float& x) {
  long xx = abs(x * 100);
  conv[0] = (x >= 0) ? '+' : '-';
  conv[1] = DIGIMOD(xx / 10000);
  conv[2] = DIGIMOD(xx / 1000);
  conv[3] = DIGIMOD(xx / 100);
  conv[4] = '.';
  conv[5] = DIGIMOD(xx / 10);
  conv[6] = DIGIMOD(xx);
  conv[7] = '\0';
  return conv;
}

// Convert signed float to space-padded string with -_23.4_ format
char* ftostr52sp(const float& x) {
  long xx = x * 100;
  uint8_t dig;
  if (xx < 0) { // negative val = -_0
    xx = -xx;
    conv[0] = '-';
    dig = (xx / 1000) % 10;
    conv[1] = dig ? DIGIT(dig) : ' ';
  }
  else { // positive val = __0
    dig = (xx / 10000) % 10;
    if (dig) {
      conv[0] = DIGIT(dig);
      conv[1] = DIGIMOD(xx / 1000);
    }
    else {
      conv[0] = ' ';
      dig = (xx / 1000) % 10;
      conv[1] = dig ? DIGIT(dig) : ' ';
    }
  }

  conv[2] = DIGIMOD(xx / 100); // lsd always

  dig = xx % 10;
  if (dig) { // 2 decimal places
    conv[5] = DIGIT(dig);
    conv[4] = DIGIMOD(xx / 10);
    conv[3] = '.';
  }
  else { // 1 or 0 decimal place
    dig = (xx / 10) % 10;
    if (dig) {
      conv[4] = DIGIT(dig);
      conv[3] = '.';
    }
    else {
      conv[3] = conv[4] = ' ';
    }
    conv[5] = ' ';
  }
  conv[6] = '\0';
  return conv;
}

char* ltostr7(const long& x) {
  if (x >= 1000000)
    conv[0]=(x/1000000)%10+'0';
  else
    conv[0]=' ';
  if (x >= 100000)
    conv[1]=(x/100000)%10+'0';
  else
    conv[1]=' ';
  if (x >= 10000)
    conv[2]=(x/10000)%10+'0';
  else
    conv[2]=' ';
  if (x >= 1000)
    conv[3]=(x/1000)%10+'0';
  else
    conv[3]=' ';
  if (x >= 100)
    conv[4]=(x/100)%10+'0';
  else
    conv[4]=' ';
  if (x >= 10)
    conv[5]=(x/10)%10+'0';
  else
    conv[5]=' ';
  conv[6]=(x)%10+'0';
  conv[7]=0;
  return conv;
}

#endif // ULTRA_LCD

#if ENABLED(SDSUPPORT) && ENABLED(SD_SETTINGS)
  void set_sd_dot() {
    #if ENABLED(DOGLCD)
      u8g.firstPage();
      do {
        u8g.setColorIndex(1);
        u8g.drawPixel(0, 0); // draw sd dot
        u8g.setColorIndex(1); // black on white
        (*currentScreen)();
      } while( u8g.nextPage() );
    #endif
  }
  void unset_sd_dot() {
    #if ENABLED(DOGLCD)
      u8g.firstPage();
      do {
        u8g.setColorIndex(0);
        u8g.drawPixel(0, 0); // draw sd dot
        u8g.setColorIndex(1); // black on white
        (*currentScreen)();
      } while( u8g.nextPage() );
    #endif
  }
#endif
