

#include	"lsq_Hmgphy.h"

#include	"PipeFiles.h"
#include	"File.h"
#include	"Maths.h"

#include	<math.h>


/* --------------------------------------------------------------- */
/* SetPointPairs ------------------------------------------------- */
/* --------------------------------------------------------------- */

void MHmgphy::SetPointPairs(
	vector<LHSCol>	&LHS,
	vector<double>	&RHS,
	double			sc,
	double			same_strength )
{
	int	nc	= vAllC.size();

	for( int i = 0; i < nc; ++i ) {

		const Constraint &C = vAllC[i];

		if( !C.used || !C.inlier )
			continue;

		double	fz =
		(vRgn[C.r1].z == vRgn[C.r2].z ? same_strength : 1);

		double	x1 = C.p1.x * fz / sc,
				y1 = C.p1.y * fz / sc,
				x2 = C.p2.x * fz / sc,
				y2 = C.p2.y * fz / sc;
		int		j  = vRgn[C.r1].itr * NX,
				k  = vRgn[C.r2].itr * NX;

		// T1(p1) - T2(p2) = 0

		double	va[6] = { x1,  y1,  fz, -x2, -y2, -fz};
		int		i1[6] = {  j, j+1, j+2,   k, k+1, k+2};
		int		i2[6] = {j+3, j+4, j+5, k+3, k+4, k+5};

		AddConstraint( LHS, RHS, 6, i1, va, 0.0 );
		AddConstraint( LHS, RHS, 6, i2, va, 0.0 );

		double	vb[4] = { x1,  y1, -x2, -y2};
		int		i3[4] = {j+6, j+7, k+6, k+7};

		AddConstraint( LHS, RHS, 4, i3, vb, 0.0 );
	}
}

/* --------------------------------------------------------------- */
/* SetIdentityTForm ---------------------------------------------- */
/* --------------------------------------------------------------- */

// Explicitly set some TForm to Identity.
// @@@ Does it matter which one we use?
//
void MHmgphy::SetIdentityTForm(
	vector<LHSCol>	&LHS,
	vector<double>	&RHS,
	int				itr )
{
	double	stiff	= 1.0;

	double	one	= stiff;
	int		j	= itr * NX;

	AddConstraint( LHS, RHS, 1, &j, &one, one );	j++;
	AddConstraint( LHS, RHS, 1, &j, &one, 0 );		j++;
	AddConstraint( LHS, RHS, 1, &j, &one, 0 );		j++;
	AddConstraint( LHS, RHS, 1, &j, &one, 0 );		j++;
	AddConstraint( LHS, RHS, 1, &j, &one, one );	j++;
	AddConstraint( LHS, RHS, 1, &j, &one, 0 );		j++;
	AddConstraint( LHS, RHS, 1, &j, &one, 0 );		j++;
	AddConstraint( LHS, RHS, 1, &j, &one, 0 );		j++;

// Report which tile we set

	int	nr = vRgn.size();

	for( int k = 0; k < nr; ++k ) {

		if( vRgn[k].itr == itr ) {

			printf( "Ref region z=%d, id=%d\n",
			vRgn[k].z, vRgn[k].id );
			break;
		}
	}
}

/* --------------------------------------------------------------- */
/* SetUniteLayer ------------------------------------------------- */
/* --------------------------------------------------------------- */

// Set one layer-full of TForms to those from a previous
// solution output file gArgs.tfm_file.
//
void MHmgphy::SetUniteLayer(
	vector<LHSCol>	&LHS,
	vector<double>	&RHS,
	double			sc,
	int				unite_layer,
	const char		*tfm_file )
{
/* ------------------------------- */
/* Load TForms for requested layer */
/* ------------------------------- */

	map<MZIDR,THmgphy>	M;

	LoadTHmgphyTbl_ThisZ( M, unite_layer, tfm_file );

/* ----------------------------- */
/* Set each TForm in given layer */
/* ----------------------------- */

	double	stiff	= 10.0;

	int	nr = vRgn.size();

	for( int i = 0; i < nr; ++i ) {

		const RGN&	R = vRgn[i];

		if( R.z != unite_layer || R.itr < 0 )
			continue;

		map<MZIDR,THmgphy>::iterator	it;

		it = M.find( MZIDR( R.z, R.id, R.rgn ) );

		if( it == M.end() )
			continue;

		double	one	= stiff,
				*t	= it->second.t;
		int		j	= R.itr * NX;

		AddConstraint( LHS, RHS, 1, &j, &one, one*t[0] );		j++;
		AddConstraint( LHS, RHS, 1, &j, &one, one*t[1] );		j++;
		AddConstraint( LHS, RHS, 1, &j, &one, one*t[2] / sc );	j++;
		AddConstraint( LHS, RHS, 1, &j, &one, one*t[3] );		j++;
		AddConstraint( LHS, RHS, 1, &j, &one, one*t[4] );		j++;
		AddConstraint( LHS, RHS, 1, &j, &one, one*t[5] / sc );	j++;
		AddConstraint( LHS, RHS, 1, &j, &one, one*t[6] );		j++;
		AddConstraint( LHS, RHS, 1, &j, &one, one*t[7] );		j++;
	}
}

