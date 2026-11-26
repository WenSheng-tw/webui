/*
 * t_w1.prg  --  PoC-002 W1 tests: new_window, bind, show, wait, wait_async, close, exit,
 * clean, set_timeout, set_config, get_string, return_string, plus rule 3, 5, 7, 9.
 *
 * The first function of this file is the entry point (hb_hrbDo runs it in mode 2).
 * Handlers are public FUNCTIONs because bridge_bind resolves them by name.
 * Suites: "headless" (no browser), "gui" (needs a display and Chrome), "all".
 * TAP descriptions contain no ids, times or counts that differ between runs; those go
 * to "#" diagnostics.
 */

STATIC s_cDir
STATIC s_cPage
STATIC s_nWin
STATIC s_hBind
STATIC s_cEnd
STATIC s_hRep
STATIC s_hChk
STATIC s_aAny
STATIC s_aSeq
STATIC s_aPar
STATIC s_lReport
STATIC s_aLive
STATIC s_aShowMs
STATIC s_nLive

FUNCTION T_W1_RUN( cDir, cSuite )

   s_cDir := cDir

   IF cSuite == "all" .OR. cSuite == "headless"
      HeadlessTests()
   ENDIF
   IF cSuite == "all" .OR. cSuite == "gui"
      GuiTests()
   ENDIF
   Finish()

RETURN NIL

/* ------------------------------------------------------------------------------------ */
/* headless                                                                              */
/* ------------------------------------------------------------------------------------ */

STATIC FUNCTION Bad( cKind )
RETURN W1T_NUM( cKind )

