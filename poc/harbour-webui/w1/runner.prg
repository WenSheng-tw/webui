/*
 * runner.prg  --  dual-mode test runner (spec section 8).
 *
 *   w1run-linked   built with -DMODE_LINKED: tests/t_w1.prg is linked into the exe and
 *                  called directly (mode 1).
 *   w1run-hrb      built with -DMODE_HRB: tests/t_w1.prg is not linked; the runner
 *                  prepares tests/t_w1.hrb (hash rules, hrbtool.prg) and runs it with
 *                  hb_hrbLoad / hb_hrbDo (mode 2).
 *
 * Options: --suite=all|headless|gui   --dir=tests   --selftest (executor self-test only)
 *          --prg=FILE --hrb=FILE (mode 2 only)
 * Exit code 0 when every TAP line is ok.
 */

#include "hbextern.ch"

REQUEST WEBUI_PUMP
REQUEST WEBUI_BRIDGE_STAT
REQUEST WEBUI_NEWWINDOW
REQUEST WEBUI_BINDHB
REQUEST W1T_NUM
REQUEST W1T_N2SIZE
REQUEST W1T_RANGE
REQUEST W1T_COUNTER

PROCEDURE Main( ... )

   LOCAL cSuite := "all"
   LOCAL cDir := "tests"
#ifdef MODE_HRB
   LOCAL cPrg
   LOCAL cHrb
   LOCAL hRes
#endif
   LOCAL lSelf := .F.
   LOCAL cArg

   FOR EACH cArg IN hb_AParams()
      DO CASE
      CASE Left( cArg, 8 ) == "--suite="
         cSuite := SubStr( cArg, 9 )
      CASE Left( cArg, 6 ) == "--dir="
         cDir := SubStr( cArg, 7 )
#ifdef MODE_HRB
      CASE Left( cArg, 6 ) == "--prg="
         cPrg := SubStr( cArg, 7 )
      CASE Left( cArg, 6 ) == "--hrb="
         cHrb := SubStr( cArg, 7 )
#endif
      CASE cArg == "--selftest"
         lSelf := .T.
      OTHERWISE
         OutStd( "unknown option " + cArg + hb_eol() )
         ErrorLevel( 2 )
         RETURN
      ENDCASE
   NEXT

   OutStd( "TAP version 13" + hb_eol() )
   T_Diag( "mode " + iif( RunMode() == "linked", "1 (linked)", "2 (hrb)" ) + ", suite " + cSuite + ", " + hb_Version() )

   IF lSelf
      SelfTest()
      ErrorLevel( T_Done() )
      RETURN
   ENDIF

#ifdef MODE_LINKED
   T_W1_RUN( cDir, cSuite )
#else
   IF cPrg == NIL
      cPrg := cDir + hb_ps() + "t_w1.prg"
   ENDIF
   IF cHrb == NIL
      cHrb := hb_FNameExtSet( cPrg, ".hrb" )
   ENDIF
   hRes := HrbPrepare( cPrg, cHrb )
   T_Diag( "hrb action: " + hRes[ "action" ] )
   IF hRes[ "action" ] $ "error|missing"
      T_Ok( .F., "hrb available (" + hRes[ "action" ] + " " + hRes[ "error" ] + ")" )
   ELSE
      HrbRun( cHrb, cDir, cSuite )
   ENDIF
#endif

   ErrorLevel( T_Done() )

RETURN

FUNCTION RunMode()
#ifdef MODE_LINKED
RETURN "linked"
#else
RETURN "hrb"
#endif