/* --------------------------------------------------------------- */
/* SolveWithSquareness ------------------------------------------- */
/* --------------------------------------------------------------- */

void MHmgphy::SolveWithSquareness(
	vector<double>	&X,
	vector<LHSCol>	&LHS,
	vector<double>	&RHS,
	int				nTr,
	double			square_strength )
{
/* -------------------------- */
/* Add squareness constraints */
/* -------------------------- */

	double	stiff = square_strength;

	for( int i = 0; i < nTr; ++i ) {

		int	j = i * NX;

		// equal cosines
		{
			double	V[2] = {stiff, -stiff};
			int		I[2] = {j, j+4};

			AddConstraint( LHS, RHS, 2, I, V, 0.0 );
		}

		// opposite sines
		{
			double	V[2] = {stiff, stiff};
			int		I[2] = {j+1, j+3};

			AddConstraint( LHS, RHS, 2, I, V, 0.0 );
		}
	}

/* ----------------- */
/* 1st pass solution */
/* ----------------- */

// We have enough info for first estimate of the global
// transforms. We will need these to formulate further
// constraints on the global shape and scale.

	printf( "Solve with [transform squareness].\n" );
	WriteSolveRead( X, LHS, RHS, false );
	PrintMagnitude( X );
}

/* --------------------------------------------------------------- */
/* SolveWithUnitMag ---------------------------------------------- */
/* --------------------------------------------------------------- */

// Effectively, we want to constrain the cosines and sines
// so that c^2 + s^2 = 1. We can't make constraints that are
// non-linear in the variables X[], but we can construct an
// approximation using the {c,s = X[]} of the previous fit:
// c*x + s*y = 1. To reduce sensitivity to the sizes of the
// previous fit c,s, we normalize them by m = sqrt(c^2 + s^2).
//
void MHmgphy::SolveWithUnitMag(
	vector<double>	&X,
	vector<LHSCol>	&LHS,
	vector<double>	&RHS,
	int				nTR,
	double			scale_strength )
{
	double	stiff = scale_strength;

	for( int i = 0; i < nTR; ++i ) {

		int		j = i * NX;
		double	c = X[j];
		double	s = X[j+3];
		double	m = sqrt( c*c + s*s );

		// c*x/m + s*y/m = 1

		double	V[2] = {c * stiff, s * stiff};
		int		I[2] = {j, j+3};

		AddConstraint( LHS, RHS, 2, I, V, m * stiff );
	}

	printf( "Solve with [unit magnitude].\n" );
	WriteSolveRead( X, LHS, RHS, false );
	printf( "\t\t\t\t" );
	PrintMagnitude( X );
}

/* --------------------------------------------------------------- */
/* RescaleAll ---------------------------------------------------- */
/* --------------------------------------------------------------- */

void MHmgphy::RescaleAll(
	vector<double>	&X,
	double			sc )
{
	int	nr	= vRgn.size();

	for( int i = 0; i < nr; ++i ) {

		int	itr = vRgn[i].itr;

		if( itr < 0 )
			continue;

		itr *= NX;

		X[itr+2] *= sc;
		X[itr+5] *= sc;
	}
}

/* --------------------------------------------------------------- */
/* RotateAll ----------------------------------------------------- */
/* --------------------------------------------------------------- */

void MHmgphy::RotateAll(
	vector<double>	&X,
	double			degcw )
{
	THmgphy	T, R;
	int		nr	= vRgn.size();

	R.SetCWRot( degcw, Point(0,0) );

	for( int i = 0; i < nr; ++i ) {

		int	itr = vRgn[i].itr;

		if( itr < 0 )
			continue;

		itr *= NX;

		THmgphy	t( &X[itr] );

		T = R * t;
		T.CopyOut( &X[itr] );
	}
}

/* --------------------------------------------------------------- */
/* NewOriginAll -------------------------------------------------- */
/* --------------------------------------------------------------- */