STATIC PROCEDURE HeadlessTests()

   LOCAL n1
   LOCAL n2
   LOCAL nBind
   LOCAL nBefore
   LOCAL cLong
   LOCAL xV

   T_Diag( "headless suite" )

   /* A12 / rule 7: the TLS setter is not exposed in W1 */
   T_Ok( ! hb_IsFunction( "WEBUI_SET_TLS_CERTIFICATE" ), "rule 7: WEBUI_SET_TLS_CERTIFICATE is not exposed in W1" )
   T_Ok( hb_IsFunction( "WEBUI_NEW_WINDOW" ) .AND. hb_IsFunction( "WEBUI_CLEAN" ), "all W1 names resolve (spot check)" )

   /* rule 9 helpers */
   T_Ok( W1T_N2SIZE( 0 ) .AND. W1T_N2SIZE( 1 ) .AND. W1T_N2SIZE( 65535 ), "size_t conversion accepts 0, 1, 65535" )
   T_Ok( W1T_N2SIZE( Bad( "2^53" ) ), "size_t conversion accepts 2^53" )
   T_Ok( ! W1T_N2SIZE( Bad( "2^53+2" ) ), "size_t conversion rejects 2^53 + 2" )
   T_Ok( ! W1T_N2SIZE( Bad( "2^64" ) ), "size_t conversion rejects 2^64" )
   T_Ok( ! W1T_N2SIZE( -1 ), "size_t conversion rejects -1" )
   T_Ok( ! W1T_N2SIZE( 1.5 ), "size_t conversion rejects 1.5" )
   T_Ok( ! W1T_N2SIZE( Bad( "nan" ) ), "size_t conversion rejects NaN" )
   T_Ok( ! W1T_N2SIZE( Bad( "inf" ) ) .AND. ! W1T_N2SIZE( Bad( "-inf" ) ), "size_t conversion rejects +inf and -inf" )
   T_Ok( W1T_RANGE( "size", 100, 100 ) .AND. W1T_RANGE( "size", 3840, 2160 ), "rule 9 window size accepts 100x100 and 3840x2160" )
   T_Ok( ! W1T_RANGE( "size", 99, 100 ) .AND. ! W1T_RANGE( "size", 100, 99 ) .AND. ;
      ! W1T_RANGE( "size", 3841, 2160 ) .AND. ! W1T_RANGE( "size", 3840, 2161 ) .AND. ! W1T_RANGE( "size", 0, 0 ), ;
      "rule 9 window size rejects 99, 3841, 2161 and 0" )
   T_Ok( W1T_RANGE( "pos", 0, 0 ) .AND. W1T_RANGE( "pos", 3000, 1800 ), "rule 9 window position accepts 0,0 and 3000,1800" )
   T_Ok( ! W1T_RANGE( "pos", 3001, 0 ) .AND. ! W1T_RANGE( "pos", 0, 1801 ), "rule 9 window position rejects 3001 and 1801" )
   T_Ok( ! W1T_RANGE( "pos", -1, 0 ) .AND. ! W1T_RANGE( "pos", 0, Bad( "nan" ) ), "rule 9 window position rejects negative and NaN" )
   T_Ok( W1T_RANGE( "script", 1, 0 ) .AND. W1T_RANGE( "script", 86400, 0 ), "rule 9 script timeout accepts 1 and 86400" )
   T_Ok( ! W1T_RANGE( "script", 0, 0 ) .AND. ! W1T_RANGE( "script", 86401, 0 ), "rule 9 script timeout rejects 0 and 86401" )
   T_Ok( W1T_RANGE( "argidx", 0, 0 ) .AND. W1T_RANGE( "argidx", 16, 0 ) .AND. ! W1T_RANGE( "argidx", 17, 0 ), ;
      "rule 9 argument index accepts 0 and 16, rejects 17" )

   /* set_timeout */
   T_Pass( {|| WEBUI_SET_TIMEOUT( 0 ) }, "set_timeout accepts 0" )
   T_Pass( {|| WEBUI_SET_TIMEOUT( 60 ) }, "set_timeout accepts 60" )
   T_Pass( {|| WEBUI_SET_TIMEOUT( 30 ) }, "set_timeout accepts 30" )
   T_Raises( {|| WEBUI_SET_TIMEOUT( 61 ) }, "set_timeout rejects 61" )
   T_Raises( {|| WEBUI_SET_TIMEOUT( -1 ) }, "set_timeout rejects -1" )
   T_Raises( {|| WEBUI_SET_TIMEOUT( 1.5 ) }, "set_timeout rejects 1.5" )
   T_Raises( {|| WEBUI_SET_TIMEOUT( 10 ^ 300 ) }, "set_timeout rejects 10^300" )
   T_Raises( {|| WEBUI_SET_TIMEOUT( Bad( "2^53+2" ) ) }, "set_timeout rejects 2^53 + 2" )
   T_Raises( {|| WEBUI_SET_TIMEOUT( Bad( "nan" ) ) }, "set_timeout rejects NaN" )
   T_Raises( {|| WEBUI_SET_TIMEOUT( Bad( "inf" ) ) }, "set_timeout rejects infinity" )
   T_Raises( {|| WEBUI_SET_TIMEOUT( "5" ) }, "set_timeout rejects a string" )
   T_Raises( {|| WEBUI_SET_TIMEOUT( NIL ) }, "set_timeout rejects NIL" )
   T_Raises( {|| WEBUI_SET_TIMEOUT() }, "set_timeout rejects a missing argument" )

   /* set_config */
   T_Pass( {|| WEBUI_SET_CONFIG( 0, .T. ) }, "set_config accepts show_wait_connection" )
   T_Pass( {|| WEBUI_SET_CONFIG( 1, .F. ) }, "set_config accepts ui_event_blocking (rule 6)" )
   T_Pass( {|| WEBUI_SET_CONFIG( 2, .F. ) }, "set_config accepts folder_monitor" )
   T_Pass( {|| WEBUI_SET_CONFIG( 3, .F. ) }, "set_config accepts multi_client" )
   T_Pass( {|| WEBUI_SET_CONFIG( 4, .F. ) }, "set_config accepts use_cookies" )
   T_Raises( {|| WEBUI_SET_CONFIG( 5, .F. ) }, "set_config rejects asynchronous_response (bridge owned, rule 3)" )
   T_Raises( {|| WEBUI_SET_CONFIG( 6, .F. ) }, "set_config rejects option 6" )
   T_Raises( {|| WEBUI_SET_CONFIG( -1, .F. ) }, "set_config rejects option -1" )
   T_Raises( {|| WEBUI_SET_CONFIG( 1.5, .F. ) }, "set_config rejects option 1.5" )
   T_Raises( {|| WEBUI_SET_CONFIG( Bad( "nan" ), .F. ) }, "set_config rejects option NaN" )
   T_Raises( {|| WEBUI_SET_CONFIG( 0, 1 ) }, "set_config rejects a numeric status" )
   T_Raises( {|| WEBUI_SET_CONFIG( 0, "T" ) }, "set_config rejects a string status" )
   T_Raises( {|| WEBUI_SET_CONFIG( 0 ) }, "set_config rejects a missing status" )
   T_Raises( {|| WEBUI_SET_CONFIG() }, "set_config rejects missing arguments" )

   /* new_window */
   n1 := WEBUI_NEW_WINDOW()
   n2 := WEBUI_NEW_WINDOW()
   T_Ok( HB_ISNUMERIC( n1 ) .AND. n1 >= 1 .AND. n1 == Int( n1 ), "new_window returns a positive integer" )
   T_Ok( HB_ISNUMERIC( n2 ) .AND. n2 >= 1 .AND. n2 != n1, "second new_window returns a different id" )
   T_Ok( WEBUI_BRIDGE_STAT( "dirty" ) == 1, "new_window marks WebUI as used (clean has work to do)" )

   /* bind argument checks */
   T_Raises( {|| WEBUI_BIND( 0, "x", "W1H_READY" ) }, "bind rejects window 0" )
   T_Raises( {|| WEBUI_BIND( -1, "x", "W1H_READY" ) }, "bind rejects window -1" )
   T_Raises( {|| WEBUI_BIND( 65536, "x", "W1H_READY" ) }, "bind rejects window 65536" )
   T_Raises( {|| WEBUI_BIND( 1.5, "x", "W1H_READY" ) }, "bind rejects window 1.5" )
   T_Raises( {|| WEBUI_BIND( Bad( "nan" ), "x", "W1H_READY" ) }, "bind rejects window NaN" )
   T_Raises( {|| WEBUI_BIND( "1", "x", "W1H_READY" ) }, "bind rejects a string window" )
   T_Raises( {|| WEBUI_BIND( n1, 5, "W1H_READY" ) }, "bind rejects a numeric element" )
   T_Raises( {|| WEBUI_BIND( n1, "x", NIL ) }, "bind rejects a NIL handler" )
   T_Raises( {|| WEBUI_BIND( n1, "x", "" ) }, "bind rejects an empty handler name" )
   T_Raises( {|| WEBUI_BIND( n1, "x", "NO_SUCH_FUNCTION_W1" ) }, "bind rejects an unknown handler name" )
   cLong := Replicate( "A", 65 )
   T_Raises( {|| WEBUI_BIND( n1, "x", cLong ) }, "bind rejects a handler name of 65 characters" )
   T_Raises( {|| WEBUI_BIND( n1, "x" ) }, "bind rejects a missing handler" )
   T_Ok( WEBUI_BRIDGE_STAT( "binds" ) == 0, "failed binds leave the handler table empty" )

   nBefore := WEBUI_BRIDGE_STAT( "binds" )
   nBind := WEBUI_BIND( n1, "w1_ready", "W1H_READY" )
   T_Ok( HB_ISNUMERIC( nBind ) .AND. nBind >= 1 .AND. nBind == Int( nBind ), "bind returns a positive integer id" )
   T_Ok( WEBUI_BRIDGE_STAT( "binds" ) == nBefore + 1, "bind adds one handler table entry" )
   xV := WEBUI_BIND( n1, "w1_echo", "w1h_echo" )
   T_Ok( xV != nBind, "a second element gets another bind id; the handler name is case-insensitive" )
   T_Ok( WEBUI_BIND( n1, "", "W1H_ANY" ) >= 1, "bind accepts the empty element (all events)" )
   T_Ok( WEBUI_BIND( n2, "w1_ready", "W1H_READY" ) >= 1, "bind works on a second window" )
   T_Ok( WEBUI_BRIDGE_STAT( "binds" ) >= 4, "handler table holds every successful bind" )
   T_Ok( WEBUI_BINDHB( n1, "legacy", "W1H_READY" ) >= 1, "PoC-001 name WEBUI_BINDHB still binds" )
   T_Ok( WEBUI_BINDHB( n1, "legacy2", "NO_SUCH_FUNCTION_W1" ) == 0 .AND. WEBUI_BINDHB( 0, "x", "W1H_READY" ) == 0 .AND. ;
      WEBUI_BINDHB( "n", 1, 2 ) == 0, "WEBUI_BINDHB returns 0 instead of raising on bad input" )
   T_Ok( HB_ISNUMERIC( WEBUI_NEWWINDOW() ), "PoC-001 name WEBUI_NEWWINDOW still creates a window" )

   /* show / close argument checks (nothing valid is shown here: no browser starts) */
   T_Raises( {|| WEBUI_SHOW( 0, "<html></html>" ) }, "show rejects window 0" )
   T_Raises( {|| WEBUI_SHOW( -3, "<html></html>" ) }, "show rejects a negative window" )
   T_Raises( {|| WEBUI_SHOW( 70000, "<html></html>" ) }, "show rejects window 70000" )
   T_Raises( {|| WEBUI_SHOW( Bad( "nan" ), "<html></html>" ) }, "show rejects window NaN" )
   T_Raises( {|| WEBUI_SHOW( Bad( "2^64" ), "<html></html>" ) }, "show rejects window 2^64" )
   T_Raises( {|| WEBUI_SHOW( n1, NIL ) }, "show rejects NIL content" )
   T_Raises( {|| WEBUI_SHOW( n1, 42 ) }, "show rejects numeric content" )
   T_Raises( {|| WEBUI_SHOW( n1 ) }, "show rejects missing content" )
   T_Raises( {|| WEBUI_SHOW( n1, "a" + Chr( 0 ) + "b" ) }, "show rejects content with a NUL byte" )
   T_Raises( {|| WEBUI_CLOSE( 0 ) }, "close rejects window 0" )
   T_Raises( {|| WEBUI_CLOSE( 65536 ) }, "close rejects window 65536" )
   T_Raises( {|| WEBUI_CLOSE( Bad( "nan" ) ) }, "close rejects NaN" )
   T_Raises( {|| WEBUI_CLOSE( "1" ) }, "close rejects a string" )
   T_Raises( {|| WEBUI_CLOSE() }, "close rejects a missing argument" )
   T_Pass( {|| WEBUI_CLOSE( n2 ) }, "close of a window that was never shown does nothing" )

   /* get_string / return_string outside a handler */
   T_Raises( {|| WEBUI_GET_STRING() }, "get_string outside a handler raises" )
   T_Raises( {|| WEBUI_RETURN_STRING( "x" ) }, "return_string outside a handler raises" )
   T_Raises( {|| WEBUI_BRIDGE_STAT( "no_such_counter" ) }, "unknown bridge statistic raises" )

   /* wait before anything was shown (rule 8: nothing is running) */
   T_Ok( WEBUI_WAIT_ASYNC() == .F., "wait_async returns .F. before the first show" )
   T_Pass( {|| WEBUI_WAIT() }, "wait returns at once before the first show" )
   T_Ok( WEBUI_PUMP() == 0, "pump with an empty queue dispatches nothing" )

   /* exit and clean without a shown window, then rebuild */
   T_Pass( {|| WEBUI_EXIT() }, "exit without a shown window does not hang" )
   T_Ok( WEBUI_CLEAN() == .T., "clean after exit reports that it cleaned" )
   T_Ok( WEBUI_BRIDGE_STAT( "binds" ) == 0 .AND. WEBUI_BRIDGE_STAT( "dirty" ) == 0 .AND. ;
      WEBUI_BRIDGE_STAT( "live_bytes" ) == 0, "clean empties the handler table and frees every bridge allocation" )
   T_Ok( WEBUI_CLEAN() == .F., "a second clean is a no-op" )
   T_Ok( WEBUI_BRIDGE_STAT( "allocs" ) == WEBUI_BRIDGE_STAT( "frees" ), "bridge allocation and free counts are equal" )

   n1 := WEBUI_NEW_WINDOW()
   T_Ok( n1 >= 1, "new_window works after exit and clean (rebuild without show)" )
   T_Ok( WEBUI_BIND( n1, "w1_ready", "W1H_READY" ) >= 1, "bind works after clean" )
   T_Ok( WEBUI_CLEAN() == .T., "clean after rebuild cleans again" )
   T_Ok( WEBUI_BRIDGE_STAT( "live_bytes" ) == 0, "no bridge memory is left after the second clean" )

   ExitOrderTests()

