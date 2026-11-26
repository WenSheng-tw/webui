// PoC-001: Harbour backend with a WebUI frontend.
// Run from the poc/harbour-webui folder so that ui/index.html resolves.

PROCEDURE Main()

   LOCAL nWindow
   LOCAL cHtml

   cHtml := MemoRead( "ui/index.html" )
   IF Empty( cHtml )
      ? "ERROR: ui/index.html not found. Run this program from poc/harbour-webui."
      RETURN
   ENDIF

   nWindow := WEBUI_NEWWINDOW()
   WEBUI_BINDHB( nWindow, "hello", "Hello" )
   WEBUI_SHOW( nWindow, cHtml )
   WEBUI_WAIT()

RETURN
