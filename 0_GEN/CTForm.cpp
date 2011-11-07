

#include	"GenDefs.h"
#include	"CTForm.h"

#include	<math.h>






/* --------------------------------------------------------------- */
/* Discussion ---------------------------------------------------- */
/* --------------------------------------------------------------- */

/*
	Coordinates
	-----------
	     ^+Z
	     |
	     |
	     /--->+X   +theta rotates +X toward +Y (clockwise)
	  +Y/

	Individual images lie in the XY-plane, but unlike usual math
	conventions the Y-axis is negated, so increases 'downward'.

	Z-coordinates usually start at zero and increase upward. The
	standard perspective is looking down on a stack.

	Because the Y-axis is negated, positive angles cause a CW
	rather than a CCW rotation.

	Some portions of this code body refer to boxes or bounds or
	bounding boxes wherein the sides are {L,R,B,T}. X increases
	from left to right in the usual way. Y increases from bottom
	to top, but again, the bottom of a box appears higher on a
	computer screen because Y runs downward.

	2D transformations map a region (such as a whole image) from
	local region coords to an abstract global coordinate space.

	In our code, the 6 affine tranform elements are interpreted
	as follows:

		x'   t2   |t0  t1|   x
		   =    + |      | *
		y'   t5   |t3  t4|   y

	In Matlab, our 6 elements are filled by column into one array
	as follows:

		{t0, t3, t1, t4, t2, t5}.

	In ImageJ (including TrakEM2) the elements in source code
	are the following:

		x'   m02  |m00  m01|   x
		   =    + |        | *
		y'   m12  |m10  m11|   y

	In a TrackEM2 XML file transform attributes are written
	"matrix(m00,m10,m01,m11,m02,m12)".
*/

/* --------------------------------------------------------------- */
/* Transform ----------------------------------------------------- */
/* --------------------------------------------------------------- */

void TForm::Transform( Point &p ) const
{
	double	x = p.x*t[0] + p.y*t[1] + t[2];
	double	y = p.x*t[3] + p.y*t[4] + t[5];

	p.x = x;
	p.y = y;
}


void TForm::Transform( vector<Point> &v ) const
{
	int		N = v.size();

	for( int i = 0; i < N; ++i )
		Transform( v[i] );
}

/* --------------------------------------------------------------- */
/* Apply_R_Part -------------------------------------------------- */
/* --------------------------------------------------------------- */

// If transform T(p) is represented as R(p) + V,
// then here we apply R(p) without V.
//
void TForm::Apply_R_Part( Point &p ) const
{
	double	x = p.x*t[0] + p.y*t[1];
	double	y = p.x*t[3] + p.y*t[4];

	p.x = x;
	p.y = y;
}


void TForm::Apply_R_Part( vector<Point> &v ) const
{
	int		N = v.size();

	for( int i = 0; i < N; ++i )
		Apply_R_Part( v[i] );
}

/* --------------------------------------------------------------- */
/* RotateAround -------------------------------------------------- */
/* --------------------------------------------------------------- */

// Fiddle with the rotation, but still map point 's' to point 'tar'
//
void TForm::RotateAround( Point s, Point tar, double rad )
{
	double	co = cos( rad ), si = sin( rad );
	double	a = t[0], b = t[1], c = t[3], d = t[4];

	t[0] = a*co    + b*si;
	t[1] = a*(-si) + b*co;
	t[3] = c*co    + d*si;
	t[4] = c*(-si) + d*co;

// now make point s come out where it originally did
	Point test( s.x, s.y );
	Transform( test );
	AddXY( tar.x - test.x, tar.y - test.y );
}

/* --------------------------------------------------------------- */
/* ScanTrackEM2 -------------------------------------------------- */
/* --------------------------------------------------------------- */

void TForm::ScanTrackEM2( const char *s )
{
	sscanf( s, "matrix(%lf,%lf,%lf,%lf,%lf,%lf",
		&t[0], &t[3], &t[1], &t[4], &t[2], &t[5] );
}

/* --------------------------------------------------------------- */
/* WriteTransform ------------------------------------------------ */
/* --------------------------------------------------------------- */

void TForm::WriteTransform( FILE *f, const char *s ) const
{
	fprintf( f,
	"%s %9.4f %9.4f %10.2f\n"
	"%s %9.4f %9.4f %10.2f.\n",
	s, t[0], t[1], t[2],
	s, t[3], t[4], t[5] );
}

/* --------------------------------------------------------------- */
/* PrintTransform ------------------------------------------------ */
/* --------------------------------------------------------------- */

void TForm::PrintTransform( FILE *f ) const
{
	if( !f )
		f = stdout;

	fprintf( f, "%7.4f %7.4f %8.2f   %7.4f %7.4f %8.2f.\n",
		t[0], t[1], t[2], t[3], t[4], t[5] );
}

/* --------------------------------------------------------------- */
/* PrintTransformAsParam ----------------------------------------- */
/* --------------------------------------------------------------- */

