//
// Frame image for a design window. See frame_skin.h.

#include "app/frame_skin.h"

#include <FL/Fl.H>
#include <FL/Fl_Image.H>
#include <FL/Fl_Window.H>
#include <FL/fl_draw.H>
#include <FL/fl_utf8.h>
#include <FL/platform.H>
#include "../../src/Fl_Window_Driver.H"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#undef min
#undef max

namespace fluid {

namespace {

// Sizes of the drawn frame, in FLTK units.
const int kPlainTitle = 28;
const int kPlainEdge = 1;
const int kPlainButton = 30;
#ifdef __APPLE__
const bool kButtonsLeft = true;
#else
const bool kButtonsLeft = false;
#endif

// After this many failed captures of a system frame, this run stays with the drawn frame.
const int kGiveUp = 3;

enum class Source { NATIVE, FLTK, PLAIN };

struct Skin {
  Fl_RGB_Image *top = nullptr;     // drawn from the top-left corner of the frame
  Fl_RGB_Image *left = nullptr;
  Fl_RGB_Image *right = nullptr;
  Fl_RGB_Image *bottom = nullptr;
  int l = kPlainEdge;
  int t = kPlainTitle;
  int r = kPlainEdge;
  int b = kPlainEdge;
  int button_x = 0;                // distance from the left of the frame to the button area
  int button_w = 0;                // 0 means the button position is unknown
  bool system = false;             // image is from the system
};

// What a frame was captured for.
struct Taken {
  bool valid = false;
  std::string title;
  int w = 0;
  int h = 0;
  float scale = 0;
  unsigned dpi = 0;   // actual DPI of the hidden window at capture. It has not settled when this differs from the main window
  bool same(const Frame_Request &request, float at_scale, unsigned owner_dpi) const {
    if (!valid || w != request.w || h != request.h || scale != at_scale || title != request.title)
      return false;
    if (dpi && owner_dpi && dpi != owner_dpi) return false;
    return true;
  }
  void set(const Frame_Request &request, float at_scale, unsigned at_dpi) {
    valid = true;
    title = request.title;
    w = request.w;
    h = request.h;
    scale = at_scale;
    dpi = at_dpi;
  }
};

Skin g_skin;
Taken g_taken;      // the frame in use
Taken g_waiting;    // queued, but not captured yet
bool g_stale = false;
bool g_force = false;   // the system appearance changed, so the next request is captured again even when it is unchanged
bool g_dpi_wait = false;  // the hidden window has not reached the target screen; capture again later
bool g_second_shot = false;  // the screen just changed; capture once more, in case the system border has not updated yet
int g_dpi_tries = 0;
int g_failures = 0;
void (*g_changed)() = nullptr;

Source pick_source() {
  const char *force = fl_getenv("FLUID_PLUS_FRAME");
  if (force && !strcmp(force, "plain")) return Source::PLAIN;
  if (force && !strcmp(force, "fltk")) return Source::FLTK;
#ifdef _WIN32
  return Source::NATIVE;
#else
  return Source::FLTK;
#endif
}

Source &source() {
  static Source value = pick_source();
  return value;
}

void drop_images(Skin &skin) {
  delete skin.top;
  delete skin.left;
  delete skin.right;
  delete skin.bottom;
  skin.top = skin.left = skin.right = skin.bottom = nullptr;
}

// Install the newly captured frame. g_skin owns the images.
void adopt(Skin &fresh) {
  drop_images(g_skin);
  g_skin = fresh;
  fresh = Skin();
  g_failures = 0;
}

// This capture failed. Keep the previous frame. After several failures in a row, switch to the drawn frame.
void failed() {
  if (++g_failures < kGiveUp) return;
  source() = Source::PLAIN;
  drop_images(g_skin);
  g_skin = Skin();
  g_taken.valid = false;
}

#ifdef _WIN32

#ifndef WM_DWMCOMPOSITIONCHANGED
#  define WM_DWMCOMPOSITIONCHANGED 0x031E
#endif
#ifndef WM_DWMCOLORIZATIONCOLORCHANGED
#  define WM_DWMCOLORIZATIONCOLORCHANGED 0x0320
#endif

const DWORD kCaptionButtonBounds = 5;   // DWMWA_CAPTION_BUTTON_BOUNDS
const DWORD kExtendedFrameBounds = 9;   // DWMWA_EXTENDED_FRAME_BOUNDS
const DWORD kCloak = 13;                // DWMWA_CLOAK
const UINT kRenderFullContent = 2;      // PW_RENDERFULLCONTENT

// The same style FLTK uses for a resizable top-level window.
const DWORD kDonorStyle = WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_THICKFRAME | WS_SYSMENU |
                          WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_CAPTION;
const DWORD kDonorEx = WS_EX_WINDOWEDGE | WS_EX_CONTROLPARENT | WS_EX_NOACTIVATE;

typedef HRESULT(WINAPI *Dwm_Get)(HWND, DWORD, PVOID, DWORD);
typedef HRESULT(WINAPI *Dwm_Set)(HWND, DWORD, LPCVOID, DWORD);

FARPROC dwm_entry(const char *name) {
  static HMODULE lib = LoadLibraryW(L"dwmapi.dll");
  return lib ? GetProcAddress(lib, name) : nullptr;
}

HWND g_donor = nullptr;

UINT window_dpi(HWND hwnd) {
  typedef UINT(WINAPI *GetDpi)(HWND);
  static GetDpi get = (GetDpi)GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow");
  if (!get || !hwnd) return 0;
  return get(hwnd);
}

void destroy_donor() {
  if (!g_donor) return;
  DestroyWindow(g_donor);
  g_donor = nullptr;
}

LRESULT CALLBACK donor_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  switch (msg) {
    case WM_NCHITTEST:
      return HTTRANSPARENT;   // the mouse always passes through it
    case WM_MOUSEACTIVATE:
      return MA_NOACTIVATE;
    case WM_CLOSE:
      return 0;
    case WM_GETMINMAXINFO: {
      // The design window can be larger than the screen, so this window has to be able to grow with it.
      MINMAXINFO *info = (MINMAXINFO *)lp;
      info->ptMaxTrackSize.x = 32000;
      info->ptMaxTrackSize.y = 32000;
      return 0;
    }
    case WM_SETTINGCHANGE:
    case WM_THEMECHANGED:
    case WM_SYSCOLORCHANGE:
    case WM_DWMCOMPOSITIONCHANGED:
    case WM_DWMCOLORIZATIONCOLORCHANGED:
      g_stale = true;
      break;
  }
  return DefWindowProcW(hwnd, msg, wp, lp);
}

// A real system window, used only so the system paints the title bar. The system hides it. The user never sees it and cannot click it.
HWND donor_window(HWND owner, float scale) {
  // A hidden window does not follow the main window to another screen. Destroy and recreate it when the target scale does not match its DPI.
  UINT want = (UINT)lround(scale * 96.f);
  UINT have = g_donor ? window_dpi(g_donor) : 0;
  if (g_donor && want && have && ((int)have - (int)want > 1 || (int)want - (int)have > 1))
    destroy_donor();
  if (g_donor) return g_donor;
  static bool failed = false;
  if (failed) return nullptr;
  Dwm_Set set = (Dwm_Set)dwm_entry("DwmSetWindowAttribute");
  if (!set) {
    failed = true;
    return nullptr;
  }
  static const wchar_t *kClass = L"FluidPlusFrameDonor";
  WNDCLASSEXW wc;
  memset(&wc, 0, sizeof(wc));
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = donor_proc;
  wc.hInstance = GetModuleHandleW(nullptr);
  wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);   // default icon of an FLTK window
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
  wc.lpszClassName = kClass;
  RegisterClassExW(&wc);
  RECT at = {0, 0, 0, 0};
  GetWindowRect(owner, &at);
  HWND hwnd = CreateWindowExW(kDonorEx, kClass, L"", kDonorStyle, at.left, at.top, 320, 240, owner,
                              nullptr, wc.hInstance, nullptr);
  if (!hwnd) {
    failed = true;
    return nullptr;
  }
  BOOL on = TRUE;
  // If it cannot be hidden, discard it, so an extra window does not appear on screen.
  if (FAILED(set(hwnd, kCloak, &on, sizeof(on)))) {
    DestroyWindow(hwnd);
    failed = true;
    return nullptr;
  }
  ShowWindow(hwnd, SW_SHOWNOACTIVATE);
  g_donor = hwnd;
  return hwnd;
}