RETURN

/* QA finding F1: WebUI refuses new windows after webui_exit() until webui_clean(). The
 * bridge records every WebUI use (also a lone exit), so clean always releases what it owns,
 * and new_window between exit and clean raises instead of returning 0. */
STATIC PROCEDURE ExitOrderTests()

   LOCAL n1
   LOCAL cMsg := ""
   LOCAL oErr

   T_Ok( WEBUI_BRIDGE_STAT( "dirty" ) == 0 .AND. WEBUI_BRIDGE_STAT( "exited" ) == 0, "F1 start state: nothing used since the last clean" )
   T_Ok( WEBUI_CLEAN() == .F., "F1 clean as the first call is a no-op" )

   /* order A: exit is the first call */
   WEBUI_EXIT()
   T_Ok( WEBUI_BRIDGE_STAT( "dirty" ) == 1 .AND. WEBUI_BRIDGE_STAT( "exited" ) == 1, "F1 a lone exit marks WebUI as used and exited" )
   T_Raises( {|| WEBUI_NEW_WINDOW() }, "F1 new_window after exit and before clean raises" )
   T_Raises( {|| WEBUI_NEWWINDOW() }, "F1 PoC-001 WEBUI_NEWWINDOW after exit and before clean raises" )
   BEGIN SEQUENCE WITH {| e | Break( e ) }
      WEBUI_NEW_WINDOW()
   RECOVER USING oErr
      IF HB_ISOBJECT( oErr )
         cMsg := oErr:description
      ENDIF
   END SEQUENCE
   T_Ok( "WEBUI_CLEAN" $ cMsg, "F1 the error text tells to call WEBUI_CLEAN: " + cMsg )
   T_Pass( {|| WEBUI_EXIT() }, "F1 a second exit is harmless" )
   T_Ok( WEBUI_CLEAN() == .T., "F1 clean after a lone exit releases WebUI and returns .T." )
   T_Ok( WEBUI_BRIDGE_STAT( "dirty" ) == 0 .AND. WEBUI_BRIDGE_STAT( "exited" ) == 0 .AND. ;
      WEBUI_BRIDGE_STAT( "live_bytes" ) == 0, "F1 clean after a lone exit resets the state and frees every bridge allocation" )
   n1 := WEBUI_NEW_WINDOW()
   T_Ok( HB_ISNUMERIC( n1 ) .AND. n1 >= 1, "F1 new_window works after exit then clean" )
   T_Ok( WEBUI_BIND( n1, "w1_ready", "W1H_READY" ) >= 1, "F1 bind works after exit then clean" )
   T_Ok( WEBUI_CLEAN() == .T. .AND. WEBUI_BRIDGE_STAT( "live_bytes" ) == 0, "F1 clean after the rebuild" )

   /* order B: window first, then exit */
   n1 := WEBUI_NEW_WINDOW()
   T_Ok( n1 >= 1, "F1 reverse order: new_window works" )
   WEBUI_EXIT()
   T_Ok( WEBUI_BRIDGE_STAT( "exited" ) == 1, "F1 reverse order: exit is recorded" )
   T_Raises( {|| WEBUI_NEW_WINDOW() }, "F1 reverse order: new_window after exit raises" )
   T_Ok( WEBUI_CLEAN() == .T., "F1 reverse order: clean returns .T." )
   n1 := WEBUI_NEW_WINDOW()
   T_Ok( n1 >= 1, "F1 reverse order: new_window works after clean" )
   T_Ok( WEBUI_CLEAN() == .T. .AND. WEBUI_BRIDGE_STAT( "live_bytes" ) == 0, "F1 reverse order: second clean frees everything" )

   /* repeated exit and clean without any window */
   T_Ok( ExitCleanLoop( 3 ), "F1 exit then clean three times in a row: every clean returns .T. and the allocations balance" )

   /* other first calls also mark WebUI as used */
   WEBUI_SET_TIMEOUT( 5 )
   T_Ok( WEBUI_BRIDGE_STAT( "dirty" ) == 1 .AND. WEBUI_CLEAN() == .T., "F1 set_timeout as the first call: clean releases WebUI" )
   WEBUI_WAIT_ASYNC()
   T_Ok( WEBUI_BRIDGE_STAT( "dirty" ) == 1 .AND. WEBUI_CLEAN() == .T., "F1 wait_async as the first call: clean releases WebUI" )
   WEBUI_CLOSE( 1 )
   T_Ok( WEBUI_BRIDGE_STAT( "dirty" ) == 1 .AND. WEBUI_CLEAN() == .T., "F1 close as the first call: clean releases WebUI" )
   T_Ok( WEBUI_BRIDGE_STAT( "live_bytes" ) == 0 .AND. WEBUI_BRIDGE_STAT( "allocs" ) == WEBUI_BRIDGE_STAT( "frees" ), "F1 no bridge memory is left" )

