// Extra check B (second process): exit first again, same as the original repro, then rebuild.
REQUEST WEBUI_NEW_WINDOW
REQUEST WEBUI_EXIT
REQUEST WEBUI_CLEAN
PROCEDURE Main()
   LOCAL w
   ? "exit", WEBUI_EXIT()
   ? "clean", WEBUI_CLEAN()
   w := WEBUI_NEW_WINDOW()
   ? "new_window nonzero", w != 0
   ? "exit", WEBUI_EXIT()
   ? "clean", WEBUI_CLEAN()
RETURN
