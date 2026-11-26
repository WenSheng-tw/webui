/*
 * qa_neg.prg  --  QA negative-input tests for the W1 bridge (spec 2.1 rule 9).
 *
 * Every case calls the real WEBUI_* functions with hostile values (NaN, infinity, negative,
 * fractional, above 2^53, unknown enum, wrong type, NUL byte, huge string) and expects a
 * raised Harbour error (EG_ARG) and no crash. One case per process: run-qa-neg.sh starts
 * this exe once per case so that a crash is attributed to its case.
 *
 *   qa_neg --case=timeout|config|window|bind|show|state|table
 */

REQUEST WEBUI_NEW_WINDOW
REQUEST WEBUI_BIND
REQUEST WEBUI_SHOW
REQUEST WEBUI_WAIT
REQUEST WEBUI_WAIT_ASYNC
REQUEST WEBUI_CLOSE
REQUEST WEBUI_EXIT
REQUEST WEBUI_SET_TIMEOUT
REQUEST WEBUI_CLEAN
REQUEST WEBUI_SET_CONFIG
REQUEST WEBUI_GET_STRING
REQUEST WEBUI_RETURN_STRING
REQUEST WEBUI_PUMP
REQUEST WEBUI_BRIDGE_STAT
REQUEST WEBUI_BINDHB
REQUEST WEBUI_NEWWINDOW
REQUEST QA_NUM

PROCEDURE Main( ... )

   LOCAL cCase := ""
   LOCAL cArg

   FOR EACH cArg IN hb_AParams()
      IF Left( cArg, 7 ) == "--case="
         cCase := SubStr( cArg, 8 )
      ENDIF
   NEXT

   OutStd( "TAP version 13" + hb_eol() )
   T_Diag( "qa_neg case " + cCase + ", " + hb_Version() )

   DO CASE
   CASE cCase == "timeout"
      CaseTimeout()
   CASE cCase == "config"
      CaseConfig()
   CASE cCase == "window"
      CaseWindow()
   CASE cCase == "bind"
      CaseBind()
   CASE cCase == "show"
      CaseShow()
   CASE cCase == "state"
      CaseState()
   CASE cCase == "table"
      CaseTable()
   CASE cCase == "finding_exit_first"
      CaseExitFirst()
   OTHERWISE
      T_Ok( .F., "unknown case " + cCase )
   ENDCASE

   T_Ok( .T., "case " + cCase + " reached its end without a crash" )
   ErrorLevel( T_Done() )

RETURN

/* hostile numbers, name => value */
STATIC FUNCTION BadNums()

RETURN { ;
   "NaN" => QA_NUM( "nan" ), "-NaN" => QA_NUM( "-nan" ), "+inf" => QA_NUM( "inf" ), "-inf" => QA_NUM( "-inf" ), ;
   "2^53+2" => QA_NUM( "2^53+2" ), "2^63" => QA_NUM( "2^63" ), "2^64" => QA_NUM( "2^64" ), "-2^63" => QA_NUM( "-2^63" ), ;
   "DBL_MAX" => QA_NUM( "dmax" ), "denormal" => QA_NUM( "denorm" ), "-1" => -1, "-0.5" => -0.5, "0.5" => 0.5, ;
   "1.0000001" => 1.0000001, "-1e300" => -( 10 ^ 300 ), "1e300" => 10 ^ 300, "-1000000" => -1000000 }

STATIC FUNCTION BadTypes()

RETURN { "string" => "5", "NIL" => NIL, "logical" => .T., "array" => { 1 }, "hash" => { "a" => 1 }, ;
   "codeblock" => {|| 1 }, "date" => Date() }

STATIC PROCEDURE CaseTimeout()

   LOCAL h
   LOCAL k
   LOCAL n

   FOR EACH n IN { 61, 62, 100, 3600, 86400, 65535, 2 ^ 31, 2 ^ 32, 2 ^ 52 }
      T_Raises( {|| WEBUI_SET_TIMEOUT( n ) }, "set_timeout(" + hb_ntos( n ) + ") out of range 0..60" )
   NEXT
   h := BadNums()
   FOR EACH k IN hb_HKeys( h )
      T_Raises( {|| WEBUI_SET_TIMEOUT( h[ k ] ) }, "set_timeout(" + k + ") rejected" )
   NEXT
   h := BadTypes()
   FOR EACH k IN hb_HKeys( h )
      T_Raises( {|| WEBUI_SET_TIMEOUT( h[ k ] ) }, "set_timeout(" + k + " value) rejected" )
   NEXT
   T_Raises( {|| WEBUI_SET_TIMEOUT() }, "set_timeout() without argument rejected" )
   FOR EACH n IN { 0, 1, 59, 60 }
      T_Pass( {|| WEBUI_SET_TIMEOUT( n ) }, "set_timeout(" + hb_ntos( n ) + ") accepted" )
   NEXT
   T_Pass( {|| WEBUI_SET_TIMEOUT( 60.0 ) }, "set_timeout(60.0) accepted (integral double)" )

