

#include	"CGBL_dmesh.h"
#include	"dmesh.h"
#include	"ApproximateMatch.h"
#include	"RegionToRegionMap.h"

#include	"Disk.h"
#include	"ImageIO.h"
#include	"Inspect.h"
#include	"LinEqu.h"


/* --------------------------------------------------------------- */
/* Macros -------------------------------------------------------- */
/* --------------------------------------------------------------- */

#define	FITPOINTS	0

/* --------------------------------------------------------------- */
/* Types --------------------------------------------------------- */
/* --------------------------------------------------------------- */

class CStatus {

public:
	int		argn,
			brgn,
			thmok,
			ntri;
	TAffine	T;

public:
	CStatus( int a, int b )	{argn=a; brgn=b; thmok=0; ntri=0;};
};


#if FITPOINTS

class PPair {

public:
	Point	A, B;

public:
	PPair( const Point& pa, const Point& pb )
	{A = pa; B = pb;};
};

#endif

/* --------------------------------------------------------------- */
/* Globals ------------------------------------------------------- */
/* --------------------------------------------------------------- */

/* --------------------------------------------------------------- */
/* Statics ------------------------------------------------------- */
/* --------------------------------------------------------------- */






/* --------------------------------------------------------------- */
/* RoughMatch ---------------------------------------------------- */
/* --------------------------------------------------------------- */

static bool RoughMatch(
	vector<TAffine>		&guesses,
	const PixPair		&px,
	const ConnRegion	&acr,
	const ConnRegion	&bcr,
	FILE*				flog )
{
	if( guesses.size() > 0 )
		return true;

	if( GBL.ctx.FLD == 'N' ) {

		// Call NoCR at most once. Possible states
		// are {0=never called, 1=failed, 2=success}.

		static vector<TAffine>	T;
		static int				state = 0;

		int	calledthistime = false;

		if( !state ) {
			state = 1 + ApproximateMatch_NoCR( T, px, flog );
			calledthistime = true;
		}

		if( state == 2 ) {

			if( !calledthistime ) {
				fprintf( flog, "\n---- Thumbnail matching ----\n" );
				T[0].TPrint( flog, "Reuse Approx: Best transform " );
			}

			guesses.push_back( T[0] );
			return true;
		}

		if( !calledthistime ) {
			fprintf( flog, "\n---- Thumbnail matching ----\n" );
			fprintf( flog, "FAIL: Approx: Case already failed.\n" );
		}

		return false;
	}
	else
		return ApproximateMatch( guesses, px, acr, bcr, flog );
}

/* --------------------------------------------------------------- */
/* CheckTransforms ----------------------------------------------- */
/* --------------------------------------------------------------- */

