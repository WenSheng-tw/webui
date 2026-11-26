// Control (no exit first): new_window, show, wait, exit, clean. Tells whether show .F. comes from the order or the environment.
REQUEST WEBUI_NEW_WINDOW
REQUEST WEBUI_EXIT
REQUEST WEBUI_CLEAN
REQUEST WEBUI_SHOW
REQUEST WEBUI_WAIT
REQUEST WEBUI_SET_TIMEOUT
PROCEDURE Main()
   LOCAL w := WEBUI_NEW_WINDOW()
   ? "new_window nonzero", w != 0
   ? "show", WEBUI_SHOW( w, '<html><head><script src="webui.js"></script></head><body>ctl</body></html>' )
   WEBUI_SET_TIMEOUT( 1 )
   ? "wait", WEBUI_WAIT()
   ? "exit", WEBUI_EXIT()
   ? "clean", WEBUI_CLEAN()
RETURN