RETURN

STATIC FUNCTION ExitCleanLoop( nTimes )

   LOCAL i
   LOCAL lOk := .T.

   FOR i := 1 TO nTimes
      WEBUI_EXIT()
      lOk := lOk .AND. WEBUI_CLEAN() == .T.
   NEXT

RETURN lOk .AND. WEBUI_BRIDGE_STAT( "live_bytes" ) == 0 .AND. WEBUI_BRIDGE_STAT( "allocs" ) == WEBUI_BRIDGE_STAT( "frees" )

/* ------------------------------------------------------------------------------------ */
/* handlers (called by the bridge dispatcher on the main thread)                         */
/* ------------------------------------------------------------------------------------ */

STATIC PROCEDURE Rec( cKey, xVal )

   IF s_hChk != NIL
      s_hChk[ cKey ] := xVal
   ENDIF

RETURN

STATIC FUNCTION Raised( bBlock )

   LOCAL lRaised := .F.

   BEGIN SEQUENCE WITH {| e | Break( e ) }
      Eval( bBlock )
   RECOVER
      lRaised := .T.
   END SEQUENCE

RETURN lRaised

FUNCTION W1H_READY( cArg, nWin, nBind, nEvt, nType )

   Rec( "ready_win", nWin == s_nWin )
   Rec( "ready_bind", nBind == s_hBind[ "w1_ready" ] )
   Rec( "ready_evt", HB_ISNUMERIC( nEvt ) .AND. nEvt > 0 )
   Rec( "ready_type", nType )
   Rec( "ready_same", WEBUI_GET_STRING() == cArg )

