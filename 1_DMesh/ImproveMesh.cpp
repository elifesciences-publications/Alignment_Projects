

#include	"CGBL_dmesh.h"
#include	"ImproveMesh.h"

#include	"Maths.h"
#include	"Correlation.h"

#include	<math.h>


/* --------------------------------------------------------------- */
/* Macros -------------------------------------------------------- */
/* --------------------------------------------------------------- */

/* --------------------------------------------------------------- */
/* Types --------------------------------------------------------- */
/* --------------------------------------------------------------- */

/* --------------------------------------------------------------- */
/* Globals ------------------------------------------------------- */
/* --------------------------------------------------------------- */

/* --------------------------------------------------------------- */
/* Statics ------------------------------------------------------- */
/* --------------------------------------------------------------- */






/* --------------------------------------------------------------- */
/* BarycentricMatrices ------------------------------------------- */
/* --------------------------------------------------------------- */

// Barycentric coordinates
// -----------------------
// We can find a set of multipliers {m1, m2, m3) of triangle
// vertices {V1, V2, V3} such that any point P in the plane
// can be represented as m1V1 + m2V2 + m3V3. With the addition
// of normalization constraint m1 + m2 + m3 = 1, we can write
// this as:
//
//	Px   | v1x v2x v3x |   m1
//	Py = | v1y v2y v3y | * m2
//	1    |  1   1   1  |   m3
//
// The inverse of the |V| matrix depicted above converts real
// points into their multipliers. The conversion matricies are
// assigned to the tri[] entries here.
//
static void BarycentricMatrices(
	vector<triangle>		&tri,
	const vector<vertex>	&ctl,
	FILE*					flog )
{
	int	ntri = tri.size();

	for( int k = 0; k < ntri; ++k ) {

		triangle&	T  = tri[k];
		vertex		v0 = ctl[T.v[0]],
					v1 = ctl[T.v[1]],
					v2 = ctl[T.v[2]];
		double		a[3][3];

		fprintf( flog,
		"Tri: (%d %d) (%d %d) (%d %d).\n",
		v0.x, v0.y, v1.x, v1.y, v2.x, v2.y );

		a[0][0] = v0.x; a[0][1] = v1.x; a[0][2] = v2.x;
		a[1][0] = v0.y; a[1][1] = v1.y; a[1][2] = v2.y;
		a[2][0] =  1.0; a[2][1] =  1.0; a[2][2] =  1.0;

		Invert3x3Matrix( T.a, a );
	}
}

/* --------------------------------------------------------------- */
/* ListVerticesMatlab -------------------------------------------- */
/* --------------------------------------------------------------- */

static void ListVerticesMatlab(
	const vector<triangle>	&tri,
	const vector<vertex>	&vrt,
	const char*				desc,
	FILE*					flog )
{
	fprintf( flog, "\n---- Matlab %s ----\n", desc );

	int	ntri = tri.size();

	for( int k = 0; k < ntri; ++k ) {

		const triangle&	T  = tri[k];
		vertex			v0 = vrt[T.v[0]],
						v1 = vrt[T.v[1]],
						v2 = vrt[T.v[2]];

		fprintf( flog,
		"x=[%d %d %d %d]; y=[%d %d %d %d]; plot(x,y); hold on;\n",
		v0.x, v1.x, v2.x, v0.x, v0.y, v1.y, v2.y, v0.y );
	}
}

/* --------------------------------------------------------------- */
/* PointsToMultipliers ------------------------------------------- */
/* --------------------------------------------------------------- */

