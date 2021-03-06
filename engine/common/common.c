/*
common.c - misc functions used by dlls'
Copyright (C) 2008 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "common.h"
#include "studio.h"
#include "mathlib.h"
#include "const.h"
#include "client.h"
#include "library.h"

static long idum = 0;

#define MAX_RANDOM_RANGE	0x7FFFFFFFUL
#define IA		16807
#define IM		2147483647
#define IQ		127773
#define IR		2836
#define NTAB		32
#define EPS		1.2e-7
#define NDIV		(1 + (IM - 1) / NTAB)
#define AM		(1.0 / IM)
#define RNMX		(1.0 - EPS)

static long lran1( void )
{
	static long	iy = 0;
	static long	iv[NTAB];
	int		j;
	long		k;

	if( idum <= 0 || !iy )
	{
		if( -(idum) < 1 ) idum = 1;
		else idum = -(idum);

		for( j = NTAB + 7; j >= 0; j-- )
		{
			k = (idum) / IQ;
			idum = IA * (idum - k * IQ) - IR * k;
			if( idum < 0 ) idum += IM;
			if( j < NTAB ) iv[j] = idum;
		}

		iy = iv[0];
	}

	k = (idum) / IQ;
	idum = IA * (idum - k * IQ) - IR * k;
	if( idum < 0 ) idum += IM;
	j = iy / NDIV;
	iy = iv[j];
	iv[j] = idum;

	return iy;
}

// fran1 -- return a random floating-point number on the interval [0,1]
static float fran1( void )
{
	float temp = (float)AM * lran1();
	if( temp > RNMX )
		return (float)RNMX;
	return temp;
}

void COM_SetRandomSeed( long lSeed )
{
	if( lSeed ) idum = lSeed;
	else idum = -time( NULL );

	if( 1000 < idum )
		idum = -idum;
	else if( -1000 < idum )
		idum -= 22261048;
}

float COM_RandomFloat( float flLow, float flHigh )
{
	float	fl;

	if( idum == 0 ) COM_SetRandomSeed( 0 );

	fl = fran1(); // float in [0,1]
	return (fl * (flHigh - flLow)) + flLow; // float in [low, high)
}

long COM_RandomLong( long lLow, long lHigh )
{
	dword	maxAcceptable;
	dword	n, x = lHigh - lLow + 1; 	

	if( idum == 0 ) COM_SetRandomSeed( 0 );

	if( x <= 0 || MAX_RANDOM_RANGE < x - 1 )
		return lLow;

	// The following maps a uniform distribution on the interval [0, MAX_RANDOM_RANGE]
	// to a smaller, client-specified range of [0,x-1] in a way that doesn't bias
	// the uniform distribution unfavorably. Even for a worst case x, the loop is
	// guaranteed to be taken no more than half the time, so for that worst case x,
	// the average number of times through the loop is 2. For cases where x is
	// much smaller than MAX_RANDOM_RANGE, the average number of times through the
	// loop is very close to 1.
	maxAcceptable = MAX_RANDOM_RANGE - ((MAX_RANDOM_RANGE + 1) % x );
	do
	{
		n = lran1();
	} while( n > maxAcceptable );

	return lLow + (n % x);
}

/*
==============
COM_IsSingleChar

interpert this character as single
==============
*/
static int COM_IsSingleChar( char c )
{
	if( c == '{' || c == '}' || c == ')' || c == '(' || c == '\'' || c == ',' )
		return true;

	if( host.com_handlecolon && c == ':' )
		return true;

	return false;
}

/*
==============
COM_IsWhiteSpace

interpret symbol as whitespace
==============
*/

static int COM_IsWhiteSpace( char space )
{
	if( space == ' ' || space == '\t' || space == '\r' || space == '\n' )
		return 1;
	return 0;
}

/*
==============
COM_ParseFile

text parser
==============
*/
char *COM_ParseFile( char *data, char *token )
{
	int	c, len;

	if( !token )
		return NULL;
	
	len = 0;
	token[0] = 0;
	
	if( !data )
		return NULL;
// skip whitespace
skipwhite:
	while(( c = ((byte)*data)) <= ' ' )
	{
		if( c == 0 )
			return NULL;	// end of file;
		data++;
	}
	
	// skip // comments
	if( c=='/' && data[1] == '/' )
	{
		while( *data && *data != '\n' )
			data++;
		goto skipwhite;
	}

	// handle quoted strings specially
	if( c == '\"' )
	{
		data++;
		while( 1 )
		{
			c = (byte)*data++;
			if( c == '\"' || !c )
			{
				token[len] = 0;
				return data;
			}
			token[len] = c;
			len++;
		}
	}

	// parse single characters
	if( COM_IsSingleChar( c ))
	{
		token[len] = c;
		len++;
		token[len] = 0;
		return data + 1;
	}

	// parse a regular word
	do
	{
		token[len] = c;
		data++;
		len++;
		c = ((byte)*data);

		if( COM_IsSingleChar( c ))
			break;
	} while( c > 32 );
	
	token[len] = 0;

	return data;
}