// All checking done at scaled (downsampled) sizes.
//
static int CheckTransforms( const vector<TAffine> &vT, FILE* flog )
{
	int	nXY, nT = vT.size();

	if( nT > 1 ) {

		// compute value spread for each transform element

		double			ang_min = PI;
		double			ang_max = -PI;
		vector<double>	pmax( 6, -1e30 );
		vector<double>	pmin( 6,  1e30 );
		vector<double>	pspan( 6 );
		bool			extreme = false;

		// collect maxima and minima

		for( int i = 0; i < nT; ++i ) {

			double	ang =  vT[i].GetRadians();

			// handle atan() branch cuts

			if( i > 0 ) {

				if( ang > ang_max + PI )
					ang = ang - 2.0*PI;
				else if( ang < ang_min - PI )
					ang = ang + 2.0*PI;
			}

			ang_min = fmin( ang_min, ang );
			ang_max = fmax( ang_max, ang );

			for( int j = 0; j < 6; ++j ) {
				pmax[j] = max( pmax[j], vT[i].t[j] );
				pmin[j] = min( pmin[j], vT[i].t[j] );
			}
		}

		fprintf( flog,
		"Pipe: Angle span: min, max, delta = %f %f %f\n",
		ang_min, ang_max, ang_max - ang_min );

		// now calculate spans

		for( int j = 0; j < 6; ++j ) {

			pspan[j] = pmax[j] - pmin[j];

			if( j == 2 || j == 5 )	// translation
				extreme |= pspan[j] > GBL.mch.LDC;
			else					// rotation
				extreme |= pspan[j] > GBL.mch.LDR;
		}

		// any extreme spans?

		if( ang_max - ang_min > GBL.mch.LDA || extreme ) {

			fprintf( flog,
			"FAIL: Pipe: Triangles too different:"
			" angles %f %f %f; LDA %f\n",
			ang_min, ang_max, ang_max-ang_min, GBL.mch.LDA );

			fprintf( flog,
			"FAIL: Pipe: Triangles too different:"
			" elements %f %f %f %f %f %f; LDR %f LDC %f\n",
			pspan[0], pspan[1], pspan[2],
			pspan[3], pspan[4], pspan[5],
			GBL.mch.LDR, GBL.mch.LDC );

			nT = 0;
		}
	}

// Translations in bounds?

	if( nT && (nXY = GBL.XYexp.size()) ) {

		bool	allok = true;

		for( int i = 0; i < nT; ++i ) {

			bool	iok = false;

			// iok true if ANY bound satisfied

			for( int j = 0; j < nXY; ++j ) {

				Point	p( vT[i].t[2], vT[i].t[5] );

				iok |= p.Dist( GBL.XYexp[j] ) <= GBL.mch.DXY;
			}

			if( !iok ) {

				fprintf( flog,
				"Pipe: Transform translation (%f %f)"
				" not on allowed list, tolerance %f\n",
				vT[i].t[2], vT[i].t[5], GBL.mch.DXY );
			}

			allok &= iok;
		}

		if( !allok ) {

			fprintf( flog,
			"FAIL: Pipe: Transform not on allowed list.\n" );

			nT = 0;
		}
	}

	return nT;
}

/* --------------------------------------------------------------- */
/* DifFromThm ---------------------------------------------------- */
/* --------------------------------------------------------------- */

static int DifFromThm(
	const vector<TAffine>	&vT,
	const TAffine			&T0,
	FILE*					flog )
{
// Average the final trans

	TAffine	A	= vT[0];
	int		nT	= vT.size();

	if( nT > 1 ) {

		for( int i = 1; i < nT; ++i ) {

			for( int j = 0; j < 6; ++j )
				A.t[j] += vT[i].t[j];
		}

		for( int j = 0; j < 6; ++j )
			A.t[j] /= nT;
	}

// Differences

	const int	f = 2;
	double		LDR = f * GBL.mch.LDR,
				LDC = f * GBL.mch.LDC;
	double		D[6];

	for( int i = 0; i < 6; ++i )
		D[i] = fabs( A.t[i] - T0.t[i] );

	fprintf( flog,
	"Pipe: Mesh-Thm:%7.4f %7.4f %8.2f   %7.4f %7.4f %8.2f"
	" %dx(LDR,LDC)=(%.2f,%.2f) -- ",
	D[0], D[1], D[2], D[3], D[4], D[5],
	f, LDR, LDC );

// Check

	for( int i = 0; i < 6; ++i ) {

		if( i == 2 || i == 5 ) {
			if( D[i] > LDC )
				goto reject;
		}
		else if( D[i] > LDR )
			goto reject;
	}

	fprintf( flog, "OK.\n" );
	return false;

reject:
	fprintf( flog, "Reject.\n" );
	return true;
}

/* --------------------------------------------------------------- */
/* UpscaleCoords ------------------------------------------------- */
/* --------------------------------------------------------------- */

static void UpscaleCoords( ffmap &maps, int scale )
{
	int	nT;

	if( scale > 1 && (nT = maps.transforms.size()) ) {

		for( int i = 0; i < nT; ++i ) {

			maps.transforms[i].MulXY( scale );

			maps.centers[i].x *= scale;
			maps.centers[i].y *= scale;
		}
	}
}