RETURN "R0"

FUNCTION W1H_ECHO( cArg )

   LOCAL cGet := WEBUI_GET_STRING()

   IF s_hChk[ "echo_n" ] == 0
      Rec( "echo_same", cGet == cArg )
      Rec( "echo_accepted", WEBUI_RETURN_STRING( "E:" + cArg ) )
   ELSE
      Rec( "hostile_same", cGet == cArg )
      Rec( "hostile_arg", cArg )
      WEBUI_RETURN_STRING( "E:" + cArg )
   ENDIF
   s_hChk[ "echo_n" ]++

RETURN "IGNORED"

FUNCTION W1H_AUTO( cArg )
RETURN "A:" + cArg

FUNCTION W1H_NIL( cArg )

   HB_SYMBOL_UNUSED( cArg )

RETURN NIL

FUNCTION W1H_TWICE()

   LOCAL l1 := WEBUI_RETURN_STRING( "first" )
   LOCAL l2 := WEBUI_RETURN_STRING( "second" )

   Rec( "twice", { l1, l2 } )

RETURN "third"

FUNCTION W1H_EMPTY( cArg )

   Rec( "empty_param", cArg == "" )
   Rec( "empty_get", WEBUI_GET_STRING() == "" )

RETURN "E0"

FUNCTION W1H_LONG( cArg )

   Rec( "long_len", Len( cArg ) )

RETURN "L:" + hb_ntos( Len( cArg ) )

FUNCTION W1H_ARGCHK()

   Rec( "argchk_rs_num", Raised( {|| WEBUI_RETURN_STRING( 123 ) } ) )
   Rec( "argchk_rs_none", Raised( {|| WEBUI_RETURN_STRING() } ) )
   Rec( "argchk_rs_nul", Raised( {|| WEBUI_RETURN_STRING( "a" + Chr( 0 ) + "b" ) } ) )
   Rec( "argchk_wait", Raised( {|| WEBUI_WAIT() } ) )
   Rec( "argchk_pump", WEBUI_PUMP() )
   Rec( "argchk_waitasync_nested", WEBUI_WAIT_ASYNC() )
   Rec( "argchk_queued", WEBUI_BRIDGE_STAT( "queued" ) )

RETURN "AC"

FUNCTION W1H_SEQ( cArg )

   AAdd( s_aSeq, Val( cArg ) )

RETURN "S" + cArg

FUNCTION W1H_PAR( cArg )

   AAdd( s_aPar, Val( cArg ) )

RETURN "P" + cArg

FUNCTION W1H_ANY( cArg, nWin, nBind, nEvt, nType )

   HB_SYMBOL_UNUSED( cArg )
   HB_SYMBOL_UNUSED( nWin )
   HB_SYMBOL_UNUSED( nBind )
   HB_SYMBOL_UNUSED( nEvt )
   AAdd( s_aAny, nType )