// For each point:
// (1) pick a best triangle
// (2) get the control point multipliers for the point
//
// IMPORTANT:
// The usual expectation is that there would be exactly three
// multipliers per point (assuming the triangle is known). But
// in this code we carry as many multipliers as control points
// and set them all zero except the relevant three. This is done
// so that each point is expressed as a function of all control
// points, and we can thereby calculate changes in correlation
// as a function of changes in control points (mesh distortion).
//
static void PointsToMultipliers(
	vector<vector<double> >	&am,
	const vector<triangle>	&tri,
	const vector<vertex>	&ctl,
	const vector<Point>		&apts )
{
	int	npts = apts.size(),
		nctl = ctl.size();

	am.resize( npts );

	for( int i = 0; i < npts; ++i ) {

		Point			ap	= apts[i];
		int				t	= BestTriangle( tri, ctl, ap );
		const triangle&	T = tri[t];
		double			m[3];

		m[0] = T.a[0][0]*ap.x + T.a[0][1]*ap.y + T.a[0][2];
		m[1] = T.a[1][0]*ap.x + T.a[1][1]*ap.y + T.a[1][2];
		m[2] = T.a[2][0]*ap.x + T.a[2][1]*ap.y + T.a[2][2];

		// all multipliers zero...
		vector<double>	mlong( nctl, 0.0 );

		// ...except these three
		for( int j = 0; j < 3; ++j )
			mlong[T.v[j]] = m[j];

		am[i] = mlong;
	}
}

/* --------------------------------------------------------------- */
/* ListPointsMatlab ---------------------------------------------- */
/* --------------------------------------------------------------- */

static void ListPointsMatlab(
	const vector<triangle>	&tri,
	const vector<Point>		&pts,
	const char*				desc,
	FILE*					flog )
{
	fprintf( flog, "\n---- Matlab %s ----\n", desc );

	int	ntri = tri.size();

	for( int k = 0; k < ntri; ++k ) {

		const triangle&	T  = tri[k];
		Point			v0 = pts[T.v[0]],
						v1 = pts[T.v[1]],
						v2 = pts[T.v[2]];

		fprintf( flog,
		"x=[%f %f %f %f]; y=[%f %f %f %f]; plot(x,y); hold on;\n",
		v0.x, v1.x, v2.x, v0.x, v0.y, v1.y, v2.y, v0.y );
	}
}

/* --------------------------------------------------------------- */
/* ReportDeltaXY ------------------------------------------------- */
/* --------------------------------------------------------------- */

static void ReportDeltaXY(
	const vector<Point>	&cpts,
	const vector<Point>	&bfor,
	FILE*				flog )
{
	fprintf( flog, "\n---- Deltas ----\n" );

	int	nctl = cpts.size();

	for( int i = 0; i < nctl; ++i ) {

		double	d = cpts[i].Dist( bfor[i] );

		fprintf( flog,
		"id=%6d: dx=%8.2f dy=%8.2f d=%8.2f.\n",
		i, cpts[i].x - bfor[i].x, cpts[i].y - bfor[i].y, d );
	}
}

/* --------------------------------------------------------------- */
/* CheckAreas ---------------------------------------------------- */
/* --------------------------------------------------------------- */

// Return true if overall area changes are within tolerances.
//
static bool CheckAreas(
	const vector<triangle>	&tri,
	const vector<Point>		&orig,
	const vector<Point>		&cpts,
	FILE*					flog )
{
	fprintf( flog, "\n---- Areas ----\n" );

/* --------------------------- */
/* Get cumulative area changes */
/* --------------------------- */

	double	max_pct		= 0.0,
			sum_A0		= 0.0,
			sum_Anew	= 0.0;
	int		ntri		= tri.size();

	for( int k = 0; k < ntri; ++k ) {

		double	A0 = tri[k].Area( orig );

		if( GBL.A.layer != GBL.B.layer ) {

			double	f = GBL.ctx.scale
						* GBL.ctx.scale
						* GBL.ctx.xscale
						* GBL.ctx.yscale;

			if( f < 0.99 || f > 1.01 ) {

				fprintf( flog,
				"Modifying old area from %f to %f because scale"
				" change was specified.\n", A0, A0 * f );

				A0 *= f;
			}
		}

		sum_A0 += A0;

		double	Anew	= tri[k].Area( cpts );
		double	pct		= (Anew - A0) / A0 * 100.0;

		sum_Anew += Anew;

		fprintf( flog,
		"Triangle %d, area was %10.1f, is %10.1f, %6.1f%%\n",
		k, A0, Anew, pct );

		max_pct = fmax( max_pct, fabs( pct ) );
	}

	double sum_pct = (sum_Anew - sum_A0) / sum_A0 * 100.0;

	fprintf( flog,
	"Combined: area was %10.1f, is %10.1f, %6.1f%%\n",
	sum_A0, sum_Anew, sum_pct );

/* ------------------ */
/* Assess area change */
/* ------------------ */

	double	sum_lim = GBL.msh.TSC;

	if( sum_Anew > 500000 )
		sum_lim *= 4.0/3.0;

	if( max_pct > GBL.msh.TMC || fabs( sum_pct ) > sum_lim ) {

		fprintf( flog,
		"FAIL: Area change too big"
		" (max, sum) = (%8.2f%% %8.2f%%), (TMC TSC)=(%f %f).\n",
		max_pct, sum_pct, GBL.msh.TMC, sum_lim );

		return false;
	}

	return true;
}

