// The original repro with the expected runtime error caught, so the later steps run too.
REQUEST WEBUI_NEW_WINDOW
REQUEST WEBUI_EXIT
REQUEST WEBUI_CLEAN
PROCEDURE Main()
   LOCAL oErr
   ? "exit", WEBUI_EXIT()
   BEGIN SEQUENCE WITH {| e | Break( e ) }
      ? "new_window (before clean)", WEBUI_NEW_WINDOW()
   RECOVER USING oErr
      ? "RAISED:", oErr:description, "|", oErr:operation, "| subcode", oErr:subCode
   END SEQUENCE
   ? "clean", WEBUI_CLEAN()
   ? "new_window (after clean) nonzero", WEBUI_NEW_WINDOW() != 0
   ? "exit", WEBUI_EXIT()
   ? "clean", WEBUI_CLEAN()
RETURN