RETURN ""

FUNCTION W1H_REPORT( cArg )

   LOCAL xRes

   hb_jsonDecode( cArg, @xRes )
   s_hRep := xRes
   s_lReport := .T.

RETURN "ok"

FUNCTION W1H_DONE()

   DO CASE
   CASE s_cEnd == "exit"
      WEBUI_EXIT()
   CASE s_cEnd == "clean"
      Rec( "clean_in_handler", WEBUI_CLEAN() )
      Rec( "clean_pending", WEBUI_BRIDGE_STAT( "clean_pending" ) )
      Rec( "binds_in_handler", WEBUI_BRIDGE_STAT( "binds" ) )
   OTHERWISE
      WEBUI_CLOSE( s_nWin )
   ENDCASE

RETURN "bye"

/* ------------------------------------------------------------------------------------ */
/* GUI cycles                                                                            */
/* ------------------------------------------------------------------------------------ */

STATIC PROCEDURE GuiTests()

   T_Diag( "gui suite" )
   s_cPage := hb_MemoRead( s_cDir + hb_ps() + "w1_page.html" )
   IF ! T_Ok( Len( s_cPage ) > 100, "test page tests/w1_page.html is readable" )
      RETURN
   ENDIF
   s_aLive := Array( 5, 3 )
   s_aShowMs := Array( 5, 2 )
   s_nLive := 0

   Cycle( "async", .T., .F. )
   Cycle( "wait", .F., .F. )
   Cycle( "exit", .F., .F. )
   Cycle( "clean", .F., .F. )
   Cycle( "async", .F., .T. )

RETURN

STATIC PROCEDURE Cycle( cEnd, lGeneric, lNoWait )

   LOCAL cTag := "[" + cEnd + iif( lGeneric, "+generic", "" ) + iif( lNoWait, "+nowait", "" ) + "] "
   LOCAL aNames := { "w1_ready", "w1_echo", "w1_auto", "w1_nil", "w1_twice", "w1_empty", "w1_long", ;
      "w1_argchk", "w1_seq", "w1_par", "w1_report", "w1_done" }
   LOCAL aFuncs := { "W1H_READY", "W1H_ECHO", "W1H_AUTO", "W1H_NIL", "W1H_TWICE", "W1H_EMPTY", "W1H_LONG", ;
      "W1H_ARGCHK", "W1H_SEQ", "W1H_PAR", "W1H_REPORT", "W1H_DONE" }
   LOCAL i
   LOCAL nBind
   LOCAL lBound := .T.
   LOCAL lShown
   LOCAL nT0
   LOCAL nShowMs
   LOCAL nGuard
   LOCAL nBinds := Len( aNames )

   s_cEnd := cEnd
   s_hChk := { "echo_n" => 0 }
   s_hBind := { => }
   s_hRep := NIL
   s_lReport := .F.
   s_aAny := {}
   s_aSeq := {}
   s_aPar := {}

   IF lNoWait
      WEBUI_SET_CONFIG( 0, .F. )
   ENDIF
   s_nWin := WEBUI_NEW_WINDOW()
   T_Ok( s_nWin >= 1, cTag + "new_window" )
   FOR i := 1 TO Len( aNames )
      nBind := WEBUI_BIND( s_nWin, aNames[ i ], aFuncs[ i ] )
      s_hBind[ aNames[ i ] ] := nBind
      lBound := lBound .AND. nBind >= 1
   NEXT
   IF lGeneric
      WEBUI_BIND( s_nWin, "", "W1H_ANY" )
      nBinds++
   ENDIF
   T_Ok( lBound, cTag + "bind: every handler bound" )
   T_Ok( WEBUI_BRIDGE_STAT( "binds" ) == nBinds, cTag + "bind: handler table holds exactly the binds of this window" )

   nT0 := hb_MilliSeconds()
   lShown := WEBUI_SHOW( s_nWin, s_cPage )
   nShowMs := hb_MilliSeconds() - nT0
   T_Diag( cTag + "show returned " + hb_ValToStr( lShown ) + " after " + hb_ntos( nShowMs ) + " ms, window " + hb_ntos( s_nWin ) )
   T_Ok( lShown == .T., cTag + "show returns .T. (browser started)" )
   s_aShowMs[ s_nLive + 1 ][ 1 ] := lNoWait
   s_aShowMs[ s_nLive + 1 ][ 2 ] := nShowMs

   IF lShown
      nT0 := hb_MilliSeconds()
      DO CASE
      CASE cEnd == "async"
         nGuard := hb_MilliSeconds() + Val( hb_GetEnv( "W1_GUARD", "40" ) ) * 1000
         DO WHILE WEBUI_WAIT_ASYNC()
            IF hb_MilliSeconds() > nGuard
               T_Diag( cTag + "guard expired, exiting" )
               WEBUI_EXIT()
               EXIT
            ENDIF
            hb_idleSleep( 0.005 )
         ENDDO
         T_Ok( WEBUI_WAIT_ASYNC() == .F., cTag + "wait_async returns .F. after the window closed" )
      OTHERWISE
         WEBUI_WAIT()
         T_Ok( .T., cTag + "wait returned" )
      ENDCASE
      T_Diag( cTag + "run took " + hb_ntos( hb_MilliSeconds() - nT0 ) + " ms" )
   ELSE
      WEBUI_EXIT()
   ENDIF

   T_Diag( cTag + "bridge: dispatched " + hb_ntos( WEBUI_BRIDGE_STAT( "dispatched" ) ) + ", auto answers " + ;
      hb_ntos( WEBUI_BRIDGE_STAT( "auto_responses" ) ) + ", missing handlers " + hb_ntos( WEBUI_BRIDGE_STAT( "missing_handlers" ) ) )
   CheckReport( cTag, cEnd )

   IF lGeneric
      T_Ok( AScan( s_aAny, 1 ) > 0, cTag + "generic bind saw the CONNECTED event" )
      T_Diag( cTag + "generic bind event types: " + hb_jsonEncode( s_aAny ) )
   ENDIF

   IF cEnd == "clean"
      T_Ok( s_hChk[ "clean_in_handler" ] == .F., cTag + "rule 5: clean inside a handler only exits (returns .F.)" )
      T_Ok( s_hChk[ "clean_pending" ] == 1, cTag + "rule 5: clean inside a handler records that a clean is owed" )
      T_Ok( s_hChk[ "binds_in_handler" ] == nBinds, cTag + "rule 5: handler table is intact inside the handler" )
      T_Ok( WEBUI_BRIDGE_STAT( "binds" ) == nBinds, cTag + "rule 5: handler table is still intact after wait returned" )
      T_Ok( WEBUI_BRIDGE_STAT( "clean_pending" ) == 1, cTag + "rule 5: the owed clean is still pending after wait" )
   ENDIF

   T_Ok( WEBUI_BRIDGE_STAT( "queued" ) == 0, cTag + "event queue is empty after the run" )
   T_Ok( WEBUI_CLEAN() == .T., cTag + "clean (non-callback context, after wait) cleans" )
   T_Ok( WEBUI_BRIDGE_STAT( "binds" ) == 0 .AND. WEBUI_BRIDGE_STAT( "clean_pending" ) == 0 .AND. ;
      WEBUI_BRIDGE_STAT( "dirty" ) == 0, cTag + "clean: handler table freed, nothing pending" )
   T_Ok( WEBUI_BRIDGE_STAT( "live_bytes" ) == 0 .AND. WEBUI_BRIDGE_STAT( "allocs" ) == WEBUI_BRIDGE_STAT( "frees" ), ;
      cTag + "clean: no bridge-owned memory is left" )
   T_Ok( WEBUI_CLEAN() == .F., cTag + "second clean is a no-op" )
   hb_gcAll()
   s_nLive++
   s_aLive[ s_nLive ][ 2 ] := W1T_COUNTER( "blocks" )
   s_aLive[ s_nLive ][ 3 ] := W1T_COUNTER( "bytes" )
   s_aLive[ s_nLive ][ 1 ] := s_nLive