int to_units(int pixels, float scale) {
  if (pixels <= 0) return 0;
  return std::max(1, (int)lround(pixels / scale));
}

// Cut one piece out of a full BGRA image.
Fl_RGB_Image *cut(const unsigned char *bits, int stride, int x, int y, int w, int h) {
  if (w <= 0 || h <= 0) return nullptr;
  unsigned char *data = new unsigned char[(size_t)w * h * 4];
  bool has_alpha = false;
  unsigned char *to = data;
  for (int row = 0; row < h; row++) {
    const unsigned char *from = bits + ((size_t)(y + row) * stride + x) * 4;
    for (int col = 0; col < w; col++, from += 4, to += 4) {
      to[0] = from[2];
      to[1] = from[1];
      to[2] = from[0];
      to[3] = from[3];
      if (from[3]) has_alpha = true;
    }
  }
  // When the system wrote no transparency, treat the whole piece as opaque.
  if (!has_alpha) {
    for (size_t i = 3; i < (size_t)w * h * 4; i += 4) data[i] = 255;
  }
  Fl_RGB_Image *image = new Fl_RGB_Image(data, w, h, 4);
  image->alloc_array = 1;
  return image;
}

// A title bar should not be one solid color. A solid color means the system did not paint it.
bool looks_blank(const unsigned char *bits, int stride, int x, int y, int w, int h) {
  if (w <= 0 || h <= 0) return true;
  const unsigned char *first = bits + ((size_t)y * stride + x) * 4;
  for (int row = 0; row < h; row++) {
    const unsigned char *from = bits + ((size_t)(y + row) * stride + x) * 4;
    for (int col = 0; col < w; col++, from += 4) {
      if (from[0] != first[0] || from[1] != first[1] || from[2] != first[2]) return false;
    }
  }
  return true;
}

