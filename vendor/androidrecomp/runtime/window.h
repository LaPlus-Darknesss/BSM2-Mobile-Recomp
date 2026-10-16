// The desktop stand-in for Android's GLSurfaceView.
//
// On Android, Java owns the window and the EGL context and calls into the
// engine; the engine imports no `egl*` symbol at all. That division is what
// makes this straightforward: the host creates a window and a GL context, and
// the engine only ever issues GL calls into whatever is current.
//
// Creating the context before loading the engine is not incidental ordering.
// The engine's 50 GL imports bind through `SDL_GL_GetProcAddress`, which needs
// a current context to answer, so the window has to exist first -- exactly as
// it does on Android.
#pragma once

#include <deque>
#include <string>

namespace arc {

class Window {
 public:
  ~Window();

  // `major`/`minor` request a particular GL version; 0 takes whatever the
  // driver offers, which has been GL 2.1 in practice. That default suits an
  // engine with a fixed-function GLESv1_CM tail, and is old for one written
  // against GLES2/GL3 -- so which to ask for is a property of the title, not
  // of the kit, and the caller decides.
  bool Open(const char* title, int width, int height, std::string* err,
            int major = 0, int minor = 0);

  // A touch, in the terms the engine's JNI bridge already speaks. A mouse is
  // a single finger that is only ever down or up, so `id` is always 0 and a
  // move with no button held is not reported at all -- Android has no hover.
  struct Pointer {
    enum Kind { Down, Move, Up } kind;
    int x, y;        // where it is now
    int from_x, from_y;  // where it was, which `pointerMoved` also wants
  };

  // Drains the event queue. Returns false once the user has asked to close,
  // which is the host's cue to call the engine's destroy path.
  bool PumpEvents();

  // One queued touch, oldest first. False when there are none left.
  bool NextPointer(Pointer* out);

  void Present();
  void Close();

  // Hand the context to the calling thread, and give it up again.
  //
  // A GL context belongs to one thread at a time, and the guest runs on a
  // thread of its own -- it needs a stack far larger than a default one. So
  // without this the engine's GL calls have no context to act on, and that
  // does not fail loudly: `glGetString` answers null and the engine calls
  // `strlen` on the result. Release on the thread that holds it before taking
  // it on another.
  bool MakeCurrent(std::string* err);
  void ReleaseCurrent();

  int width() const { return width_; }
  int height() const { return height_; }
  void* native_window() const { return window_; }
  void* native_context() const { return context_; }

  // True when the drawable was resized since the last call, so the host knows
  // to forward it to OGLESResize.
  bool TakeResized();

  // Human-readable description of the context we actually got.
  std::string Describe() const;

 private:
  void* window_ = nullptr;
  void* context_ = nullptr;
  int width_ = 0;
  int height_ = 0;
  bool resized_ = false;
  std::deque<Pointer> pointers_;
  int last_x_ = 0, last_y_ = 0;
  bool down_ = false;

  // A wheel notch, turned into a drag.
  //
  // The engine has no scroll event, because Android does not send it one: a
  // list longer than the screen -- a character's task list, a store category
  // -- is moved by dragging it, and down/move/up is the only vocabulary
  // available. So a notch is translated into that gesture, and it has to be
  // spread across frames: the host drains this queue completely every frame,
  // so a whole gesture pushed at once arrives as a press and a release at the
  // same point, which the engine reads as a tap on whatever was under the
  // cursor. Selecting a task is the opposite of scrolling past it.
  //
  // Pixels rather than steps, so notches arriving faster than the drag plays
  // out lengthen it instead of being dropped.
  int wheel_pending_ = 0;  // signed pixels still to drag
  int wheel_x_ = 0, wheel_y_ = 0;
  bool wheel_active_ = false;  // a synthetic finger is currently down
};

}  // namespace arc
