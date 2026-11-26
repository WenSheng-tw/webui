// Handler bound to the JavaScript function hello(name) in ui/index.html.

STATIC s_nCalls := 0

FUNCTION Hello( cName )

   s_nCalls++

RETURN "Harbour says: Hello, " + hb_ValToStr( cName ) + "! (call #" + LTrim( Str( s_nCalls ) ) + ")"
