/*
 * testlib.prg  --  tiny TAP helper shared by the runner, the self-test and the W1 tests.
 *
 * Output goes to stdout through OutStd(). Lines starting with "#" are diagnostics and
 * are ignored when the two runner modes are compared (run-w1.sh).
 */

STATIC s_nTest := 0
STATIC s_nFail := 0

FUNCTION T_Ok( lCond, cDesc )

   s_nTest++
   IF ! HB_ISLOGICAL( lCond ) .OR. ! lCond
      s_nFail++
      OutStd( "not ok " + hb_ntos( s_nTest ) + " - " + cDesc + hb_eol() )
      RETURN .F.
   ENDIF
   OutStd( "ok " + hb_ntos( s_nTest ) + " - " + cDesc + hb_eol() )

RETURN .T.

PROCEDURE T_Diag( cText )

   OutStd( "# " + cText + hb_eol() )

RETURN

FUNCTION T_Raises( bBlock, cDesc )

   LOCAL lRaised := .F.
   LOCAL cMsg := ""
   LOCAL oErr

   BEGIN SEQUENCE WITH {| e | Break( e ) }
      Eval( bBlock )
   RECOVER USING oErr
      lRaised := .T.
      IF HB_ISOBJECT( oErr )
         cMsg := oErr:description
      ENDIF
   END SEQUENCE

   IF lRaised
      T_Diag( cDesc + ": " + cMsg )
   ENDIF

RETURN T_Ok( lRaised, cDesc )

FUNCTION T_Pass( bBlock, cDesc )

   LOCAL lRaised := .F.
   LOCAL cMsg := ""
   LOCAL oErr

   BEGIN SEQUENCE WITH {| e | Break( e ) }
      Eval( bBlock )
   RECOVER USING oErr
      lRaised := .T.
      IF HB_ISOBJECT( oErr )
         cMsg := oErr:description
      ENDIF
   END SEQUENCE

   IF lRaised
      T_Diag( cDesc + " raised: " + cMsg )
   ENDIF

RETURN T_Ok( ! lRaised, cDesc )

/* Pumps the bridge until bCond is true or nSeconds passed. Main thread only. */
FUNCTION T_WaitFor( bCond, nSeconds )

   LOCAL nEnd := hb_MilliSeconds() + nSeconds * 1000

   DO WHILE ! Eval( bCond )
      IF hb_MilliSeconds() > nEnd
         RETURN .F.
      ENDIF
      WEBUI_PUMP()
      hb_idleSleep( 0.005 )
   ENDDO

RETURN .T.

FUNCTION T_Count()
RETURN s_nTest

FUNCTION T_Failures()
RETURN s_nFail

FUNCTION T_Done()

   OutStd( "1.." + hb_ntos( s_nTest ) + hb_eol() )
   IF s_nFail > 0
      OutStd( "# FAILED " + hb_ntos( s_nFail ) + " of " + hb_ntos( s_nTest ) + hb_eol() )
   ENDIF

RETURN iif( s_nFail == 0, 0, 1 )