/* --------------------------------------------------------------- */
/* TransformsAndCenters ------------------------------------------ */
/* --------------------------------------------------------------- */

static void TransformsAndCenters(
	vector<TForm>			&transforms,
	vector<Point>			&centers,
	const vector<triangle>	&tri,
	const vector<Point>		&orig,
	const vector<Point>		&cpts,
	const TForm				&Tguess,
	FILE*					flog )
{
	fprintf( flog, "\n---- Transforms ----\n" );

	int	ntri = tri.size();

	for( int k = 0; k < ntri; ++k ) {

		const triangle&	T  = tri[k];
		int				i0 = T.v[0],
						i1 = T.v[1],
						i2 = T.v[2];

		// Find transformation that maps original control points
		// (orig) into optimized (cpts).
		//
		// Begin with a transform mapping a unit right triangle
		// { (0,0), (1,0), (0,1) } in abstract global space to
		// the respective orig vertices { o0, o1, o2 }. To see
		// how simple this really is, just apply the TForm (o)
		// that we define below to each of the global vertices.

		const Point&	o0 = orig[i0],
						o1 = orig[i1],
						o2 = orig[i2];

		TForm	o(	o1.x - o0.x, o2.x - o0.x, o0.x,
					o1.y - o0.y, o2.y - o0.y, o0.y );

		// And make a like mapping from global space to (cpts)

		const Point&	c0 = cpts[i0],
						c1 = cpts[i1],
						c2 = cpts[i2];

		TForm	c(	c1.x - c0.x, c2.x - c0.x, c0.x,
					c1.y - c0.y, c2.y - c0.y, c0.y );

		// Now make transform t = c * o-inv from orig to cpts

		TForm	t, oi;

		InvertTrans( oi, o );
		MultiplyTrans( t, c, oi );

		t.PrintTransform( flog );

		// Sanity check the "angular" change

		if(	(fabs( t.t[0] - 1.0 ) > 0.1 &&
			 fabs( t.t[0] - Tguess.t[0] ) > 0.1)
			||
			(fabs( t.t[4] - 1.0 ) > 0.1 &&
			 fabs( t.t[4] - Tguess.t[4] ) > 0.1) ) {

			fprintf( flog,
			"Large deviation in t[0], t[4]: vertices %d %d %d.\n",
			i0, i1, i2 );

			fprintf( flog,
			"orig (%f %f) (%f %f) (%f %f).\n",
			o0.x, o0.y, o1.x, o1.y, o2.x, o2.y );

			fprintf( flog,
			"cpts (%f %f) (%f %f) (%f %f)\n",
			c0.x, c0.y, c1.x, c1.y, c2.x, c2.y );
		}

		transforms.push_back( t );

		centers.push_back(
			Point(	(o0.x + o1.x + o2.x) / 3.0,
					(o0.y + o1.y + o2.y) / 3.0 ) );
	}
}

/* --------------------------------------------------------------- */
/* ImproveMesh --------------------------------------------------- */
/* --------------------------------------------------------------- */