#if FITPOINTS

/* --------------------------------------------------------------- */
/* FitAffine ----------------------------------------------------- */
/* --------------------------------------------------------------- */

static void FitAffine(
	const PixPair		&px,
	const vector<PPair>	&pair,
	FILE				*flog )
{
	int	np = pair.size();

	if( np < 3 ) {
		fprintf( flog,
		"Pipe: Too few points to fit affine [%d].\n", np );
		return;
	}

// Create system of normal equations

	vector<double>	X( 6 );
	vector<double>	RHS( 6, 0.0 );
	vector<LHSCol>	LHS( 6 );
	int				i1[3] = { 0, 1, 2 },
					i2[3] = { 3, 4, 5 };

	for( int i = 0; i < np; ++i ) {

		const Point&	A = pair[i].A;
		const Point&	B = pair[i].B;

		double	v[3] = { A.x, A.y, 1.0 };

		AddConstraint( LHS, RHS, 3, i1, v, B.x );
		AddConstraint( LHS, RHS, 3, i2, v, B.y );
	}

// Solve

	WriteSolveRead( X, LHS, RHS, true );

	TAffine	T( &X[0] );

// Report

	T.TPrint( flog, "Pipe: FitAffine: " );

// RMS error

	double	E = 0;;

	for( int i = 0; i < np; ++i ) {

		Point	a = pair[i].A;

		T.Transform( a );

		double	err = a.DistSqr( pair[i].B );

		E += err;
	}

	E /= np;
	fprintf( flog, "Pipe: FitAffineRMSerr: %g\n", E );

// Paint

	YellowView( px, T );
}

/* --------------------------------------------------------------- */
/* FitHmgphy ----------------------------------------------------- */
/* --------------------------------------------------------------- */

static void FitHmgphy(
	const PixPair		&px,
	const vector<PPair>	&pair,
	FILE				*flog )
{
	int	np = pair.size();

	if( np < 4 ) {
		fprintf( flog,
		"Pipe: Too few points to fit homography [%d].\n", np );
		return;
	}

// Create system of normal equations

	vector<double>	X( 8 );
	vector<double>	RHS( 8, 0.0 );
	vector<LHSCol>	LHS( 8 );
	int				i1[5] = { 0, 1, 2, 6, 7 },
					i2[5] = { 3, 4, 5, 6, 7 };

	for( int i = 0; i < np; ++i ) {

		const Point&	A = pair[i].A;
		const Point&	B = pair[i].B;

		double	v[5] = { A.x, A.y, 1.0, -A.x*B.x, -A.y*B.x };

		AddConstraint( LHS, RHS, 5, i1, v, B.x );

		v[3] = -A.x*B.y;
		v[4] = -A.y*B.y;

		AddConstraint( LHS, RHS, 5, i2, v, B.y );
	}

// Solve

	WriteSolveRead( X, LHS, RHS, true );

	THmgphy	T( &X[0] );

// Report

	T.TPrint( flog, "Pipe: FitHmgphy: " );

// RMS error

	double	E = 0;;

	for( int i = 0; i < np; ++i ) {

		Point	a = pair[i].A;

		T.Transform( a );

		double	err = a.DistSqr( pair[i].B );

		E += err;
	}

	E = sqrt( E / np );

	fprintf( flog, "Pipe: FitAHmgphyRMSerr: %g\n", E );

// Paint

	YellowView( px, T );
}

#endif

/* --------------------------------------------------------------- */
/* WritePOINTEntries --------------------------------------------- */
/* --------------------------------------------------------------- */