/*
================
COM_ParseVector

================
*/
qboolean COM_ParseVector( char **pfile, float *v, size_t size )
{
	string	token;
	qboolean	bracket = false;
	char	*saved;
	uint	i;

	if( v == NULL || size == 0 )
		return false;

	memset( v, 0, sizeof( *v ) * size );

	if( size == 1 )
	{
		*pfile = COM_ParseFile( *pfile, token );
		v[0] = Q_atof( token );
		return true;
	}

	saved = *pfile;

	if(( *pfile = COM_ParseFile( *pfile, token )) == NULL )
		return false;

	if( token[0] == '(' )
		bracket = true;
	else *pfile = saved; // restore token to right get it again

	for( i = 0; i < size; i++ )
	{
		*pfile = COM_ParseFile( *pfile, token );
		v[i] = Q_atof( token );
	}

	if( !bracket ) return true;	// done

	if(( *pfile = COM_ParseFile( *pfile, token )) == NULL )
		return false;

	if( token[0] == ')' )
		return true;
	return false;
}

/*
=============
COM_FileSize

=============
*/
int COM_FileSize( const char *filename )
{
	return FS_FileSize( filename, false );
}

/*
=============
COM_AddAppDirectoryToSearchPath

=============
*/
void COM_AddAppDirectoryToSearchPath( const char *pszBaseDir, const char *appName )
{
	string	dir;

	if( !pszBaseDir || !appName )
	{
		MsgDev( D_ERROR, "COM_AddDirectorySearchPath: bad directory or appname\n" );
		return;
	}

	Q_snprintf( dir, sizeof( dir ), "%s/%s", pszBaseDir, appName );
	FS_AddGameDirectory( dir, FS_GAMEDIR_PATH );
}

/*
===========
COM_ExpandFilename

Finds the file in the search path, copies over the name with the full path name.
This doesn't search in the pak file.
===========
*/
int COM_ExpandFilename( const char *fileName, char *nameOutBuffer, int nameOutBufferSize )
{
	const char	*path;
	char		result[MAX_SYSPATH];

	if( !fileName || !*fileName || !nameOutBuffer || nameOutBufferSize <= 0 )
		return 0;

	// filename examples:
	// media\sierra.avi - D:\Xash3D\valve\media\sierra.avi
	// models\barney.mdl - D:\Xash3D\bshift\models\barney.mdl
	if(( path = FS_GetDiskPath( fileName, false )) != NULL )
	{
		Q_sprintf( result, "%s/%s", host.rootdir, path );		

		// check for enough room
		if( Q_strlen( result ) > nameOutBufferSize )
			return 0;

		Q_strncpy( nameOutBuffer, result, nameOutBufferSize );
		return 1;
	}
	return 0;
}

/*
=============
COM_TrimSpace

trims all whitespace from the front
and end of a string
=============
*/
void COM_TrimSpace( const char *source, char *dest )
{
	int	start, end, length;

	start = 0;
	end = Q_strlen( source );

	while( source[start] && COM_IsWhiteSpace( source[start] ))
		start++;
	end--;

	while( end > 0 && COM_IsWhiteSpace( source[end] ))
		end--;
	end++;

	length = end - start;

	if( length > 0 )
		memcpy( dest, source + start, length );
	else length = 0;

	// terminate the dest string
	dest[length] = 0;
}

/*
============
COM_FixSlashes

Changes all '/' characters into '\' characters, in place.
============
*/
void COM_FixSlashes( char *pname )
{
	while( *pname )
	{
		if( *pname == '\\' )
			*pname = '/';
		pname++;
	}
}

