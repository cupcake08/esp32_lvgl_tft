#include <Arduino.h>
#include <lvgl.h>

#if LV_USE_TFT_ESPI
#include <TFT_eSPI.h>
#endif

#define TFT_HOR_RES 176
#define TFT_VER_RES 220
#define TFT_ROTATION LV_DISPLAY_ROTATION_90
#define AMP 80
#define LINE_W 5

#define CENTER_X (TFT_VER_RES / 2)
#define CENTER_Y (TFT_HOR_RES / 2)

#define PRECESION 0.01
#define TOTAL_POINTS (int)(4 * PI / PRECESION)
#define FRAME_SIZE (int)(TOTAL_POINTS * 0.2)

#define ANIM_DURATION 4000

#define DRAW_BUF_SIZE (TFT_HOR_RES * TFT_VER_RES / 10 * (LV_COLOR_DEPTH / 8))
uint32_t draw_buf[DRAW_BUF_SIZE / 4]; // lvgl buffer

static lv_point_precise_t path_buffer[TOTAL_POINTS];
static lv_point_precise_t frame_buffer[FRAME_SIZE];

static lv_point_precise_t outer_path_buffer[TOTAL_POINTS];
static lv_point_precise_t outer_frame_buffer[FRAME_SIZE];

static lv_point_precise_t third_path_buffer[TOTAL_POINTS];
static lv_point_precise_t third_frame_buffer[FRAME_SIZE];

#if LV_USE_LOG != 0
void my_print(lv_log_level_t level, const char *buf)
{
  LV_UNUSED(level);
  Serial.println(buf);
  Serial.flush();
}
#endif

static uint32_t my_tick(void)
{
  return millis();
}

static void frame_dx(int32_t s, lv_point_precise_t *path_buffer, lv_point_precise_t *frame_buffer)
{
  int idx = 0;
  for (int i = s; i < s + FRAME_SIZE; i++)
  {
    if (idx >= FRAME_SIZE)
      break;
    int pathIdx = i % TOTAL_POINTS;
    frame_buffer[idx].x = path_buffer[pathIdx].x;
    frame_buffer[idx].y = path_buffer[pathIdx].y;
    idx++;
  }
}

static void frame(void *obj, int32_t v)
{
  frame_dx(v, path_buffer, frame_buffer);
  lv_line_set_points((lv_obj_t *)obj, frame_buffer, FRAME_SIZE);
}

static void outer_frame(void *obj, int32_t v)
{
  frame_dx(v, outer_path_buffer, outer_frame_buffer);
  lv_line_set_points((lv_obj_t *)obj, outer_frame_buffer, FRAME_SIZE);
}

static void third_frame(void *obj, int32_t v)
{
  frame_dx(v, third_path_buffer, third_frame_buffer);
  lv_line_set_points((lv_obj_t *)obj, third_frame_buffer, FRAME_SIZE);
}

static void calc_paths(void)
{
  int idx = 0;
  float asymmetry = 0.8;

  for (double t = 0; t <= 4 * PI && idx < TOTAL_POINTS; t += PRECESION)
  {
    float x = AMP * sin(t) / (1 + cos(t) * cos(t));
    float y = AMP * sin(t) * cos(t) / (1 + cos(t) * cos(t));

    float xx = x;
    float yy = y;

    float xxx = x;
    float yyy = y;

    if (x < 0)
    {
      x *= asymmetry;
      y *= asymmetry;
    }
    path_buffer[idx].x = x + CENTER_X;
    path_buffer[idx].y = y + CENTER_Y;

    if (xx > 0)
    {
      xx *= asymmetry;
      yy *= asymmetry;
    }
    outer_path_buffer[idx].x = xx + (LINE_W << 1) + CENTER_X;
    outer_path_buffer[idx].y = yy + CENTER_Y;

    if (xxx > 0)
    {
      xxx *= (asymmetry - 0.2);
      yyy *= (asymmetry - 0.2);
    }
    else
    {
      xxx *= (2.0 - asymmetry);
      yyy *= (2.0 - asymmetry);
    }
    third_path_buffer[idx].x = xxx + (LINE_W << 2) + CENTER_X;
    third_path_buffer[idx].y = yyy + CENTER_Y;

    idx++;
  }
}

void setup()
{
  String LVGL_Arduino = "Hello ESP32! ";
  LVGL_Arduino += String('V') + lv_version_major() + "." + lv_version_minor() + "." + lv_version_patch();

  Serial.begin(115200);
  Serial.println(LVGL_Arduino);

  lv_init();

  /*Set a tick source so that LVGL will know how much time elapsed. */
  lv_tick_set_cb(my_tick);

  /* register print function for debugging */
#if LV_USE_LOG != 0
  lv_log_register_print_cb(my_print);
#endif

  lv_display_t *disp = lv_tft_espi_create(TFT_HOR_RES, TFT_VER_RES, draw_buf, sizeof(draw_buf));
  lv_display_set_rotation(disp, TFT_ROTATION);

  lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x0A0A15), 0);

  // create infinity symbol path
  calc_paths();

  // line style
  static lv_style_t style_line;
  lv_style_init(&style_line);
  lv_style_set_line_width(&style_line, LINE_W);
  lv_style_set_line_color(&style_line, lv_color_hex(0xFF6F61)); // coral
  lv_style_set_line_rounded(&style_line, true);
  lv_style_set_line_dash_gap(&style_line, 0);
  lv_style_set_line_opa(&style_line, LV_OPA_COVER); // Full opacity

  lv_obj_t *line = lv_line_create(lv_screen_active());
  lv_obj_add_style(line, &style_line, 0);

  lv_obj_t *line_outer = lv_line_create(lv_screen_active());
  lv_obj_add_style(line_outer, &style_line, 0);
  lv_obj_set_style_line_color(line_outer, lv_color_hex(0x32CD32), 0); // lime green

  lv_obj_t *line_third = lv_line_create(lv_screen_active());
  lv_obj_add_style(line_third, &style_line, 0);
  lv_obj_set_style_line_color(line_third, lv_color_hex(0x87CEEB), 0); // sky blue

  lv_anim_t a_outer;
  lv_anim_init(&a_outer);
  lv_anim_set_var(&a_outer, line_outer);
  lv_anim_set_values(&a_outer, 0, TOTAL_POINTS);
  lv_anim_set_duration(&a_outer, 6000);
  lv_anim_set_path_cb(&a_outer, lv_anim_path_linear);
  lv_anim_set_exec_cb(&a_outer, outer_frame);
  lv_anim_set_repeat_count(&a_outer, LV_ANIM_REPEAT_INFINITE);
  lv_anim_start(&a_outer);

  lv_anim_t a3;
  lv_anim_init(&a3);
  lv_anim_set_var(&a3, line_third);
  lv_anim_set_values(&a3, 0, TOTAL_POINTS);
  lv_anim_set_duration(&a3, 8000);
  lv_anim_set_path_cb(&a3, lv_anim_path_linear);
  lv_anim_set_exec_cb(&a3, third_frame);
  lv_anim_set_repeat_count(&a3, LV_ANIM_REPEAT_INFINITE);
  lv_anim_start(&a3);

  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, line);
  lv_anim_set_values(&a, 0, TOTAL_POINTS);
  lv_anim_set_duration(&a, ANIM_DURATION);
  lv_anim_set_path_cb(&a, lv_anim_path_linear);
  lv_anim_set_exec_cb(&a, frame);
  lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);

  lv_anim_start(&a);

  Serial.println("Animation started");
}

void loop()
{
  lv_timer_handler();
  delay(5);
}