// CPOINT2 entries are reported at full size.
//
static void WritePOINTEntries(
	const PixPair	&px,
	const ffmap		&maps,
	const uint8*	fold_mask_a,
	const uint8*	fold_mask_b,
	int				wf,
	int				hf,
	FILE*			flog )
{
	CMutex	M;
	char	name[256];
	char	*sud;

// set pts file type and layer

	if( GBL.A.layer < GBL.B.layer )
		sud = "up";
	else if( GBL.A.layer == GBL.B.layer )
		sud = "same";
	else
		sud = "down";

	sprintf( name, "%s_%d", sud, GBL.A.layer );

	if( M.Get( name ) ) {

		sprintf( name, "pts.%s", sud );
		FILE *f = fopen( name, "a" );

		if( f ) {

#if FITPOINTS
			vector<PPair>	pair;
#endif

			int				nT = maps.transforms.size();

			for( int i = 0; i < nT; ++i ) {

				Point	pa = maps.centers[i],
						pb = pa;
				int		ma, mb, ix, iy;

				// Lookup for A-point

				ix = int(pa.x);
				iy = int(pa.y);

				if( ix >= 0 && ix < wf && iy >= 0 && iy < hf ) {

					ma = fold_mask_a[ix + wf*iy];

					if( ma <= 0 ) {

						fprintf( flog,
						"CPOINT2: A-centroid has bad mask value:"
						" mask=%d @ (%d, %d).\n", ma, ix, iy );

						continue;
					}
				}
				else {

					fprintf( flog,
					"CPOINT2: A-centroid out of A-image bounds"
					" (%d, %d).\n", ix, iy );

					continue;
				}

				// Lookup for B-point

				maps.transforms[i].Transform( pb );

				ix = int(pb.x);
				iy = int(pb.y);

				if( ix >= 0 && ix < wf && iy >= 0 && iy < hf ) {

					mb = fold_mask_b[ix + wf*iy];

					if( mb <= 0 ) {

						fprintf( flog,
						"CPOINT2: B-centroid has bad mask value:"
						" mask=%d @ (%d, %d).\n", mb, ix, iy );

						continue;
					}
				}
				else {

					fprintf( flog,
					"CPOINT2: B-centroid out of B-image bounds"
					" (%d, %d).\n", ix, iy );

					continue;
				}

				// Write entry

				fprintf( f,
				"CPOINT2"
				" %d.%d:%d %f %f"
				" %d.%d:%d %f %f\n",
				GBL.A.layer, GBL.A.tile, ma, pa.x, pa.y,
				GBL.B.layer, GBL.B.tile, mb, pb.x, pb.y );

#if FITPOINTS
				// Accumulate point pair

				pair.push_back( PPair( pa, pb ) );
#endif
			}

#if FITPOINTS
			// Model transforms from point pairs

			FitAffine( px, pair, flog );
			FitHmgphy( px, pair, flog );
#endif

			// done

			fflush( f );
			fclose( f );
		}
	}

	M.Release();
}

/* --------------------------------------------------------------- */
/* PipelineDeformableMap ----------------------------------------- */
/* --------------------------------------------------------------- */