/*
=============
COM_MemFgets

=============
*/
char *COM_MemFgets( byte *pMemFile, int fileSize, int *filePos, char *pBuffer, int bufferSize )
{
	int	i, last, stop;

	if( !pMemFile || !pBuffer || !filePos )
		return NULL;

	if( *filePos >= fileSize )
		return NULL;

	i = *filePos;
	last = fileSize;

	// fgets always NULL terminates, so only read bufferSize-1 characters
	if( last - *filePos > ( bufferSize - 1 ))
		last = *filePos + ( bufferSize - 1);

	stop = 0;

	// stop at the next newline (inclusive) or end of buffer
	while( i < last && !stop )
	{
		if( pMemFile[i] == '\n' )
			stop = 1;
		i++;
	}

	// if we actually advanced the pointer, copy it over
	if( i != *filePos )
	{
		// we read in size bytes
		int	size = i - *filePos;

		// copy it out
		memcpy( pBuffer, pMemFile + *filePos, size );
		
		// If the buffer isn't full, terminate (this is always true)
		if( size < bufferSize ) pBuffer[size] = 0;

		// update file pointer
		*filePos = i;
		return pBuffer;
	}

	return NULL;
}

/*
====================
Cache_Check

consistency check
====================
*/
void *Cache_Check( byte *mempool, cache_user_t *c )
{
	if( !c->data )
		return NULL;

	if( !Mem_IsAllocatedExt( mempool, c->data ))
		return NULL;

	return c->data;
}

/*
=============
COM_LoadFileForMe

=============
*/
byte* COM_LoadFileForMe( const char *filename, int *pLength )
{
	string	name;
	byte	*file, *pfile;
	int	iLength;

	if( !filename || !*filename )
	{
		if( pLength )
			*pLength = 0;
		return NULL;
	}

	Q_strncpy( name, filename, sizeof( name ));
	COM_FixSlashes( name );

	pfile = FS_LoadFile( name, &iLength, false );
	if( pLength ) *pLength = iLength;

	if( pfile )
	{
		file = malloc( iLength + 1 );
		if( file != NULL )
		{
			memcpy( file, pfile, iLength );
			file[iLength] = '\0';
		}
		Mem_Free( pfile );
		pfile = file;
	}

	return pfile;
}

/*
=============
COM_LoadFile

=============
*/
byte *COM_LoadFile( const char *filename, int usehunk, int *pLength )
{
	return COM_LoadFileForMe( filename, pLength );
}

/*
=============
COM_LoadFile

=============
*/
int COM_SaveFile( const char *filename, const void *data, long len )
{
	// check for empty filename
	if( !filename || !*filename )
		return false;

	// check for null data
	if( !data || len <= 0 )
		return false;

	return FS_WriteFile( filename, data, len );
}

/*
=============
COM_FreeFile

=============
*/
void COM_FreeFile( void *buffer )
{
	free( buffer ); 
}

/*
=============
COM_NormalizeAngles

=============
*/
void COM_NormalizeAngles( vec3_t angles )
{
	int i;

	for( i = 0; i < 3; i++ )
	{
		if( angles[i] > 180.0f )
			angles[i] -= 360.0f;
		else if( angles[i] < -180.0f )
			angles[i] += 360.0f;
	}
}

/*
=============
pfnGetModelType

=============
*/
int pfnGetModelType( model_t *mod )
{
	if( !mod ) return mod_bad;
	return mod->type;
}

/*
=============
pfnGetModelBounds

=============
*/
void pfnGetModelBounds( model_t *mod, float *mins, float *maxs )
{
	if( mod )
	{
		if( mins ) VectorCopy( mod->mins, mins );
		if( maxs ) VectorCopy( mod->maxs, maxs );
	}
	else
	{
		MsgDev( D_ERROR, "Mod_GetBounds: NULL model\n" );
		if( mins ) VectorClear( mins );
		if( maxs ) VectorClear( maxs );
	}
}

/*
=============
pfnCvar_RegisterServerVariable

standard path to register game variable
=============
*/
void pfnCvar_RegisterServerVariable( cvar_t *variable )
{
	if( variable != NULL )
		SetBits( variable->flags, FCVAR_EXTDLL );
	Cvar_RegisterVariable( (convar_t *)variable );
}

/*
=============
pfnCvar_RegisterEngineVariable

use with precaution: this cvar will NOT unlinked
after game.dll is unloaded
=============
*/
void pfnCvar_RegisterEngineVariable( cvar_t *variable )
{
	Cvar_RegisterVariable( (convar_t *)variable );
}

/*
=============
pfnCvar_RegisterVariable

=============
*/
cvar_t *pfnCvar_RegisterClientVariable( const char *szName, const char *szValue, int flags )
{
	if( FBitSet( flags, FCVAR_GLCONFIG ))
		return (cvar_t *)Cvar_Get( szName, szValue, flags, va( "enable or disable %s", szName ));
	return (cvar_t *)Cvar_Get( szName, szValue, flags|FCVAR_CLIENTDLL, "client cvar" );
}

