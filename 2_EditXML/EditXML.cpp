//
// New TrakEM2 file with:
//
// channel -chn
// type=1
// min=200
// max=1000
//
// Accepts layer range [zmin, zmax].
//

#include	"Cmdline.h"
#include	"File.h"
#include	"TrakEM2_UTL.h"


/* --------------------------------------------------------------- */
/* Macros -------------------------------------------------------- */
/* --------------------------------------------------------------- */

/* --------------------------------------------------------------- */
/* Types --------------------------------------------------------- */
/* --------------------------------------------------------------- */

/* --------------------------------------------------------------- */
/* CArgs_rgbm ---------------------------------------------------- */
/* --------------------------------------------------------------- */

class CArgs_rgbm {

public:
    char	*infile;
    int		zmin, zmax;
    int		chn;

public:
    CArgs_rgbm()
    {
        infile	= NULL;
        zmin	= 0;
        zmax	= 32768;
        chn		= 0;
    };

    void SetCmdLine( int argc, char* argv[] );
};

/* --------------------------------------------------------------- */
/* Statics ------------------------------------------------------- */
/* --------------------------------------------------------------- */

static CArgs_rgbm	gArgs;
static FILE*		flog = NULL;






/* --------------------------------------------------------------- */
/* SetCmdLine ---------------------------------------------------- */
/* --------------------------------------------------------------- */

void CArgs_rgbm::SetCmdLine( int argc, char* argv[] )
{
// start log

    flog = FileOpenOrDie( "EditXML.log", "w" );

// log start time

    time_t	t0 = time( NULL );
    char	atime[32];

    strcpy( atime, ctime( &t0 ) );
    atime[24] = '\0';	// remove the newline

    fprintf( flog, "Start: %s ", atime );

// parse command line args

    if( argc < 2 ) {
        printf( "Usage: EditXML <xml-file> [options].\n" );
        exit( 42 );
    }

    for( int i = 1; i < argc; ++i ) {

        // echo to log
        fprintf( flog, "%s ", argv[i] );

        if( argv[i][0] != '-' )
            infile = argv[i];
        else if( GetArg( &zmin, "-zmin=%d", argv[i] ) )
            ;
        else if( GetArg( &zmax, "-zmax=%d", argv[i] ) )
            ;
        else if( GetArg( &chn, "-chn=%d", argv[i] ) )
            ;
        else {
            printf( "Did not understand option '%s'.\n", argv[i] );
            exit( 42 );
        }
    }

    fprintf( flog, "\n\n" );
    fflush( flog );
}

/* --------------------------------------------------------------- */
/* EditPath ------------------------------------------------------ */
/* --------------------------------------------------------------- */

static void EditPath( TiXmlElement* ptch )
{
    char	buf[2048];
    int		len;

    len = sprintf( buf, "%s", ptch->Attribute( "file_path" ) );
    buf[len - 5] = '0' + gArgs.chn;
    ptch->SetAttribute( "file_path", buf );
}

/* --------------------------------------------------------------- */
/* UpdateTiles --------------------------------------------------- */
/* --------------------------------------------------------------- */

static void UpdateTiles( TiXmlElement* layer )
{
    TiXmlElement*	ptch = layer->FirstChildElement( "t2_patch" );

    for( ; ptch; ptch = ptch->NextSiblingElement() ) {

        EditPath( ptch );
        ptch->SetAttribute( "type", 1 );
        ptch->SetAttribute( "min", 200 );
        ptch->SetAttribute( "max", 1000 );
    }
}

/* --------------------------------------------------------------- */
/* WriteXML ------------------------------------------------------ */
/* --------------------------------------------------------------- */

static void WriteXML()
{
/* ---- */
/* Open */
/* ---- */

    XML_TKEM		xml( gArgs.infile, flog );
    TiXmlElement*	layer	= xml.GetFirstLayer();

/* ---------- */
/* Fix layers */
/* ---------- */

    for( ; layer; layer = layer->NextSiblingElement() ) {

        int	z = atoi( layer->Attribute( "z" ) );

        if( z > gArgs.zmax )
            break;

        if( z < gArgs.zmin )
            continue;

        UpdateTiles( layer );
    }

/* ---- */
/* Save */
/* ---- */

    xml.Save( "xmltmp.txt", true );
}

/* --------------------------------------------------------------- */
/* main ---------------------------------------------------------- */
/* --------------------------------------------------------------- */

int main( int argc, char* argv[] )
{
/* ------------------ */
/* Parse command line */
/* ------------------ */

    gArgs.SetCmdLine( argc, argv );

/* ------------- */
/* Write new xml */
/* ------------- */

    WriteXML();

/* ---- */
/* Done */
/* ---- */

    fprintf( flog, "\n" );
    fclose( flog );

    return 0;
}


