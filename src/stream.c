/*
 * stream.c
 *
 * All rights reserved. Copyright (C) 1996 by NARITA Tomio.
 * $Id: stream.c,v 1.5 2003/11/13 03:08:19 nrt Exp $
 */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#ifdef UNIX
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#endif /* UNIX */

#ifdef MSDOS
#include <dos.h>
#endif /* MSDOS */

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#include <process.h>
#include <windows.h>
#endif /* _WIN32 */

#include <import.h>
#include <uty.h>
#include <command.h>
#include <begin.h>
#include <stream.h>

private byte *gz_filter = "zcat";
private byte *bz2_filter = "bzcat";
private byte *xz_filter = "xzcat";
private byte *zstd_filter = "zstdcat";

private stream_t *StreamAlloc()
{
  stream_t *st;

  st = (stream_t *)Malloc( sizeof( stream_t ) );

  st->fp  = NULL;
  st->sp  = NULL;
  st->pid = -1;

  return st;
}

#ifdef _WIN32
/* MSVC's tmpfile() creates a file on a root directory.
 * It might fail on Windows Vista or later.
 * Implement our own tmpfile(). */
private FILE *mytmpfile( void )
{
  TCHAR TempFileName[ MAX_PATH ];
  TCHAR TempPath[ MAX_PATH ];
  DWORD ret;
  HANDLE hFile;
  static UINT cnt = 0;
  UINT tick, org_cnt;
  int fd;
  FILE *fp;

  ret = GetTempPath( MAX_PATH, TempPath );
  if( ret > MAX_PATH || ret == 0 )
    return NULL;

  tick = GetTickCount();
  org_cnt = cnt;
  do {
    if (++cnt - org_cnt > 65536)
      return NULL;
    if (tick + cnt == 0)
      ++cnt;
    ret = GetTempFileName( TempPath, TEXT( "lv" ), tick + cnt, TempFileName );
    if( ret == 0 )
      return NULL;

    hFile = CreateFile( TempFileName, GENERIC_READ | GENERIC_WRITE,
	0, NULL, CREATE_NEW,
	FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE, NULL );
  } while ( hFile == INVALID_HANDLE_VALUE
      && GetLastError() == ERROR_FILE_EXISTS );
  if( hFile == INVALID_HANDLE_VALUE )
    return NULL;

  fd = _open_osfhandle( (intptr_t)hFile, 0 );
  if( fd == -1 ){
    CloseHandle( hFile );
    return NULL;
  }

  fp = _fdopen( fd, "w+b" );
  if( fp == NULL ){
    _close( fd );
    return NULL;
  }
  return fp;
}
#define tmpfile()   mytmpfile()
#endif /* _WIN32 */

private int CreateArgv( byte *filter, byte *file, byte ***pargv )
{
  int argc, i;
  byte **argv;
  byte *s;
  byte quotationChar = 0;

  s = filter;
  for( i = 0; s = strpbrk( s, " \t" ); i++, s++ )
    ;
  argv = Malloc( sizeof( byte * ) * ( i + 3 ) );
  argc = 0;
  s = filter;
  while( 0x00 != *s ){
    if( '\'' == *s || '"' == *s ){
      quotationChar = *s;
      argv[ argc ] = TokenAlloc( s );
      s++;
      while( 0x00 != *s && quotationChar != *s )
	s++;
    } else {
      argv[ argc ] = TokenAlloc( s );
      s++;
      while( 0x00 != *s && ' ' != *s && '\t' != *s )
	s++;
    }
    if( 0x00 != *s )
      s++;
    argc++;
  }
  argv[ argc++ ] = Strdup( file );
  argv[ argc ] = NULL;

  *pargv = argv;
  return argc;
}

private void DestroyArgv( byte **argv )
{
  int i;

  if( NULL == argv )
    return;
  for( i = 0; NULL != argv[ i ]; i++ )
    free( argv[ i ] );
  free( argv );
}

