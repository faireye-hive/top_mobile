#include "top/GlxWindow.h"

#include <GL/glx.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>

#include <cstdio>
#include <cstring>

namespace top {
namespace {

using CreateContextAttribs = GLXContext (*)(Display*, GLXFBConfig, GLXContext, Bool, const int*);

int keyIndex(KeySym sym) {
    if (sym >= 'a' && sym <= 'z') return static_cast<int>(sym);
    if (sym >= 'A' && sym <= 'Z') return static_cast<int>(sym - 'A' + 'a');
    if (sym >= '0' && sym <= '9') return static_cast<int>(sym);
    if (sym >= XK_KP_0 && sym <= XK_KP_9) return static_cast<int>('0' + (sym - XK_KP_0));
    if (sym >= XK_F1 && sym <= XK_F9) return 128 + static_cast<int>(sym - XK_F1);
    switch (sym) {
        case XK_Escape: return 27;
        case XK_Return:
        case XK_KP_Enter: return 13;
        case XK_Left: return 1;
        case XK_Right: return 2;
        case XK_Up: return 3;
        case XK_Down: return 4;
        case XK_Shift_L:
        case XK_Shift_R: return 5;
        case XK_Alt_L:
        case XK_Alt_R: return 6;
        case XK_Page_Up:
        case XK_plus:
        case XK_equal:
        case XK_KP_Add: return 7;
        case XK_Page_Down:
        case XK_minus:
        case XK_KP_Subtract: return 8;
        case XK_Insert:
        case XK_KP_Insert: return 9;
        default: return 0;
    }
}

}  // namespace

bool GlxWindow::open(int width, int height, const char* title) {
    Display* display = XOpenDisplay(nullptr);
    if (!display) {
        std::fprintf(stderr, "OpenGL: sem display X11\n");
        return false;
    }
    int glxMajor = 0;
    int glxMinor = 0;
    if (!glXQueryVersion(display, &glxMajor, &glxMinor) || (glxMajor == 1 && glxMinor < 3)) {
        std::fprintf(stderr, "OpenGL: GLX antigo demais\n");
        XCloseDisplay(display);
        return false;
    }

    static const int visualAttribs[] = {
        GLX_X_RENDERABLE, True,
        GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
        GLX_RENDER_TYPE, GLX_RGBA_BIT,
        GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR,
        GLX_RED_SIZE, 8,
        GLX_GREEN_SIZE, 8,
        GLX_BLUE_SIZE, 8,
        GLX_ALPHA_SIZE, 8,
        GLX_DEPTH_SIZE, 24,
        GLX_DOUBLEBUFFER, True,
        None,
    };
    int configCount = 0;
    GLXFBConfig* configs = glXChooseFBConfig(display, DefaultScreen(display), visualAttribs, &configCount);
    if (!configs || configCount <= 0) {
        std::fprintf(stderr, "OpenGL: sem framebuffer\n");
        XCloseDisplay(display);
        return false;
    }
    GLXFBConfig config = configs[0];
    XVisualInfo* visual = glXGetVisualFromFBConfig(display, config);
    if (!visual) {
        XFree(configs);
        XCloseDisplay(display);
        return false;
    }

    XSetWindowAttributes swa;
    std::memset(&swa, 0, sizeof swa);
    swa.colormap = XCreateColormap(display, RootWindow(display, visual->screen), visual->visual, AllocNone);
    swa.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask |
                     PointerMotionMask | StructureNotifyMask;
    Window window = XCreateWindow(display, RootWindow(display, visual->screen), 0, 0, static_cast<unsigned>(width),
                                  static_cast<unsigned>(height), 0, visual->depth, InputOutput, visual->visual,
                                  CWColormap | CWEventMask, &swa);
    XStoreName(display, window, title);
    // Tell the window manager to SEND WM_DELETE_WINDOW rather than invoking
    // XKillClient (which closes the X11 connection and triggers fatal XIO).
    Atom wmDeleteWindow = XInternAtom(display, "WM_DELETE_WINDOW", False);
    if (wmDeleteWindow != None) XSetWMProtocols(display, window, &wmDeleteWindow, 1);
    XMapWindow(display, window);

    auto createAttribs = reinterpret_cast<CreateContextAttribs>(glXGetProcAddress(reinterpret_cast<const GLubyte*>("glXCreateContextAttribsARB")));
    GLXContext context = nullptr;
    if (createAttribs) {
        static const int ctxAttribs[] = {
            GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
            GLX_CONTEXT_MINOR_VERSION_ARB, 3,
            GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
            None,
        };
        context = createAttribs(display, config, nullptr, True, ctxAttribs);
    }
    if (!context) {
        std::fprintf(stderr, "OpenGL: contexto 3.3 nao abriu\n");
        XDestroyWindow(display, window);
        XFree(visual);
        XFree(configs);
        XCloseDisplay(display);
        return false;
    }
    glXMakeCurrent(display, window, context);
    display_ = display;
    wmDeleteWindow_ = static_cast<unsigned long>(wmDeleteWindow);
    window_ = static_cast<unsigned long>(window);
    context_ = context;
    width_ = width;
    height_ = height;
    XFree(visual);
    XFree(configs);
    return true;
}

void GlxWindow::hidePointer() {
    Display* display = static_cast<Display*>(display_);
    if (!display || !window_ || blankCursor_) return;
    char bits = 0;
    Pixmap pixmap = XCreateBitmapFromData(display, static_cast<Window>(window_), &bits, 1, 1);
    if (!pixmap) return;
    XColor color;
    std::memset(&color, 0, sizeof color);
    blankCursor_ = static_cast<unsigned long>(XCreatePixmapCursor(display, pixmap, pixmap, &color, &color, 0, 0));
    XFreePixmap(display, pixmap);
    if (blankCursor_) XDefineCursor(display, static_cast<Window>(window_), static_cast<Cursor>(blankCursor_));
}

void GlxWindow::close() {
    Display* display = static_cast<Display*>(display_);
    if (!display) return;
    if (blankCursor_) {
        XFreeCursor(display, static_cast<Cursor>(blankCursor_));
        blankCursor_ = 0;
    }
    glXMakeCurrent(display, None, nullptr);
    if (context_) glXDestroyContext(display, static_cast<GLXContext>(context_));
    if (window_) XDestroyWindow(display, static_cast<Window>(window_));
    XCloseDisplay(display);
    display_ = nullptr;
    window_ = 0;
    context_ = nullptr;
    wmDeleteWindow_ = 0;
    haveMouse_ = false;
}

std::string GlxWindow::swapControlInfo() const {
    Display* display = static_cast<Display*>(display_);
    if (!display || !window_) return "GLX: sem janela";
    const char* extensions = glXQueryExtensionsString(display, DefaultScreen(display));
    if (extensions && std::strstr(extensions, "GLX_EXT_swap_control")) {
        // GLX_SWAP_INTERVAL_EXT; the extension may be implemented by Mesa.
        unsigned interval = 0;
        glXQueryDrawable(display, static_cast<GLXDrawable>(window_), 0x20F1, &interval);
        return "GLX_EXT_swap_control: intervalo=" + std::to_string(interval);
    }
    if (extensions && std::strstr(extensions, "GLX_MESA_swap_control"))
        return "GLX_MESA_swap_control: intervalo nao consultado";
    return "GLX sem consulta ao intervalo (compositor/driver podem limitar FPS)";
}

bool GlxWindow::setSwapInterval(int interval) {
    Display* display = static_cast<Display*>(display_);
    if (!display || !window_ || (interval != 0 && interval != 1)) return false;
    const char* extensions = glXQueryExtensionsString(display, DefaultScreen(display));
    if (extensions && std::strstr(extensions, "GLX_EXT_swap_control")) {
        using SetExt = void (*)(Display*, GLXDrawable, int);
        auto fn = reinterpret_cast<SetExt>(glXGetProcAddressARB(
            reinterpret_cast<const GLubyte*>("glXSwapIntervalEXT")));
        if (fn) { fn(display, static_cast<GLXDrawable>(window_), interval); return true; }
    }
    if (extensions && std::strstr(extensions, "GLX_MESA_swap_control")) {
        using SetMesa = int (*)(unsigned);
        auto fn = reinterpret_cast<SetMesa>(glXGetProcAddressARB(
            reinterpret_cast<const GLubyte*>("glXSwapIntervalMESA")));
        if (fn && fn(static_cast<unsigned>(interval)) == 0) return true;
    }
    return false;
}

void GlxWindow::swap() {
    if (!display_ || !window_) return;
    glXSwapBuffers(static_cast<Display*>(display_), static_cast<Window>(window_));
}

void GlxWindow::setTitle(const char* title) {
    if (!display_ || !window_) return;
    XStoreName(static_cast<Display*>(display_), static_cast<Window>(window_), title);
}

void GlxWindow::pump(Input& input) {
    Display* display = static_cast<Display*>(display_);
    input.mouseDx = 0;
    input.mouseDy = 0;
    input.mousePressed = false;
    input.mouseReleased = false;
    input.wheel = 0;
    input.textInput.clear();
    input.backspacePressed = false;
    if (!display) return;
    while (XPending(display)) {
        XEvent event;
        XNextEvent(display, &event);
        if (event.type == ConfigureNotify) {
            width_ = event.xconfigure.width;
            height_ = event.xconfigure.height;
        } else if (event.type == KeyPress || event.type == KeyRelease) {
            const KeySym symbol = XLookupKeysym(&event.xkey, 0);
            const int index = keyIndex(symbol);
            if (index > 0 && index < 256) input.key[index] = event.type == KeyPress;
            if (event.type == KeyPress && symbol == XK_BackSpace) input.backspacePressed = true;
            if (event.type == KeyPress) {
                char text[16] = {};
                KeySym translated = 0;
                const int length = XLookupString(&event.xkey, text, sizeof(text), &translated, nullptr);
                for (int i = 0; i < length; ++i)
                    if (static_cast<unsigned char>(text[i]) >= 32 && static_cast<unsigned char>(text[i]) <= 126)
                        input.textInput.push_back(text[i]);
            }
        } else if (event.type == ButtonPress && event.xbutton.button == 1) {
            input.mouseDown = true;
            input.mousePressed = true;
            input.mouseX = event.xbutton.x;
            input.mouseY = event.xbutton.y;
        } else if (event.type == ButtonRelease && event.xbutton.button == 1) {
            input.mouseDown = false;
            input.mouseReleased = true;
            input.mouseX = event.xbutton.x;
            input.mouseY = event.xbutton.y;
        } else if (event.type == ButtonPress && event.xbutton.button == 3) {
            input.rightDown = true;
        } else if (event.type == ButtonRelease && event.xbutton.button == 3) {
            input.rightDown = false;
        } else if (event.type == ButtonPress && event.xbutton.button == 4) {
            input.wheel += 1;
        } else if (event.type == ButtonPress && event.xbutton.button == 5) {
            input.wheel -= 1;
        } else if (event.type == MotionNotify) {
            if (haveMouse_) {
                input.mouseDx += event.xmotion.x - lastMouseX_;
                input.mouseDy += event.xmotion.y - lastMouseY_;
            }
            lastMouseX_ = event.xmotion.x;
            lastMouseY_ = event.xmotion.y;
            input.mouseX = lastMouseX_;
            input.mouseY = lastMouseY_;
            haveMouse_ = true;
        } else if (event.type == ClientMessage &&
                   wmDeleteWindow_ && event.xclient.format == 32 &&
                   static_cast<unsigned long>(event.xclient.data.l[0]) == wmDeleteWindow_) {
            input.quit = true;
        } else if (event.type == DestroyNotify &&
                   static_cast<unsigned long>(event.xdestroywindow.window) == window_) {
            input.quit = true;
            window_ = 0; // prevent XDestroyWindow of a window already destroyed
        }
    }
}

}  // namespace top
