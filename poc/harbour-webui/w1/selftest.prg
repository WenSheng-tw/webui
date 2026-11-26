/*
 * selftest.prg  --  executor self-test (spec 8.4, acceptance A13).
 *
 * Exercises HrbPrepare() on throw-away .prg files in a temporary folder, without WebUI:
 *   1 no hrb                       -> compile, hrb and two-line sidecar written
 *   2 same prg, hash identical     -> reuse; proven by replacing the hrb behind the
 *                                     sidecar's back and seeing the replacement still run
 *   3 prg changed                  -> recompile, sidecar updated
 *   4 prg removed                  -> hrb-only, existing hrb runs, sidecar not consulted
 *   5 sidecar version line changed -> recompile
 *   6 sidecar removed              -> recompile
 *   7 syntax error                 -> error, hrb and sidecar unchanged
 *   8 neither prg nor hrb          -> missing
 * The timestamps of the files are never used.
 */

STATIC FUNCTION Write( cFile, cText )
RETURN hb_MemoWrit( cFile, cText )

STATIC FUNCTION Src( nValue )
RETURN "FUNCTION ST_VALUE()" + hb_eol() + "RETURN " + hb_ntos( nValue ) + hb_eol()

STATIC FUNCTION Compile( cText )

   LOCAL cTmp := hb_FNameMerge( hb_DirTemp(), "st_c" + hb_ntos( hb_RandomInt( 1000000 ) ), ".prg" )
   LOCAL cBuf

   Write( cTmp, cText )
   cBuf := hb_compileBuf( "harbour", "-n", "-w", "-q0", "-gh", cTmp )
   FErase( cTmp )

RETURN cBuf

PROCEDURE SelfTest()

   LOCAL cDir := hb_DirTemp() + "w1st_" + hb_ntos( hb_RandomInt( 100000000 ) ) + hb_ps()
   LOCAL cPrg := cDir + "st.prg"
   LOCAL cHrb := cDir + "st.hrb"
   LOCAL cSide := cHrb + ".md5"
   LOCAL hRes
   LOCAL cHrbBefore
   LOCAL cSideBefore
   LOCAL aLines

   hb_DirCreate( cDir )

   T_Ok( HrbPrepare( cPrg, cHrb )[ "action" ] == "missing", "A13 self: neither prg nor hrb gives missing" )

   Write( cPrg, Src( 41 ) )
   hRes := HrbPrepare( cPrg, cHrb )
   T_Ok( hRes[ "action" ] == "compile", "A13 self: no hrb compiles" )
   T_Ok( hb_FileExists( cHrb ) .AND. hb_FileExists( cSide ), "A13 self: hrb and sidecar exist after compile" )
   aLines := hb_ATokens( StrTran( hb_MemoRead( cSide ), Chr( 13 ), "" ), Chr( 10 ) )
   T_Ok( Len( aLines ) >= 2 .AND. aLines[ 1 ] == hb_md5file( cPrg ) .AND. aLines[ 2 ] == hb_Version(), ;
      "A13 self: sidecar line 1 is md5 of the prg, line 2 is hb_Version()" )
   T_Ok( Len( aLines[ 1 ] ) == 32 .AND. aLines[ 1 ] == Lower( aLines[ 1 ] ), "A13 self: md5 is 32 lowercase hex characters" )
   T_Ok( HrbRun( cHrb ) == 41, "A13 self: compiled hrb runs and returns 41" )

   Write( cHrb, Compile( Src( 99 ) ) )
   hRes := HrbPrepare( cPrg, cHrb )
   T_Ok( hRes[ "action" ] == "reuse", "A13 self: identical hash skips the compile" )
   T_Ok( HrbRun( cHrb ) == 99, "A13 self: reuse really kept the existing hrb (replacement still runs)" )

   Write( cPrg, Src( 42 ) )
   hRes := HrbPrepare( cPrg, cHrb )
   T_Ok( hRes[ "action" ] == "compile", "A13 self: changed prg recompiles" )
   T_Ok( HrbRun( cHrb ) == 42, "A13 self: recompiled hrb returns 42" )
   T_Ok( hb_MemoRead( cSide ) == HrbSidecarText( cPrg ), "A13 self: sidecar updated to the new hash" )

   Write( cPrg, Src( 42 ) + "/* same value, different bytes */" + hb_eol() )
   T_Ok( HrbPrepare( cPrg, cHrb )[ "action" ] == "compile", "A13 self: any byte change of the prg recompiles" )

   Write( cSide, "0123456789abcdef0123456789abcdef" + hb_eol() + "other version" + hb_eol() )
   T_Ok( HrbPrepare( cPrg, cHrb )[ "action" ] == "compile", "A13 self: wrong hash and version in sidecar recompiles" )

   Write( cSide, hb_md5file( cPrg ) + hb_eol() + "other version" + hb_eol() )
   T_Ok( HrbPrepare( cPrg, cHrb )[ "action" ] == "compile", "A13 self: right hash but other Harbour version recompiles" )

   FErase( cSide )
   T_Ok( HrbPrepare( cPrg, cHrb )[ "action" ] == "compile", "A13 self: missing sidecar recompiles" )
   T_Ok( HrbPrepare( cPrg, cHrb )[ "action" ] == "reuse", "A13 self: sidecar written again, next call reuses" )

   cHrbBefore := hb_MemoRead( cHrb )
   cSideBefore := hb_MemoRead( cSide )
   Write( cPrg, "FUNCTION ST_VALUE(" + hb_eol() + "RETURN" + hb_eol() )
   hRes := HrbPrepare( cPrg, cHrb )
   T_Ok( hRes[ "action" ] == "error", "A13 self: syntax error gives error" )
   T_Ok( hb_MemoRead( cHrb ) == cHrbBefore .AND. hb_MemoRead( cSide ) == cSideBefore, ;
      "A13 self: failed compile leaves hrb and sidecar untouched" )

   Write( cSide, "garbage" + hb_eol() )
   FErase( cPrg )
   hRes := HrbPrepare( cPrg, cHrb )
   T_Ok( hRes[ "action" ] == "hrb-only", "A13 self: missing prg uses the hrb" )
   T_Ok( HrbRun( cHrb ) == 42, "A13 self: hrb-only run returns the old value 42" )
   T_Ok( hb_MemoRead( cSide ) == "garbage" + hb_eol(), "A13 self: hrb-only leaves the sidecar alone" )

   FErase( cHrb )
   T_Ok( HrbPrepare( cPrg, cHrb )[ "action" ] == "missing", "A13 self: prg and hrb both gone gives missing" )

   FErase( cSide )
   hb_DirDelete( cDir )
   T_Ok( ! hb_DirExists( cDir ), "A13 self: temporary folder removed" )

RETURN