RETURN

STATIC PROCEDURE CaseConfig()

   LOCAL h
   LOCAL k
   LOCAL n

   FOR EACH n IN { 5, 6, 7, 8, 100, 255, 256, 65535, 65536, 2 ^ 32 }
      T_Raises( {|| WEBUI_SET_CONFIG( n, .T. ) }, "set_config(option " + hb_ntos( n ) + ") unknown or bridge-owned enum" )
   NEXT
   h := BadNums()
   FOR EACH k IN hb_HKeys( h )
      T_Raises( {|| WEBUI_SET_CONFIG( h[ k ], .T. ) }, "set_config(option " + k + ") rejected" )
   NEXT
   h := BadTypes()
   FOR EACH k IN hb_HKeys( h )
      T_Raises( {|| WEBUI_SET_CONFIG( h[ k ], .T. ) }, "set_config(option " + k + " value) rejected" )
      IF ! HB_ISLOGICAL( h[ k ] )
         T_Raises( {|| WEBUI_SET_CONFIG( 0, h[ k ] ) }, "set_config(0, status " + k + " value) rejected" )
      ENDIF
   NEXT
   T_Raises( {|| WEBUI_SET_CONFIG( 0, 1 ) }, "set_config(0, 1) numeric status rejected" )
   T_Raises( {|| WEBUI_SET_CONFIG( 0, 0 ) }, "set_config(0, 0) numeric status rejected" )
   T_Raises( {|| WEBUI_SET_CONFIG( 0, QA_NUM( "nan" ) ) }, "set_config(0, NaN) rejected" )
   T_Raises( {|| WEBUI_SET_CONFIG( 0 ) }, "set_config(0) without status rejected" )
   T_Raises( {|| WEBUI_SET_CONFIG() }, "set_config() rejected" )
   FOR EACH n IN { 0, 1, 2, 3, 4 }
      T_Pass( {|| WEBUI_SET_CONFIG( n, .T. ) }, "set_config(" + hb_ntos( n ) + ", .T.) accepted" )
      T_Pass( {|| WEBUI_SET_CONFIG( n, .F. ) }, "set_config(" + hb_ntos( n ) + ", .F.) accepted" )
   NEXT

RETURN

STATIC PROCEDURE CaseWindow()

   LOCAL h
   LOCAL k
   LOCAL n
   LOCAL xRes

   h := BadNums()
   h[ "0" ] := 0
   h[ "65536" ] := 65536
   h[ "1e9" ] := 10 ^ 9
   h[ "2^53" ] := QA_NUM( "2^53" )
   FOR EACH k IN hb_HKeys( h )
      T_Raises( {|| WEBUI_BIND( h[ k ], "x", "QA_H" ) }, "bind(window " + k + ") rejected" )
      T_Raises( {|| WEBUI_SHOW( h[ k ], "<html></html>" ) }, "show(window " + k + ") rejected" )
      T_Raises( {|| WEBUI_CLOSE( h[ k ] ) }, "close(window " + k + ") rejected" )
   NEXT
   h := BadTypes()
   FOR EACH k IN hb_HKeys( h )
      T_Raises( {|| WEBUI_BIND( h[ k ], "x", "QA_H" ) }, "bind(window " + k + " value) rejected" )
      T_Raises( {|| WEBUI_SHOW( h[ k ], "<html></html>" ) }, "show(window " + k + " value) rejected" )
      T_Raises( {|| WEBUI_CLOSE( h[ k ] ) }, "close(window " + k + " value) rejected" )
   NEXT
   /* inside the rule-9 id range 1..65535 but never created: WebUI must refuse without a crash */
   FOR EACH n IN { 1, 2, 255, 256, 257, 65534, 65535 }
      T_Raises( {|| WEBUI_BIND( n, "x", "QA_H" ) }, "bind(unused window " + hb_ntos( n ) + ") raises, no crash" )
      xRes := NIL
      T_Pass( {|| xRes := WEBUI_SHOW( n, "<html></html>" ) }, "show(unused window " + hb_ntos( n ) + ") does not raise" )
      T_Ok( xRes == .F., "show(unused window " + hb_ntos( n ) + ") returns .F." )
      T_Pass( {|| WEBUI_CLOSE( n ) }, "close(unused window " + hb_ntos( n ) + ") does nothing" )
   NEXT
   T_Ok( WEBUI_BINDHB( QA_NUM( "nan" ), "x", "QA_H" ) == 0 .AND. WEBUI_BINDHB( -1, "x", "QA_H" ) == 0 .AND. ;
      WEBUI_BINDHB( QA_NUM( "2^64" ), "x", "QA_H" ) == 0, "legacy WEBUI_BINDHB returns 0 for NaN, -1 and 2^64" )