void MHmgphy::NewOriginAll(
	vector<double>	&X,
	double			xorg,
	double			yorg )
{
	int	nr	= vRgn.size();

	for( int i = 0; i < nr; ++i ) {

		int	itr = vRgn[i].itr;

		if( itr < 0 )
			continue;

		itr *= NX;

		X[itr+2] -= xorg;
		X[itr+5] -= yorg;
	}
}

/* --------------------------------------------------------------- */
/* WriteSideRatios ----------------------------------------------- */
/* --------------------------------------------------------------- */

// Experiment to guage trapezoidism by reporting the ratio of
// each image's left vertical side over its right side.
//
void MHmgphy::WriteSideRatios(
	const vector<zsort>		&zs,
	const vector<double>	&X )
{
	FILE	*f	= FileOpenOrDie( "SideRatios.txt", "w" );
	int		nr	= vRgn.size();
	MeanStd	M[4];

	for( int i = 0; i < nr; ++i ) {

		const RGN&	I = vRgn[zs[i].i];

		if( I.itr < 0 )
			continue;

		int	j = I.itr * NX;

		THmgphy		T( &X[j] );
		Point		A, B;
		double		d;
		const char	*c, *n = FileNamePtr( I.GetName() );
		int			cam = 0;

		B = Point( 0, 2200 );
		A = Point( 0, 0 );

		T.Transform( A );
		T.Transform( B );
		d = B.Dist( A );

		B = Point( 2200, 2200 );
		A = Point( 2200, 0 );

		T.Transform( A );
		T.Transform( B );
		d /= B.Dist( A );

		if( c = strstr( n, "_cam" ) )
			cam = atoi( c + 4 );

		fprintf( f, "%d\t%g\n", cam, d );

		M[cam].Element( d );
	}

	for( int i = 0; i < 4; ++i ) {

		double	ave, std;

		M[i].Stats( ave, std );

		printf( "{cam,L/R,std}: {%d,%g,%g}\n", i, ave, std );
	}

	fclose( f );
}

/* --------------------------------------------------------------- */
/* WriteTransforms ----------------------------------------------- */
/* --------------------------------------------------------------- */

void MHmgphy::WriteTransforms(
	const vector<zsort>		&zs,
	const vector<double>	&X,
	int						bstrings,
	FILE					*FOUT )
{
	printf( "---- Write transforms ----\n" );

	FILE	*f   = FileOpenOrDie( "THmgphyTable.txt", "w" );
	double	smin = 100.0,
			smax = 0.0,
			smag = 0.0;
	int		nr   = vRgn.size(), nTr = 0;

	for( int i = 0; i < nr; ++i ) {

		const RGN&	I = vRgn[zs[i].i];

		if( I.itr < 0 )
			continue;

		int	j = I.itr * NX;

		++nTr;

		fprintf( f, "%d\t%d\t%d\t%f\t%f\t%f\t%f\t%f\t%f\t%f\t%f\n",
		I.z, I.id, I.rgn,
		X[j  ], X[j+1], X[j+2],
		X[j+3], X[j+4], X[j+5],
		X[j+6], X[j+7] );

		if( !bstrings ) {

			fprintf( FOUT, "THMGPHY %d.%d:%d %f %f %f %f %f %f %f %f\n",
			I.z, I.id, I.rgn,
			X[j  ], X[j+1], X[j+2],
			X[j+3], X[j+4], X[j+5],
			X[j+6], X[j+7] );
		}
		else {
			fprintf( FOUT, "THMGPHY '%s::%d' %f %f %f %f %f %f %f %f\n",
			I.GetName(), I.rgn,
			X[j  ], X[j+1], X[j+2],
			X[j+3], X[j+4], X[j+5],
			X[j+6], X[j+7] );
		}

		double	mag = sqrt( X[j]*X[j+4] - X[j+1]*X[j+3] );

		smag += mag;
		smin  = fmin( smin, mag );
		smax  = fmax( smax, mag );
	}

	fclose( f );

	printf(
	"Average magnitude=%f, min=%f, max=%f, max/min=%f\n\n",
	smag/nTr, smin, smax, smax/smin );

	WriteSideRatios( zs, X );
}

/* --------------------------------------------------------------- */
/* L2GPoint ------------------------------------------------------ */
/* --------------------------------------------------------------- */

void MHmgphy::L2GPoint(
	Point			&p,
	vector<double>	&X,
	int				itr )
{
	THmgphy	T( &X[itr * NX] );
	T.Transform( p );
}


void MHmgphy::L2GPoint(
	vector<Point>	&p,
	vector<double>	&X,
	int				itr )
{
	THmgphy	T( &X[itr * NX] );
	T.Transform( p );
}