// The main routine the pipeline should call.
//
// Ntrans		- returned tform count
// tr_array		- array of these values
// map_mask		- <10=no map, else tform[pix-10]
// px			- a and b image pixels
// fold_mask_a	- 0=fold, 1,2,3...=region #
// fold_mask_b	- 0=fold, 1,2,3...=region #
// flog			- detailed output
//
void PipelineDeformableMap(
	int				&Ntrans,
	double*			&tr_array,
	uint16*			map_mask,
	const PixPair	&px,
	const uint8*	fold_mask_a,
	const uint8*	fold_mask_b,
	FILE*			flog )
{
	int	wf = px.wf, hf = px.hf;

/* --------------- */
/* Default results */
/* --------------- */

	Ntrans		= 0;
	tr_array	= NULL;

	memset( map_mask, 0, wf * hf * sizeof(uint16) );

/* --------------------------------- */
/* Create the connected region lists */
/* --------------------------------- */

// Note that the connected region lists are always
// at the reduced resolution, if this is used.

	fprintf( flog, "\n---- Connected regions ----\n" );

	vector<ConnRegion>	Acr, Bcr;

	if( GBL.ctx.FLD == 'N' ) {

		fprintf( flog, "Forcing single connected region.\n" );

		ConnRgnForce1( Acr, px.ws, px.hs );
		Bcr = Acr;
	}
	else {

		ConnRgnsFromFoldMask( Acr, fold_mask_a,
			wf, hf, px.scl, uint32(0.9 * GBL.mch.MMA), flog );

		ConnRgnsFromFoldMask( Bcr, fold_mask_b,
			wf, hf, px.scl, uint32(0.9 * GBL.mch.MMA), flog );
	}

/* ----------------------------------------- */
/* Find mappings for each region-region pair */
/* ----------------------------------------- */

	vector<CStatus>	vstat;
	ffmap			maps;  // transforms and centers
	FILE			*ftri	= NULL;

	//ftri = fopen( "Triangles.txt", "w" );

	for( int i = 0; i < Acr.size(); ++i ) {

		for( int j = 0; j < Bcr.size(); ++j ) {

			fprintf( flog, "\n---- Begin A-%d to B-%d ----\n",
			Acr[i].id, Bcr[j].id );

			// start list with user's transform arguments

			CStatus			stat( Acr[i].id, Bcr[j].id );
			vector<TAffine>	guesses = GBL.Tmsh;

			if( RoughMatch( guesses, px, Acr[i], Bcr[j], flog ) ) {

				stat.thmok = true;

				// Try to get detailed mesh solution from each
				// guess {user + all returned from RoughMatch}.
				// The first to be successful (diff count > 0)
				// causes break.

				for( int k = 0; k < guesses.size(); ++k ) {

					int	count = maps.transforms.size();

					// Downscale coordinates
					guesses[k].MulXY( 1.0 / px.scl );
					stat.T = guesses[k];

					RegionToRegionMap( maps, map_mask,
						px, Acr[i], Bcr[j],
						guesses[k], flog, ftri );

					count = maps.transforms.size() - count;

					stat.ntri = count;

					if( count )
						break;
				}
			}

			vstat.push_back( stat );
		}
	}

	//if( ftri )
	//	fclose( ftri );

/* ------------------------------------- */
/* Report thumb and triangle raw results */
/* ------------------------------------- */

	fprintf( flog,
	"\n---- Summary Region-region results ----\n" );

	fprintf( flog,
	"Pipe: Before sanity checks {Az t r Bz t r thmok ntri}\n" );

	int	nstat = vstat.size();

	for( int i = 0; i < nstat; ++i ) {

		const CStatus& S = vstat[i];

		fprintf( flog,
		"FOUND: %5d %4d %3d  %5d %4d %3d %3d %3d\n",
		GBL.A.layer, GBL.A.tile, S.argn,
		GBL.B.layer, GBL.B.tile, S.brgn,
		S.thmok, S.ntri );
	}

	fprintf( flog, "\n" );

/* -------------------- */
/* Sanity check results */
/* -------------------- */

	if( maps.transforms.size() ) {

		// @@@ IMPORTANT!!
		// These checks do not currently observe the fact
		// that the transform list covers multiple conn
		// regions. Rather, the stat records should be
		// used to loop over conn regions.

		Ntrans = CheckTransforms( maps.transforms, flog );

		if( Ntrans &&
			DifFromThm( maps.transforms, vstat[0].T, flog ) ) {

			Ntrans = 0;
		}
	}

	fprintf( flog,
	"Pipe: After sanity checks, returning %d transforms.\n", Ntrans );

/* ------ */
/* Report */
/* ------ */

	if( Ntrans ) {

		UpscaleCoords( maps, px.scl );

		WritePOINTEntries( px, maps, fold_mask_a, fold_mask_b,
			wf, hf, flog );

		tr_array = (double*)malloc( Ntrans * 6 * sizeof(double) );

		for( int i = 0; i < Ntrans; ++i ) {

			maps.transforms[i].ToMatlab();
			maps.transforms[i].CopyOut( tr_array + i*6 );
		}
	}
	else
		memset( map_mask, 0, wf * hf * sizeof(uint16) );
}