/*
=============
pfnCvar_RegisterVariable

=============
*/
cvar_t *pfnCvar_RegisterGameUIVariable( const char *szName, const char *szValue, int flags )
{
	if( FBitSet( flags, FCVAR_GLCONFIG ))
		return (cvar_t *)Cvar_Get( szName, szValue, flags, va( "enable or disable %s", szName ));
	return (cvar_t *)Cvar_Get( szName, szValue, flags|FCVAR_GAMEUIDLL, "GameUI cvar" );
}

/*
=============
pfnCVarGetPointer

can return NULL
=============
*/
cvar_t *pfnCVarGetPointer( const char *szVarName )
{
	return (cvar_t *)Cvar_FindVar( szVarName );
}

/*
=============
pfnCVarDirectSet

allow to set cvar directly
=============
*/
void pfnCVarDirectSet( cvar_t *var, const char *szValue )
{
	Cvar_DirectSet( (convar_t *)var, szValue );
}

/*
=============
Con_Printf

=============
*/
void Con_Printf( char *szFmt, ... )
{
	static char	buffer[16384];	// must support > 1k messages
	va_list		args;

	if( host.developer < D_INFO )
		return;

	va_start( args, szFmt );
	Q_vsnprintf( buffer, sizeof( buffer ), szFmt, args );
	va_end( args );

	Sys_Print( buffer );
}

/*
=============
Con_DPrintf

=============
*/
void Con_DPrintf( char *szFmt, ... )
{
	static char	buffer[16384];	// must support > 1k messages
	va_list		args;

	if( host.developer < D_ERROR )
		return;

	va_start( args, szFmt );
	Q_vsnprintf( buffer, sizeof( buffer ), szFmt, args );
	va_end( args );

	Sys_Print( buffer );
}

/*
=============
COM_CompareFileTime

=============
*/
int COM_CompareFileTime( const char *filename1, const char *filename2, int *iCompare )
{
	int	bRet = 0;

	*iCompare = 0;

	if( filename1 && filename2 )
	{
		long ft1 = FS_FileTime( filename1, false );
		long ft2 = FS_FileTime( filename2, false );

		// one of files is missing
		if( ft1 == -1 || ft2 == -1 )
			return bRet;

		*iCompare = Host_CompareFileTime( ft1,  ft2 );
		bRet = 1;
	}

	return bRet;
}

/*
=============
pfnGetGameDir

=============
*/
void pfnGetGameDir( char *szGetGameDir )
{
	if( !szGetGameDir ) return;
	Q_sprintf( szGetGameDir, "%s/%s", host.rootdir, GI->gamedir );
}

/*
=============
pfnSequenceGet

used by CS:CZ
=============
*/
void *pfnSequenceGet( const char *fileName, const char *entryName )
{
	Msg( "Sequence_Get: file %s, entry %s\n", fileName, entryName );
	return NULL;
}

/*
=============
pfnSequencePickSentence

used by CS:CZ
=============
*/
void *pfnSequencePickSentence( const char *groupName, int pickMethod, int *picked )
{
	Msg( "Sequence_PickSentence: group %s, pickMethod %i\n", groupName, pickMethod );
	*picked = 0;

	return NULL;
}

/*
=============
pfnIsCareerMatch

used by CS:CZ (client stub)
=============
*/
int pfnIsCareerMatch( void )
{
	return 0;
}

/*
=============
pfnRegisterTutorMessageShown

only exists in PlayStation version
=============
*/
void pfnRegisterTutorMessageShown( int mid )
{
}

/*
=============
pfnGetTimesTutorMessageShown

only exists in PlayStation version
=============
*/
int pfnGetTimesTutorMessageShown( int mid )
{
	return 0;
}

/*
=============
pfnProcessTutorMessageDecayBuffer

only exists in PlayStation version
=============
*/
void pfnProcessTutorMessageDecayBuffer( int *buffer, int bufferLength )
{
}

/*
=============
pfnConstructTutorMessageDecayBuffer

only exists in PlayStation version
=============
*/
void pfnConstructTutorMessageDecayBuffer( int *buffer, int bufferLength )
{
}

/*
=============
pfnResetTutorMessageDecayData

only exists in PlayStation version
=============
*/
void pfnResetTutorMessageDecayData( void )
{
}