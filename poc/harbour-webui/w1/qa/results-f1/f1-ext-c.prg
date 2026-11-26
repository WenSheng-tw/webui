// Extra check C: new_window after exit and clean, then exit and clean again, then once more.
REQUEST WEBUI_NEW_WINDOW
REQUEST WEBUI_EXIT
REQUEST WEBUI_CLEAN
PROCEDURE Main()
   LOCAL w, i
   w := WEBUI_NEW_WINDOW()
   ? "new_window nonzero", w != 0
   ? "exit", WEBUI_EXIT()
   ? "clean", WEBUI_CLEAN()
   FOR i := 1 TO 2
      w := WEBUI_NEW_WINDOW()
      ? "round", i, "new_window nonzero", w != 0
      ? "exit", WEBUI_EXIT()
      ? "clean", WEBUI_CLEAN()
   NEXT
   ? "extra exit", WEBUI_EXIT(), "extra clean", WEBUI_CLEAN()
RETURN