RETURN

STATIC PROCEDURE CheckReport( cTag, cEnd )

   LOCAL lGot := s_hRep != NIL .AND. HB_ISHASH( s_hRep )
   LOCAL i
   LOCAL lSeq := .T.
   LOCAL lPar := .T.
   LOCAL aP

   HB_SYMBOL_UNUSED( cEnd )

   T_Ok( s_lReport .AND. lGot, cTag + "page ran to the end and reported" )
   IF ! lGot
      RETURN
   ENDIF
   T_Ok( ! hb_HHasKey( s_hRep, "error" ), cTag + "page reported no JavaScript error" )
   IF hb_HHasKey( s_hRep, "error" )
      T_Diag( cTag + "page error: " + s_hRep[ "error" ] )
   ENDIF

   T_Ok( hb_HGetDef( s_hRep, "ready", NIL ) == "R0", cTag + "bound call returns the handler's RETURN string" )
   T_Ok( s_hChk[ "ready_win" ] .AND. s_hChk[ "ready_bind" ] .AND. s_hChk[ "ready_evt" ], ;
      cTag + "handler parameters: window id, bind id, event number" )
   T_Ok( s_hChk[ "ready_type" ] == 4, cTag + "handler parameter: event type is CALLBACK" )
   T_Ok( s_hChk[ "ready_same" ] .AND. s_hChk[ "echo_same" ], cTag + "get_string equals the handler's first parameter" )
   T_Ok( hb_HGetDef( s_hRep, "echo", NIL ) == "E:héllo 世界", ;
      cTag + "return_string delivers a UTF-8 answer unchanged" )
   T_Ok( s_hChk[ "echo_accepted" ] == .T., cTag + "return_string reports that the answer was accepted" )
   T_Ok( hb_HGetDef( s_hRep, "auto", NIL ) == "A:x", cTag + "handler RETURN string is the answer when return_string is not called" )
   T_Ok( hb_HGetDef( s_hRep, "nil", NIL ) == "", cTag + "handler returning NIL answers with an empty string" )
   T_Ok( hb_HGetDef( s_hRep, "twice", NIL ) == "first" .AND. s_hChk[ "twice" ][ 1 ] .AND. ! s_hChk[ "twice" ][ 2 ], ;
      cTag + "return_string twice: the first answer wins, the second reports .F., the RETURN value is ignored" )
   T_Ok( hb_HGetDef( s_hRep, "empty", NIL ) == "E0" .AND. s_hChk[ "empty_param" ] .AND. s_hChk[ "empty_get" ], ;
      cTag + "call without argument: parameter and get_string are empty" )
   T_Ok( hb_HGetDef( s_hRep, "long", NIL ) == "L:20000" .AND. s_hChk[ "long_len" ] == 20000, ;
      cTag + "a 20000 character argument arrives complete" )
   T_Ok( hb_HGetDef( s_hRep, "hostile", NIL ) == "E:" + s_hChk[ "hostile_arg" ] .AND. ;
      s_hChk[ "hostile_arg" ] == 'q"' + "'" + "\" + Chr( 10 ) + Chr( 9 ) + "<&>" .AND. s_hChk[ "hostile_same" ], ;
      cTag + "quotes, backslash, newline, tab and markup characters survive both directions" )
   T_Ok( s_hChk[ "argchk_rs_num" ] .AND. s_hChk[ "argchk_rs_none" ] .AND. s_hChk[ "argchk_rs_nul" ], ;
      cTag + "return_string rejects a number, a missing argument and a NUL byte inside a handler" )
   T_Ok( s_hChk[ "argchk_wait" ], cTag + "wait inside a handler raises instead of deadlocking" )
   T_Ok( s_hChk[ "argchk_pump" ] == 0, cTag + "pump inside a handler dispatches nothing (no re-entry)" )
   T_Ok( s_hChk[ "argchk_waitasync_nested" ] == .T., cTag + "wait_async inside a handler does not re-enter or close the app" )
   T_Ok( hb_HGetDef( s_hRep, "argchk", NIL ) == "AC", cTag + "handler that raised and recovered still answers" )

   FOR i := 1 TO 20
      lSeq := lSeq .AND. hb_HGetDef( s_hRep, "seq", {} ) != NIL .AND. Len( s_hRep[ "seq" ] ) == 20 .AND. ;
         s_hRep[ "seq" ][ i ] == "S" + hb_ntos( i - 1 )
   NEXT
   T_Ok( lSeq .AND. Len( s_aSeq ) == 20, cTag + "20 sequential calls all answered with their own result" )
   aP := hb_HGetDef( s_hRep, "par", {} )
   FOR i := 1 TO 20
      lPar := lPar .AND. Len( aP ) == 20 .AND. aP[ i ] == "P" + hb_ntos( i - 1 )
   NEXT
   T_Ok( lPar .AND. Len( s_aPar ) == 20, cTag + "20 concurrent calls: every caller gets its own answer (rule 3 queue)" )
   T_Ok( ASort( AClone( s_aPar ) )[ 1 ] == 0 .AND. ATail( ASort( AClone( s_aPar ) ) ) == 19, ;
      cTag + "20 concurrent calls: every call reached the handler exactly once" )
   T_Ok( WEBUI_BRIDGE_STAT( "missing_handlers" ) == 0, cTag + "no call reached an unknown handler" )