RETURN

STATIC PROCEDURE CaseBind()

   LOCAL nWin := WEBUI_NEW_WINDOW()
   LOCAL h
   LOCAL k
   LOCAL xId
   LOCAL cBig

   T_Ok( nWin >= 1, "setup: window created" )
   h := BadTypes()
   FOR EACH k IN hb_HKeys( h )
      IF ! HB_ISSTRING( h[ k ] )
         T_Raises( {|| WEBUI_BIND( nWin, h[ k ], "QA_H" ) }, "bind(element " + k + " value) rejected" )
      ENDIF
      T_Raises( {|| WEBUI_BIND( nWin, "e", h[ k ] ) }, "bind(handler " + k + " value) rejected" )
   NEXT
   FOR EACH k IN { "", " ", "QA H", "QA-H", "QA.H", "1QA", "NO_SUCH_FN", "WEBUI_NO_SUCH", Replicate( "A", 64 ), Replicate( "A", 65 ), ;
         Replicate( "A", 100000 ), "QA_H" + Chr( 0 ), Chr( 0 ), "QA_H" + Chr( 255 ) }
      T_Raises( {|| WEBUI_BIND( nWin, "e", k ) }, "bind(handler name of " + hb_ntos( Len( k ) ) + " chars, hostile) rejected" )
   NEXT
   T_Raises( {|| WEBUI_BIND( nWin, "a" + Chr( 0 ) + "b", "QA_H" ) }, "bind(element with NUL) rejected" )
   T_Ok( WEBUI_BRIDGE_STAT( "binds" ) == 0, "rejected binds left the handler table empty" )

   cBig := Replicate( "e", 1000000 )
   BEGIN SEQUENCE WITH {| e | Break( e ) }
      xId := WEBUI_BIND( nWin, cBig, "QA_H" )
   RECOVER
      xId := 0
   END SEQUENCE
   T_Diag( "1 MB element name bind returned " + hb_ValToStr( xId ) )
   T_Ok( HB_ISNUMERIC( xId ), "bind(element of 1 MB) returns without a crash" )
   T_Ok( WEBUI_BIND( nWin, "ok", "qa_h" ) >= 1, "valid bind still works after the rejected ones" )
   T_Ok( WEBUI_CLEAN() == .T., "clean after the hostile binds" )
   T_Ok( WEBUI_BRIDGE_STAT( "live_bytes" ) == 0, "no bridge-owned memory left after clean" )

RETURN

STATIC PROCEDURE CaseShow()

   LOCAL nWin := WEBUI_NEW_WINDOW()
   LOCAL h
   LOCAL k

   h := BadTypes()
   FOR EACH k IN hb_HKeys( h )
      IF ! HB_ISSTRING( h[ k ] )
         T_Raises( {|| WEBUI_SHOW( nWin, h[ k ] ) }, "show(content " + k + " value) rejected" )
      ENDIF
   NEXT
   T_Raises( {|| WEBUI_SHOW( nWin, 12 ) }, "show(content number) rejected" )
   T_Raises( {|| WEBUI_SHOW( nWin, QA_NUM( "nan" ) ) }, "show(content NaN) rejected" )
   T_Raises( {|| WEBUI_SHOW( nWin, "<html>" + Chr( 0 ) + "</html>" ) }, "show(content with NUL) rejected" )
   T_Raises( {|| WEBUI_SHOW( nWin ) }, "show(window) without content rejected" )
   T_Raises( {|| WEBUI_SHOW() }, "show() rejected" )
   T_Raises( {|| WEBUI_CLOSE() }, "close() rejected" )
   T_Raises( {|| WEBUI_BIND() }, "bind() rejected" )
   T_Ok( WEBUI_CLEAN() == .T., "clean after the rejected shows" )

