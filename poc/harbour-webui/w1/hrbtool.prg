/*
 * hrbtool.prg  --  spec 8.4: compile a test .prg to .hrb, with .hrb.md5 hash sidecar.
 *
 *   HrbPrepare( cPrg, cHrb ) -> hash { "action" => ..., "error" => ... }
 *
 * The sidecar <hrb>.md5 holds two lines: the md5 of the .prg bytes and hb_Version().
 *   prg missing, hrb present          action "hrb-only"  (hash is not checked)
 *   prg missing, hrb missing          action "missing"
 *   hrb and sidecar present, both lines equal to the current values
 *                                     action "reuse"     (no compile)
 *   otherwise                         action "compile"   (hrb written, then sidecar, only
 *                                     after a successful compile) or "error"
 */

REQUEST HB_COMPILEBUF
REQUEST HB_HRBLOAD
REQUEST HB_HRBDO
REQUEST HB_HRBUNLOAD
REQUEST HB_MD5FILE

FUNCTION HrbSidecar( cHrb )
RETURN cHrb + ".md5"

FUNCTION HrbSidecarText( cPrg )
RETURN hb_md5file( cPrg ) + hb_eol() + hb_Version() + hb_eol()

STATIC FUNCTION ReadSidecar( cHrb )

   LOCAL cText := hb_MemoRead( HrbSidecar( cHrb ) )
   LOCAL aLines := hb_ATokens( StrTran( cText, Chr( 13 ), "" ), Chr( 10 ) )

   IF Len( aLines ) < 2
      RETURN { "", "" }
   ENDIF

RETURN { aLines[ 1 ], aLines[ 2 ] }

FUNCTION HrbPrepare( cPrg, cHrb )

   LOCAL hRes := { "action" => "", "error" => "" }
   LOCAL aSide
   LOCAL cBuf

   IF ! hb_FileExists( cPrg )
      hRes[ "action" ] := iif( hb_FileExists( cHrb ), "hrb-only", "missing" )
      RETURN hRes
   ENDIF

   IF hb_FileExists( cHrb ) .AND. hb_FileExists( HrbSidecar( cHrb ) )
      aSide := ReadSidecar( cHrb )
      IF aSide[ 1 ] == hb_md5file( cPrg ) .AND. aSide[ 2 ] == hb_Version()
         hRes[ "action" ] := "reuse"
         RETURN hRes
      ENDIF
   ENDIF

   cBuf := hb_compileBuf( "harbour", "-n", "-w", "-q0", "-gh", cPrg )
   IF ! HB_ISSTRING( cBuf ) .OR. Empty( cBuf )
      hRes[ "action" ] := "error"
      hRes[ "error" ] := "compile failed: " + cPrg
      RETURN hRes
   ENDIF
   IF ! hb_MemoWrit( cHrb, cBuf )
      hRes[ "action" ] := "error"
      hRes[ "error" ] := "cannot write " + cHrb
      RETURN hRes
   ENDIF
   IF ! hb_MemoWrit( HrbSidecar( cHrb ), HrbSidecarText( cPrg ) )
      hRes[ "action" ] := "error"
      hRes[ "error" ] := "cannot write " + HrbSidecar( cHrb )
      RETURN hRes
   ENDIF
   hRes[ "action" ] := "compile"

RETURN hRes

/* Loads the .hrb and runs its first function; returns its result. */
FUNCTION HrbRun( cHrb, ... )

   LOCAL pHrb := hb_hrbLoad( cHrb )
   LOCAL xRet

   IF pHrb == NIL
      RETURN NIL
   ENDIF
   xRet := hb_hrbDo( pHrb, ... )
   hb_hrbUnload( pHrb )

RETURN xRet