bool capture_native(const Frame_Request &request, Skin &out) {
  HWND owner = fl_xid(request.owner);
  if (!owner) return false;
  float scale = Fl::screen_scale(request.owner->screen_num());
  if (scale < 0.1f) scale = 1.f;
  HWND hwnd = donor_window(owner, scale);
  if (!hwnd) return false;
  // While dragging across a screen boundary, the main window's scale has already changed, and the hidden window may still be on the previous screen.
  // The border thickness of that capture is wrong. Do not keep it. Capture again once the window has settled.
  unsigned dpi = window_dpi(hwnd);
  unsigned expect = (unsigned)lround(scale * 96.f);
  if (dpi && expect && ((int)dpi - (int)expect > 1 || (int)expect - (int)dpi > 1)) {
    g_dpi_wait = true;
    return false;
  }
  // FLTK converts a window size to pixels the same way.
  int want_w = std::max(1, int(request.w * scale));
  int want_h = std::max(1, int(request.h * scale));

  std::vector<wchar_t> title(request.title.size() + 1);
  unsigned length = fl_utf8towc(request.title.c_str(), (unsigned)request.title.size(), title.data(),
                                (unsigned)title.size());
  title[std::min<size_t>(length, title.size() - 1)] = 0;
  SetWindowTextW(hwnd, title.data());

  // Place it on the main window's screen, so the system paints the title bar at that screen's scale.
  RECT owner_rect;
  if (!GetWindowRect(owner, &owner_rect)) return false;
  SetWindowPos(hwnd, nullptr, owner_rect.left, owner_rect.top, 0, 0,
               SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
  RECT whole;
  RECT client;
  for (int pass = 0; pass < 2; pass++) {
    if (!GetWindowRect(hwnd, &whole) || !GetClientRect(hwnd, &client)) return false;
    if (client.right == want_w && client.bottom == want_h) break;
    int extra_w = (whole.right - whole.left) - client.right;
    int extra_h = (whole.bottom - whole.top) - client.bottom;
    SetWindowPos(hwnd, nullptr, 0, 0, want_w + extra_w, want_h + extra_h,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
  }
  // Paint it with the active window's appearance, without taking focus.
  SendMessageW(hwnd, WM_NCACTIVATE, TRUE, 0);
  if (!GetWindowRect(hwnd, &whole) || !GetClientRect(hwnd, &client)) return false;
  POINT origin = {0, 0};
  if (!ClientToScreen(hwnd, &origin)) return false;
  // A system window rectangle has an invisible resize margin around it. The visible frame is queried separately.
  RECT frame = whole;
  Dwm_Get get = (Dwm_Get)dwm_entry("DwmGetWindowAttribute");
  if (get) {
    RECT seen;
    if (SUCCEEDED(get(hwnd, kExtendedFrameBounds, &seen, sizeof(seen)))) frame = seen;
  }
  int full_w = whole.right - whole.left;
  int full_h = whole.bottom - whole.top;
  if (full_w <= 0 || full_h <= 0) return false;

  // Have the system paint the whole window, title bar included, into memory.
  BITMAPINFO info;
  memset(&info, 0, sizeof(info));
  info.bmiHeader.biSize = sizeof(info.bmiHeader);
  info.bmiHeader.biWidth = full_w;
  info.bmiHeader.biHeight = -full_h;
  info.bmiHeader.biPlanes = 1;
  info.bmiHeader.biBitCount = 32;
  void *bits = nullptr;
  HDC screen = GetDC(nullptr);
  HDC memory = CreateCompatibleDC(screen);
  HBITMAP bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
  bool ok = false;
  if (memory && bitmap && bits) {
    HGDIOBJ old = SelectObject(memory, bitmap);
    ok = PrintWindow(hwnd, memory, kRenderFullContent) != 0;
    GdiFlush();
    if (ok) {
      const unsigned char *pixels = (const unsigned char *)bits;
      int frame_l = frame.left - whole.left;
      int frame_t = frame.top - whole.top;
      int frame_w = frame.right - frame.left;
      int client_l = origin.x - whole.left;
      int client_t = origin.y - whole.top;
      int left_px = client_l - frame_l;
      int top_px = client_t - frame_t;
      int right_px = (frame.right - whole.left) - (client_l + client.right);
      int bottom_px = (frame.bottom - whole.top) - (client_t + client.bottom);
      ok = frame_l >= 0 && frame_t >= 0 && frame_w > 0 && left_px >= 0 && top_px > 0 &&
           right_px >= 0 && bottom_px >= 0 && frame.right - whole.left <= full_w &&
           frame.bottom - whole.top <= full_h &&
           !looks_blank(pixels, full_w, frame_l, frame_t, frame_w, top_px);
      if (ok) {
        out.l = to_units(left_px, scale);
        out.t = to_units(top_px, scale);
        out.r = to_units(right_px, scale);
        out.b = to_units(bottom_px, scale);
        // When the system will not allow a window this narrow, the captured client area is larger than requested. It is squeezed back when drawn.
        int client_w = client.right == want_w ? request.w : to_units(client.right, scale);
        int client_h = client.bottom == want_h ? request.h : to_units(client.bottom, scale);
        int outer_w = client_w + out.l + out.r;
        out.top = cut(pixels, full_w, frame_l, frame_t, frame_w, top_px);
        out.left = cut(pixels, full_w, frame_l, client_t, left_px, client.bottom);
        out.right = cut(pixels, full_w, client_l + client.right, client_t, right_px, client.bottom);
        out.bottom = cut(pixels, full_w, frame_l, client_t + client.bottom, frame_w, bottom_px);
        if (out.top) out.top->scale(outer_w, out.t, 0, 1);
        if (out.left) out.left->scale(out.l, client_h, 0, 1);
        if (out.right) out.right->scale(out.r, client_h, 0, 1);
        if (out.bottom) out.bottom->scale(outer_w, out.b, 0, 1);
        RECT buttons;
        if (get && SUCCEEDED(get(hwnd, kCaptionButtonBounds, &buttons, sizeof(buttons))) &&
            buttons.right > buttons.left) {
          out.button_x = (int)lround((buttons.left - frame_l) / scale);
          out.button_w = to_units(buttons.right - buttons.left, scale);
        }
        out.system = true;
      }
    }
    SelectObject(memory, old);
  }
  if (bitmap) DeleteObject(bitmap);
  if (memory) DeleteDC(memory);
  ReleaseDC(nullptr, screen);
  return ok;
}

#else

bool capture_native(const Frame_Request &, Skin &) {
  return false;
}

#endif

// ---- FLTK's per-platform capture ----
//
// FLTK captures a real window on screen, so a real window is shown first, and the capture waits until the system has painted the border.

const double kSettle = 0.6;   // show the temporary window only after the title or size has stopped changing for this long
const double kPaint = 0.25;   // wait this long after the temporary window is shown, then capture

Fl_Window *g_fltk_donor = nullptr;
Fl_Widget_Tracker *g_focus = nullptr;   // widget that held focus before the temporary window was shown
Frame_Request g_pending;

void fltk_grab(void *) {
  Fl_Window *donor = g_fltk_donor;
  if (!donor || !donor->shown()) return;
  int w = donor->w();
  int h = donor->h();
  int whole_w = donor->decorated_w();
  int whole_h = donor->decorated_h();
  Skin fresh;
  Fl_Window_Driver::driver(donor)->capture_titlebar_and_borders(fresh.top, fresh.left, fresh.bottom,
                                                                fresh.right);
  donor->hide();
  // Showing the temporary window took the focus. Give it back to the main window and the previous input widget.
  Fl_Window *owner = g_pending.owner;
  if (owner && owner->shown()) owner->show();
  if (g_focus) {
    if (g_focus->exists()) Fl::focus(g_focus->widget());
    delete g_focus;
    g_focus = nullptr;
  }

  if (fresh.top && fresh.top->w() > 0 && fresh.top->h() > 0) {
    // FLTK reports only the total size with the border included. The left, right, and bottom edges are taken as equal width, which window managers mostly do.
    fresh.l = fresh.r = fresh.b = std::max(0, (whole_w - w) / 2);
    fresh.t = whole_h - h - fresh.b;
    if (fresh.t <= 0) fresh.t = fresh.top->h();
    fresh.system = true;
    adopt(fresh);
    g_taken = g_waiting;
  } else {
    drop_images(fresh);
    failed();
  }
  g_waiting.valid = false;
  if (g_changed) g_changed();
}

void fltk_show(void *) {
  Fl_Window *owner = g_pending.owner;
  if (!owner || !owner->shown() || !owner->visible()) return;
  // The whole window has to be on screen to be captured. A design window larger than the screen is captured smaller, and the middle is stretched when drawn.
  // Place it over the frame in the design area when possible, so the temporary window appears with no visible change.
  int sx = 0, sy = 0, sw = 800, sh = 600;
  Fl::screen_work_area(sx, sy, sw, sh, owner->screen_num());
  int w = std::max(40, std::min(g_pending.w, sw - 40));
  int h = std::max(40, std::min(g_pending.h, sh - 80));
  int x = std::max(sx + 20, std::min(g_pending.screen_x, sx + sw - w - 20));
  int y = std::max(sy + 60, std::min(g_pending.screen_y, sy + sh - h - 20));
  if (!g_fltk_donor) {
    Fl_Group *current = Fl_Group::current();
    Fl_Group::current(nullptr);
    g_fltk_donor = new Fl_Window(w, h);
    g_fltk_donor->end();
    g_fltk_donor->resizable(g_fltk_donor);
    Fl_Group::current(current);
  }
  g_fltk_donor->copy_label(g_pending.title.c_str());
  g_fltk_donor->resize(x, y, w, h);
  delete g_focus;
  g_focus = new Fl_Widget_Tracker(Fl::focus());
  g_fltk_donor->show();
  Fl::remove_timeout(fltk_grab);
  Fl::add_timeout(kPaint, fltk_grab);
}

void retry_frame(void *) {
  if (g_dpi_tries > 12) {
    g_dpi_wait = false;
    return;
  }
  g_stale = true;
  g_force = true;
  if (g_changed) g_changed();
}

void capture_later(const Frame_Request &request) {
  g_pending = request;
  Fl::remove_timeout(fltk_show);
  Fl::add_timeout(kSettle, fltk_show);
}

// ---- drawing ----

// Color of the leftmost pixel on the bottom row of the image. It fills the left, right, and bottom when the system gave no images for them.
Fl_Color edge_color(const Fl_RGB_Image *image) {
  if (!image || image->d() < 3 || !image->data() || !image->data()[0]) return FL_DARK3;
  int line = image->ld() ? image->ld() : image->data_w() * image->d();
  const unsigned char *pixel =
      (const unsigned char *)image->data()[0] + (size_t)(image->data_h() - 1) * line;
  return fl_rgb_color(pixel[0], pixel[1], pixel[2]);
}

void draw_top(const Skin &skin, int x, int y, int w) {
  Fl_RGB_Image *image = skin.top;
  int image_w = image->w();
  int image_h = image->h();
  if (image_w == w) {
    image->draw(x, y);
    return;
  }
  // The width does not match: the size is being dragged, or the system will not allow a window this narrow.
  // Draw the left and right parts unchanged, and stretch one column of pixels through the middle, so the buttons and the icon do not distort.
  int split = skin.button_w > 0 ? skin.button_x - 1 : image_w * 3 / 4;
  split = std::max(1, std::min(split, image_w - 1));
  int right_w = image_w - split;
  int right_x = x + w - right_w;
  int left_w = std::min(split, std::max(0, right_x - x));
  if (left_w > 0) image->draw(x, y, left_w, image_h, 0, 0);
  for (int gap = x + split; gap < right_x; gap++) image->draw(gap, y, 1, image_h, split - 1, 0);
  image->draw(right_x, y, right_w, image_h, split, 0);
}

void draw_system(const Skin &skin, int x, int y, int w, int h) {
  fl_push_clip(x, y, w, h);
  // On some systems top is an image of the whole window, with all four edges in it.
  bool whole = skin.top && skin.top->h() > skin.t + 1;
  if (skin.top) draw_top(skin, x, y, w);
  int side_h = h - skin.t - skin.b;
  if (!whole && side_h > 0) {
    Fl_Color fill = edge_color(skin.top);
    if (skin.left) {
      skin.left->scale(skin.l, side_h, 0, 1);
      skin.left->draw(x, y + skin.t);
    } else if (skin.l > 0) {
      fl_rectf(x, y + skin.t, skin.l, side_h, fill);
    }
    if (skin.right) {
      skin.right->scale(skin.r, side_h, 0, 1);
      skin.right->draw(x + w - skin.r, y + skin.t);
    } else if (skin.r > 0) {
      fl_rectf(x + w - skin.r, y + skin.t, skin.r, side_h, fill);
    }
    if (skin.bottom) {
      skin.bottom->scale(w, skin.b, 0, 1);
      skin.bottom->draw(x, y + h - skin.b);
    } else if (skin.b > 0) {
      fl_rectf(x, y + h - skin.b, w, skin.b, fill);
    }
  }
  fl_pop_clip();
}

// Used when a system frame cannot be captured. The buttons follow the side the system uses.
void draw_plain(int x, int y, int w, int h, const char *title) {
  Fl_Color edge = fl_color_average(FL_FOREGROUND_COLOR, FL_BACKGROUND_COLOR, 0.45f);
  Fl_Color ink = fl_color_average(FL_FOREGROUND_COLOR, FL_BACKGROUND_COLOR, 0.8f);
  fl_push_clip(x, y, w, h);
  fl_rectf(x, y, w, kPlainTitle, FL_BACKGROUND2_COLOR);
  fl_color(edge);
  fl_rect(x, y, w, h);
  fl_xyline(x, y + kPlainTitle - 1, x + w - 1);
  int middle = y + kPlainTitle / 2;
  int text_x = x + 10;
  int text_w = w - 20;
  Fl_Align align = FL_ALIGN_LEFT;
  if (kButtonsLeft) {
    static const unsigned char lights[3][3] = {{255, 95, 87}, {254, 188, 46}, {40, 200, 64}};
    for (int i = 0; i < 3; i++) {
      fl_color(fl_rgb_color(lights[i][0], lights[i][1], lights[i][2]));
      fl_pie(x + 9 + i * 20, middle - 6, 12, 12, 0, 360);
    }
    text_x = x + 72;
    text_w = w - 144;
    align = FL_ALIGN_CENTER;
  } else {
    int first = x + w - kPlainEdge - 3 * kPlainButton;
    int at = first + kPlainButton / 2;
    fl_color(ink);
    fl_xyline(at - 5, middle, at + 5);
    at += kPlainButton;
    fl_rect(at - 5, middle - 5, 11, 11);
    at += kPlainButton;
    fl_line(at - 5, middle - 5, at + 5, middle + 5);
    fl_line(at - 5, middle + 5, at + 5, middle - 5);
    text_w = first - text_x - 6;
  }
  if (title && *title && text_w > 0) {
    fl_font(FL_HELVETICA, 12);
    fl_color(FL_FOREGROUND_COLOR);
    fl_draw(title, text_x, y, text_w, kPlainTitle - 1, align | FL_ALIGN_CLIP, nullptr, 0);
  }
  fl_pop_clip();
}

}  // namespace

void frame_skin_request(const Frame_Request &request) {
  if (!request.owner) return;
  // While the main window is minimized the system does not paint windows. Capture again when it comes back.
  if (!request.owner->shown() || !request.owner->visible()) {
    g_stale = true;
    return;
  }
  // Skip the capture when the title, size, and scale are unchanged. A project-tree refresh reaches here every time.
  float scale = Fl::screen_scale(request.owner->screen_num());
  unsigned owner_dpi = 0;
#ifdef _WIN32
  if (request.owner->shown()) owner_dpi = window_dpi(fl_xid(request.owner));
#endif
  bool force = g_force;
  g_force = false;
  if (!force && g_taken.same(request, scale, owner_dpi)) return;
  switch (source()) {
    case Source::NATIVE: {
      Skin fresh;
      g_dpi_wait = false;
      if (capture_native(request, fresh)) {
        float previous = g_taken.scale;
        adopt(fresh);
        g_taken.set(request, scale, owner_dpi ? owner_dpi : g_taken.dpi);
#ifdef _WIN32
        if (g_donor) g_taken.dpi = window_dpi(g_donor);
#endif
        g_dpi_tries = 0;
        // When dragging from a high-resolution screen to a low-resolution one, the first capture is often still the previous screen's border.
        // Capture once more a moment later. Once the window has settled, the result matches switching windows or changing the size.
        bool moved = previous > 0.f && (previous < scale - 0.01f || previous > scale + 0.01f);
        if (moved && !g_second_shot) {
          g_second_shot = true;
          Fl::remove_timeout(retry_frame);
          Fl::add_timeout(0.12, retry_frame);
        } else {
          g_second_shot = false;
        }
      } else {
        drop_images(fresh);
        if (g_dpi_wait && g_dpi_tries < 12) {
          g_dpi_tries++;
          Fl::remove_timeout(retry_frame);
          Fl::add_timeout(0.05, retry_frame);
        } else {
          g_dpi_wait = false;
          failed();
        }
      }
      break;
    }
    case Source::FLTK:
      if (!force && g_waiting.same(request, scale, owner_dpi)) break;
      g_waiting.set(request, scale, owner_dpi);
      capture_later(request);
      break;
    case Source::PLAIN:
      break;
  }
}

void frame_skin_insets(int &left, int &top, int &right, int &bottom) {
  left = g_skin.l;
  top = g_skin.t;
  right = g_skin.r;
  bottom = g_skin.b;
}

void frame_skin_draw(int x, int y, int w, int h, const char *title) {
  if (w <= 0 || h <= 0) return;
  if (g_skin.system && g_skin.top) draw_system(g_skin, x, y, w, h);
  else draw_plain(x, y, w, h, title);
}

bool frame_skin_inert(int px, int w) {
  if (g_skin.system && g_skin.top) {
    if (g_skin.button_w <= 0) return false;
    // The buttons stay with the right side of the frame.
    int from = w - (g_skin.top->w() - g_skin.button_x);
    return px >= from && px < from + g_skin.button_w;
  }
  if (kButtonsLeft) return px < 72;
  return px >= w - kPlainEdge - 3 * kPlainButton;
}

bool frame_skin_live() {
  return source() == Source::NATIVE;
}

bool frame_skin_take_stale() {
  bool stale = g_stale;
  g_stale = false;
  if (stale) g_force = true;
  return stale;
}

void frame_skin_on_change(void (*changed)()) {
  g_changed = changed;
}

}  // namespace fluid