RETURN

STATIC PROCEDURE CaseState()

   LOCAL n
   LOCAL nWin

   T_Raises( {|| WEBUI_GET_STRING() }, "get_string outside a handler raises" )
   T_Raises( {|| WEBUI_RETURN_STRING( "x" ) }, "return_string outside a handler raises" )
   T_Raises( {|| WEBUI_RETURN_STRING( QA_NUM( "nan" ) ) }, "return_string(NaN) outside a handler raises" )
   T_Ok( WEBUI_WAIT_ASYNC() == .F., "wait_async before any window is .F." )
   T_Pass( {|| WEBUI_WAIT() }, "wait before any window returns" )
   FOR n := 1 TO 5
      nWin := WEBUI_NEW_WINDOW()
      T_Ok( nWin >= 1, "cycle " + hb_ntos( n ) + ": new_window after clean" )
      T_Raises( {|| WEBUI_SET_TIMEOUT( QA_NUM( "nan" ) ) }, "cycle " + hb_ntos( n ) + ": hostile set_timeout between lifecycle calls" )
      T_Raises( {|| WEBUI_BIND( 70000, "x", "QA_H" ) }, "cycle " + hb_ntos( n ) + ": hostile bind between lifecycle calls" )
      T_Ok( WEBUI_BIND( nWin, "x", "QA_H" ) >= 1, "cycle " + hb_ntos( n ) + ": valid bind" )
      T_Pass( {|| WEBUI_CLOSE( nWin ) }, "cycle " + hb_ntos( n ) + ": close of a never shown window" )
      T_Pass( {|| WEBUI_EXIT() }, "cycle " + hb_ntos( n ) + ": exit" )
      T_Ok( WEBUI_CLEAN() == .T., "cycle " + hb_ntos( n ) + ": clean" )
      T_Ok( WEBUI_BRIDGE_STAT( "live_bytes" ) == 0, "cycle " + hb_ntos( n ) + ": no bridge memory left" )
   NEXT
   T_Ok( WEBUI_CLEAN() == .F., "clean with nothing to clean is a no-op" )
   T_Pass( {|| WEBUI_EXIT() }, "exit without any window does not crash" )
   T_Pass( {|| WEBUI_EXIT() }, "exit twice does not crash" )
   T_Pass( {|| WEBUI_CLEAN() }, "clean after a lone exit does not crash" )

RETURN

/* Known finding F1 (not part of the pass/fail suite): a lone WEBUI_EXIT() is not counted as
 * WebUI use, so WEBUI_CLEAN() is a no-op afterwards and new_window returns 0 for the rest of
 * the process. */
STATIC PROCEDURE CaseExitFirst()

   T_Pass( {|| WEBUI_EXIT() }, "exit before any window" )
   T_Pass( {|| WEBUI_CLEAN() }, "clean after the lone exit" )
   T_Ok( WEBUI_NEW_WINDOW() >= 1, "new_window works after exit and clean (spec section 3: rebuild after clean)" )

RETURN

STATIC PROCEDURE CaseTable()

   LOCAL nWin := WEBUI_NEW_WINDOW()
   LOCAL i
   LOCAL nOk := 0
   LOCAL nId

   FOR i := 1 TO 600
      nId := WEBUI_BIND( nWin, "qa_el_" + hb_ntos( i ), "QA_H" )
      IF nId >= 1
         nOk++
      ENDIF
   NEXT
   T_Diag( "600 element binds, " + hb_ntos( nOk ) + " accepted, table holds " + hb_ntos( WEBUI_BRIDGE_STAT( "binds" ) ) )
   T_Ok( nOk >= 250, "many binds on one window: table grows and the first 250 are accepted" )
   T_Ok( WEBUI_BRIDGE_STAT( "binds" ) == nOk, "handler table count equals the accepted binds" )
   T_Ok( WEBUI_CLEAN() == .T., "clean frees the whole table" )
   T_Ok( WEBUI_BRIDGE_STAT( "binds" ) == 0 .AND. WEBUI_BRIDGE_STAT( "live_bytes" ) == 0, "no bridge memory left" )

RETURN

FUNCTION QA_H( cArg )

   HB_SYMBOL_UNUSED( cArg )

RETURN ""
