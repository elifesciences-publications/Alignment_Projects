

#include	"CGBL_Thumbs.h"

#include	"Cmdline.h"


/* --------------------------------------------------------------- */
/* Constants ----------------------------------------------------- */
/* --------------------------------------------------------------- */

/* --------------------------------------------------------------- */
/* Macros -------------------------------------------------------- */
/* --------------------------------------------------------------- */

/* --------------------------------------------------------------- */
/* Types --------------------------------------------------------- */
/* --------------------------------------------------------------- */

/* --------------------------------------------------------------- */
/* Globals ------------------------------------------------------- */
/* --------------------------------------------------------------- */

CGBL_Thumbs	GBL;

/* --------------------------------------------------------------- */
/* Statics ------------------------------------------------------- */
/* --------------------------------------------------------------- */






/* --------------------------------------------------------------- */
/* Object management --------------------------------------------- */
/* --------------------------------------------------------------- */

CGBL_Thumbs::CGBL_Thumbs()
{
	arg.SCALE			= 999.0;
	arg.XSCALE			= 999.0;
	arg.YSCALE			= 999.0;
	arg.SKEW			= 999.0;
	arg.CTR				= 999.0;
	arg.ima				= NULL;
	arg.imb				= NULL;
	arg.fma				= NULL;
	arg.fmb				= NULL;
	arg.Transpose		= false;
	arg.NoFolds			= false;
	arg.SingleFold		= false;

	A.layer	= 0;
	A.tile	= 0;

	B.layer	= 0;
	B.tile	= 0;
}

/* --------------------------------------------------------------- */
/* SetCmdLine ---------------------------------------------------- */
/* --------------------------------------------------------------- */

bool CGBL_Thumbs::SetCmdLine( int argc, char* argv[] )
{
// Parse args

	char	*key;

	for( int i = 1; i < argc; ++i ) {

		if( argv[i][0] != '-' )
			key = argv[i];
		else if( GetArg( &arg.SCALE, "-SCALE=%lf", argv[i] ) )
			;
		else if( GetArg( &arg.XSCALE, "-XSCALE=%lf", argv[i] ) )
			;
		else if( GetArg( &arg.YSCALE, "-YSCALE=%lf", argv[i] ) )
			;
		else if( GetArg( &arg.SKEW, "-SKEW=%lf", argv[i] ) )
			;
		else if( GetArg( &arg.CTR, "-CTR=%lf", argv[i] ) )
			;
		else if( GetArgStr( arg.ima, "-ima=", argv[i] ) )
			;
		else if( GetArgStr( arg.imb, "-imb=", argv[i] ) )
			;
		else if( GetArgStr( arg.fma, "-fma=", argv[i] ) )
			;
		else if( GetArgStr( arg.fmb, "-fmb=", argv[i] ) )
			;
		else if( IsArg( "-tr", argv[i] ) )
			arg.Transpose = true;
		else if( IsArg( "-nf", argv[i] ) )
			arg.NoFolds = true;
		else if( IsArg( "-sf", argv[i] ) )
			arg.SingleFold = true;
		else {
			printf( "Did not understand option '%s'.\n", argv[i] );
			return false;
		}
	}

// Decode labels in key

	if( !key ||
		(4 != sscanf( key, "%d/%d@%d/%d",
				&A.layer, &A.tile,
				&B.layer, &B.tile )) ) {

		printf( "main: Usage: thumbs <za/ia@zb/ib>.\n" );
		return false;
	}

// Rename stdout using image labels

	OpenPairLog( A.layer, A.tile, B.layer, B.tile );

	printf( "\n---- Thumbnail matching ----\n" );

// Record start time

	time_t	t0 = time( NULL );
	printf( "main: Start: %s\n", ctime(&t0) );

// Get default parameters

	if( !ReadThmParams( thm, A.layer ) )
		return false;

// Context dependent choices: (same,cross) layer

	if( A.layer == B.layer ) {

		ctx.scale		= 1.0;
		ctx.xscale		= 1.0;
		ctx.yscale		= 1.0;
		ctx.skew		= 0.0;
		ctx.nbmax		= thm.NBMXHT_SL;
		ctx.halfAngDN	= thm.HFANGDN_SL;
		ctx.halfAngPR	= thm.HFANGPR_SL;
		ctx.rthresh		= thm.RTRSH_SL;
		ctx.min_2D_olap	= thm.OLAP2D_SL;
		ctx.pkwid		= thm.PKWID_SL;
		ctx.pkgrd		= thm.PKGRD_SL;
	}
	else {

		ctx.scale		= thm.SCALE;
		ctx.xscale		= thm.XSCALE;
		ctx.yscale		= thm.YSCALE;
		ctx.skew		= thm.SKEW;
		ctx.nbmax		= thm.NBMXHT_XL;
		ctx.halfAngDN	= thm.HFANGDN_XL;
		ctx.halfAngPR	= thm.HFANGPR_XL;
		ctx.rthresh		= thm.RTRSH_XL;
		ctx.min_2D_olap	= thm.OLAP2D_XL;
		ctx.pkwid		= thm.PKWID_XL;
		ctx.pkgrd		= thm.PKGRD_XL;
	}

// Commandline overrides

	printf( "\n---- Command-line overrides ----\n" );

	if( A.layer != B.layer ) {

		if( arg.SCALE != 999.0 && arg.SCALE != ctx.scale ) {
			ctx.scale = arg.SCALE;
			printf( "SCALE=%g\n", arg.SCALE );
		}

		if( arg.XSCALE != 999.0 && arg.XSCALE != ctx.xscale ) {
			ctx.xscale = arg.XSCALE;
			printf( "XSCALE=%g\n", arg.XSCALE );
		}

		if( arg.YSCALE != 999.0 && arg.YSCALE != ctx.yscale ) {
			ctx.yscale = arg.YSCALE;
			printf( "YSCALE=%g\n", arg.YSCALE );
		}

		if( arg.SKEW != 999.0 && arg.SKEW != ctx.skew ) {
			ctx.skew = arg.SKEW;
			printf( "SKEW=%g\n", arg.SKEW );
		}
	}

	if( arg.CTR != 999.0 )
		printf( "CTR=%g\n", arg.CTR );

	printf( "\n" );

// Fetch Til2Img entries

	printf( "\n---- Input images ----\n" );

	IDBReadImgParams( idb );

	if( !IDBTil2Img( A.t2i, idb, A.layer, A.tile, arg.ima ) ||
		!IDBTil2Img( B.t2i, idb, B.layer, B.tile, arg.imb ) ) {

		return false;
	}

	PrintTil2Img( stdout, 'A', A.t2i );
	PrintTil2Img( stdout, 'B', B.t2i );

	printf( "\n" );

// Extract file name as useful label

	A.file = ExtractFilename( A.t2i.path.c_str() );
	B.file = ExtractFilename( B.t2i.path.c_str() );

	return true;
}


