REQUEST WEBUI_NEW_WINDOW
REQUEST WEBUI_EXIT
REQUEST WEBUI_CLEAN
PROCEDURE Main()
   ? "exit", WEBUI_EXIT()
   ? "clean", WEBUI_CLEAN()
   ? "new_window", WEBUI_NEW_WINDOW()
   ? "clean", WEBUI_CLEAN()
   ? "exit", WEBUI_EXIT()
   ? "clean", WEBUI_CLEAN()
   ? "new_window", WEBUI_NEW_WINDOW()
RETURN