RETURN

STATIC PROCEDURE Finish()

   LOCAL a
   LOCAL nMin
   LOCAL nMax

   IF s_nLive != NIL .AND. s_nLive > 0
      FOR EACH a IN s_aLive
         IF a[ 1 ] != NIL
            T_Diag( "cycle " + hb_ntos( a[ 1 ] ) + ": live blocks after clean " + hb_ntos( a[ 2 ] ) + ", bytes " + hb_ntos( a[ 3 ] ) )
         ENDIF
      NEXT
      IF W1T_COUNTER( "enabled" ) == 1 .AND. s_nLive >= 3
         T_Ok( s_aLive[ s_nLive ][ 2 ] <= s_aLive[ 2 ][ 2 ], ;
            "counters: no net growth of live blocks between the clean of cycle 2 and the clean of cycle " + hb_ntos( s_nLive ) )
      ENDIF
   ENDIF
   IF s_aShowMs != NIL .AND. s_nLive != NIL .AND. s_nLive >= 2
      nMin := 1000000
      nMax := 1000000
      FOR EACH a IN s_aShowMs
         IF HB_ISLOGICAL( a[ 1 ] )
            IF a[ 1 ]
               nMax := a[ 2 ]
            ELSE
               nMin := Min( nMin, a[ 2 ] )
            ENDIF
         ENDIF
      NEXT
      T_Diag( "show time: default " + hb_ntos( nMin ) + " ms at best, show_wait_connection off " + hb_ntos( nMax ) + " ms" )
      T_Ok( nMax * 2 < nMin, "set_config(show_wait_connection, .F.) makes show return before the browser connected" )
   ENDIF
   IF WEBUI_BRIDGE_STAT( "dirty" ) == 1
      WEBUI_CLEAN()
   ENDIF
   T_Ok( WEBUI_BRIDGE_STAT( "live_bytes" ) == 0 .AND. WEBUI_BRIDGE_STAT( "allocs" ) == WEBUI_BRIDGE_STAT( "frees" ), ;
      "at the end every bridge-owned allocation is freed (counted by the bridge)" )

RETURN