// transforms	- output transforms
// centers		- output tri centroids
// tri			- source tris (their a-matrices are set here)
// ctl			- source control points
// apts, av		- source image
// bimg			- B-raster mapped to
// w, h			- B-raster dims
// tr_guess		- starting guess
// threshold	- required correlation
// describe		- descriptive string for logs
//
double ImproveMesh(
	vector<TForm>			&transforms,
	vector<Point>			&centers,
	vector<triangle>		&tri,
	const vector<vertex>	&ctl,
	const vector<Point>		&apts,
	const vector<double>	&av,
	const vector<double>	&bimg,
	int						w,
	int						h,
	const TForm				&tr_guess,
	double					threshold,
	FILE					*flog,
	const char				*describe )
{
	fprintf( flog, "\n---- ImproveMesh - %s ----\n", describe );

/* --------------- */
/* Init transforms */
/* --------------- */

// As a convenience, we allow the caller to set 'tr_guess'
// to some element of transforms[], but we need to zero
// transforms[] here. Therefore, we save copy 'Tguess.'

	TForm	Tguess = tr_guess;

	transforms.clear();
	centers.clear();

/* --------------------------------- */
/* Make point-to-multiplier matrices */
/* --------------------------------- */

	BarycentricMatrices( tri, ctl, flog );

/* --------------------------------- */
/* Report triangles in Matlab format */
/* --------------------------------- */

	ListVerticesMatlab( tri, ctl, "Vertices", flog );

/* --------------------- */
/* Points to multipliers */
/* --------------------- */

	vector<vector<double> >	am;

	PointsToMultipliers( am, tri, ctl, apts );

/* -------------------- */
/* Init change tracking */
/* -------------------- */

// We want three copies of the control points to track deltas:
// (1) orig: are the unmodified points
// (2) bfor: are transformed to B-coords but not optimized
// (3) cpts: are the optimized points

	int				nctl = ctl.size();
	vector<Point>	orig( nctl ), bfor, cpts;

	for( int k = 0; k < nctl; ++k )
		orig[k] = Point( ctl[k].x, ctl[k].y );

	bfor = orig;
	tr_guess.Transform( bfor );

	cpts = bfor;

/* ------------- */
/* Optimize mesh */
/* ------------- */

	fprintf( flog, "\n---- ImproveControlPts ----\n" );

// On entry, corr temporarily holds the desired final
// threshold. On exit, corr is the mesh correlation.
//
// A negative value for the threshold skips mesh optimization.
// This is useful for same-layer alignments with sparse optical
// data. An observed pathology is the case of an extended soma
// with a bright core that occupies most of a single mesh triangle.
// The optimizer tends to shrink that triangle so the whole soma
// in A overlays its core in B, giving an artificially high corr.
// This is still a danger for cross-layer, but in same-layer there
// really shouldn't be any deformation.
//
// For homogeneous EM case, mesh optimization works well and
// is always recommended.

	double	corr = threshold;

	if( !GBL.msh.DSL && (GBL.A.layer == GBL.B.layer) )
		corr = -1;

	corr = ImproveControlPts(
				cpts, am, av,
				bimg, w, h,
				flog, describe,
				GBL.msh.DIT, corr );

	if( corr < threshold ) {

		fprintf( flog,
		"FAIL: ImproveMesh: corr=%f, below final thresh=%f.\n",
		corr, threshold );

		return corr;
	}

/* -------------- */
/* Report results */
/* -------------- */

	ListPointsMatlab( tri, orig, "A-Sys Originals", flog );
	ListPointsMatlab( tri, bfor, "B-Sys Originals", flog );
	ListPointsMatlab( tri, cpts, "B-Sys Optimized", flog );

	ReportDeltaXY( cpts, bfor, flog );

/* ----------- */
/* Check areas */
/* ----------- */

	if( !CheckAreas( tri, orig, cpts, flog ) )
		return 0.0;

/* ------------------------------------------ */
/* Get transform and center for each triangle */
/* ------------------------------------------ */

	TransformsAndCenters( transforms, centers,
		tri, orig, cpts, Tguess, flog );

	return corr;
}