void TForm::PrintTransformAsParam( FILE *f, bool newline ) const
{
	fprintf( f, " -TRA=%.4f,%.4f,%.2f,%.4f,%.4f,%.2f%c",
		t[0], t[1], t[2], t[3], t[4], t[5],
		(newline ? '\n' : ' ') );
}

/* --------------------------------------------------------------- */
/* InvertTrans --------------------------------------------------- */
/* --------------------------------------------------------------- */

void InvertTrans( TForm &inv, const TForm &t )
{
	double	det = t.t[0]*t.t[4] - t.t[1]*t.t[3];

	inv.t[0] =  t.t[4]/det;
	inv.t[1] = -t.t[1]/det;
	inv.t[2] = (t.t[5]*t.t[1]-t.t[4]*t.t[2])/det;
	inv.t[3] = -t.t[3]/det;
	inv.t[4] =  t.t[0]/det;
	inv.t[5] = (t.t[2]*t.t[3]-t.t[0]*t.t[5])/det;
}

/* --------------------------------------------------------------- */
/* MultiplyTrans ------------------------------------------------- */
/* --------------------------------------------------------------- */

// Compute r = a*b
//
void MultiplyTrans( TForm &r, const TForm &a, const TForm &b )
{
	r.t[0] = a.t[0]*b.t[0]+a.t[1]*b.t[3];
	r.t[1] = a.t[0]*b.t[1]+a.t[1]*b.t[4];
	r.t[2] = a.t[0]*b.t[2]+a.t[1]*b.t[5]+a.t[2];
	r.t[3] = a.t[3]*b.t[0]+a.t[4]*b.t[3];
	r.t[4] = a.t[3]*b.t[1]+a.t[4]*b.t[4];
	r.t[5] = a.t[3]*b.t[2]+a.t[4]*b.t[5]+a.t[5];
}

/* --------------------------------------------------------------- */
/* AToBTrans ----------------------------------------------------- */
/* --------------------------------------------------------------- */

// Compute TForm [atob = binv*a] from global TForms a, b.
//
void AToBTrans( TForm &atob, const TForm &a, const TForm &b )
{
	TForm	binv;

	InvertTrans( binv, b );
	MultiplyTrans( atob, binv, a );
}

/* --------------------------------------------------------------- */
/* CreateCWRotation ---------------------------------------------- */
/* --------------------------------------------------------------- */

// Create a transform that performs a CW rotation of deg degrees
// about the given pivot point (remember +deg is CW).
//
// That is, A' = piv + R(A-piv) = R(A) + piv - R(piv).
//
void CreateCWRot( TForm &T, double deg, const Point &pivot )
{
	double	r = deg*PI/180, c = cos( r ), s = sin( r );

	T.t[0] = c;
	T.t[1] = -s;
	T.t[2] = pivot.x - (c*pivot.x - s*pivot.y);
	T.t[3] = s;
	T.t[4] = c;
	T.t[5] = pivot.y - (s*pivot.x + c*pivot.y);
}

/* --------------------------------------------------------------- */
/* RadiansFromAffine --------------------------------------------- */
/* --------------------------------------------------------------- */

// A general affine may include (ignoring reflection here),
// rotation, skew and scale. Using polar decomposition we
// attempt to extract the (roughly) pure rotation matrix R
// from general affine A and return its associated angle
// in radians.
//
// Method (adapted from "Graphics Gems IV", section III.4,
// by Ken Shoemake).
//
// From TForm's matrix-part P = R( a ), iteratively compute
// N = (P + Inv(Trp(P)) / 2, until max element-wise change
// in (N - P) is tiny.
//
double RadiansFromAffine( const TForm &a )
{
	double	P[4] = {a.t[0],a.t[1],a.t[3],a.t[4]};

	for( int iter = 0; iter < 100; ++iter ) {

		double	N[4], d, t;

		// N = P-transpose
		N[0] = P[0]; N[1] = P[2]; N[2] = P[1]; N[3] = P[3];

		// N = N-inverse;
		d = N[0]*N[3] - N[1]*N[2];
		t = N[0];
		N[0] =  N[3] / d;
		N[1] = -N[1] / d;
		N[2] = -N[2] / d;
		N[3] =  N[0] / d;

		// N = (P + N)/2
		N[0] = (P[0] + N[0]) / 2.0;
		N[1] = (P[1] + N[1]) / 2.0;
		N[2] = (P[2] + N[2]) / 2.0;
		N[3] = (P[3] + N[3]) / 2.0;

		// max( N - P )
		d = fabs( N[0] - P[0] );
		d = fmax( d, fabs( N[1] - P[1] ) );
		d = fmax( d, fabs( N[2] - P[2] ) );
		d = fmax( d, fabs( N[3] - P[3] ) );

		// converged?
		if( d < 1e-7 ) {

			//printf( "i=%d, %f  %f  %f  %f\n",
			//iter, N[0], N[1], N[2], N[3] );

			return atan2( N[2], N[0] );
		}

		// P = N;
		P[0] = N[0]; P[1] = N[1]; P[2] = N[2]; P[3] = N[3];
	}

// fall back on standard crude estimator

	return atan2( a.t[3], a.t[0] );
}