public stream_t *StreamOpen( byte *file )
{
  stream_t *st;
  byte *exts, *filter = NULL;

  if( access( file, 0 ) ){
    perror( file );
    return NULL;
  }

  st = StreamAlloc();

  if( !strcmp( "AUTO", filter_program ) ){
    if( NULL != (exts = Exts( file )) ){
      if( !strcmp( "gz", exts ) || !strcmp( "GZ", exts )
	  || !strcmp( "z", exts ) || !strcmp( "Z", exts ) )
	filter = gz_filter;
      else if( !strcmp( "bz2", exts ) || !strcmp( "BZ2", exts ) )
	filter = bz2_filter;
      else if( !strcmp( "xz", exts ) || !strcmp( "XZ", exts )
	  || !strcmp( "lzma", exts ) || !strcmp( "LZMA", exts ) )
	filter = xz_filter;
      else if( !strcmp( "zst", exts ) || !strcmp( "ZST", exts ) )
    filter = zstd_filter;
    }
  } else if( !strcmp( "NONE", filter_program ) ){
    filter = NULL;
  } else {
    filter = filter_program;
  }
  if( NULL != filter ){
    /*
     * zcat, bzcat or xzcat
     */
    byte **argv;

    if( NULL == (st->fp = (FILE *)tmpfile()) )
      perror( "temporary file" ), exit( -1 );

    CreateArgv( filter, file, &argv );

#if defined( MSDOS ) || defined( _WIN32 )
    { int sout;

      sout = dup( 1 );
      close( 1 );
      dup( fileno( st->fp ) );
      if( 0 > spawnvp( 0, argv[0], (char **)argv ) )
	FatalErrorOccurred();
      close( 1 );
      dup( sout );
      rewind( st->fp );

      DestroyArgv( argv );
      return st;
    }
#endif /* MSDOS,_WIN32 */

#ifdef UNIX
    { int fds[ 2 ], pid;

      if( 0 > pipe( fds ) )
	perror( "pipe" ), exit( -1 );

      switch( pid = fork() ){
      case -1:
	perror( "fork" ), exit( -1 );
      case 0:
	/*
	 * child process
	 */
	close( fds[ 0 ] );
	close( 1 );
	dup( fds[ 1 ] );
	if( 0 > execvp( argv[0], (char **)argv ) )
	  perror( filter ), exit( -1 );
	/*
	 * never reach here
	 */
      default:
	/*
	 * parent process
	 */
	st->pid = pid;
	close( fds[ 1 ] );
	if( NULL == (st->sp = fdopen( fds[ 0 ], "r" )) )
	  perror( "fdopen" ), exit( -1 );

	DestroyArgv( argv );
	return st;
      }
    }
#endif /* UNIX */
  }

  if( NULL == (st->fp = fopen( file, "rb" )) ){
    perror( file );
    return NULL;
  }

  return st;
}

private void StdinDuplicationFailed()
{
  fprintf( stderr, "lv: stdin duplication failed\n" );
  exit( -1 );
}

public stream_t *StreamReconnectStdin()
{
  stream_t *st;
#if defined( UNIX ) || defined( _WIN32 )
  struct stat sbuf;
#endif

  st = StreamAlloc();

#ifdef MSDOS
  if( NULL == (st->fp = fdopen( dup( 0 ), "rb" )) )
    StdinDuplicationFailed();
  close( 0 );
  dup( 1 );
#endif /* MSDOS */

#if defined( UNIX ) || defined( _WIN32 )
  fstat( 0, &sbuf );
  if( S_IFREG == ( sbuf.st_mode & S_IFMT ) ){
    /* regular */
    if( NULL == (st->fp = fdopen( dup( 0 ), "r" )) )
      StdinDuplicationFailed();
  } else {
    /* socket */
    if( NULL == (st->fp = (FILE *)tmpfile()) )
      perror( "temporary file" ), exit( -1 );
    if( NULL == (st->sp = fdopen( dup( 0 ), "r" )) )
      StdinDuplicationFailed();
  }
  close( 0 );
#ifndef _WIN32
  if( IsAtty( 1 ) && 0 != open( "/dev/tty", O_RDONLY ) )
    perror( "/dev/tty" ), exit( -1 );
#endif /* WIN32 */
#endif /* UNIX,_WIN32 */

  return st;
}

public boolean_t StreamClose( stream_t *st )
{
  fclose( st->fp );
  if( st->sp )
    fclose( st->sp );

  free( st );

  return TRUE;
}
