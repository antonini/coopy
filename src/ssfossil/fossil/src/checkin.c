/*
** Copyright (c) 2007 D. Richard Hipp
**
** This program is free software; you can redistribute it and/or
** modify it under the terms of the Simplified BSD License (also
** known as the "2-Clause License" or "FreeBSD License".)

** This program is distributed in the hope that it will be useful,
** but without any warranty; without even the implied warranty of
** merchantability or fitness for a particular purpose.
**
** Author contact information:
**   drh@hwaci.com
**   http://www.hwaci.com/drh/
**
*******************************************************************************
**
** This file contains code used to check-in versions of the project
** from the local repository.
*/
#include "config.h"
#include "checkin.h"
#include <assert.h>

/*
** Generate text describing all changes.  Prepend zPrefix to each line
** of output.
**
** We assume that vfile_check_signature has been run.
**
** If missingIsFatal is true, then any files that are missing or which
** are not true files results in a fatal error.
*/
static void status_report(
  Blob *report,          /* Append the status report here */
  const char *zPrefix,   /* Prefix on each line of the report */
  int missingIsFatal     /* MISSING and NOT_A_FILE are fatal errors */
){
  Stmt q;
  int nPrefix = strlen(zPrefix);
  int nErr = 0;
  db_prepare(&q, 
    "SELECT pathname, deleted, chnged, rid, coalesce(origname!=pathname,0)"
    "  FROM vfile "
    " WHERE file_is_selected(id)"
    "   AND (chnged OR deleted OR rid=0 OR pathname!=origname) ORDER BY 1"
  );
  while( db_step(&q)==SQLITE_ROW ){
    const char *zPathname = db_column_text(&q,0);
    int isDeleted = db_column_int(&q, 1);
    int isChnged = db_column_int(&q,2);
    int isNew = db_column_int(&q,3)==0;
    int isRenamed = db_column_int(&q,4);
    char *zFullName = mprintf("%s/%s", g.zLocalRoot, zPathname);
    blob_append(report, zPrefix, nPrefix);
    if( isDeleted ){
      blob_appendf(report, "DELETED    %s\n", zPathname);
    }else if( !file_isfile(zFullName) ){
      if( access(zFullName, 0)==0 ){
        blob_appendf(report, "NOT_A_FILE %s\n", zPathname);
        if( missingIsFatal ){
          fossil_warning("not a file: %s", zPathname);
          nErr++;
        }
      }else{
        blob_appendf(report, "MISSING    %s\n", zPathname);
        if( missingIsFatal ){
          fossil_warning("missing file: %s", zPathname);
          nErr++;
        }
      }
    }else if( isNew ){
      blob_appendf(report, "ADDED      %s\n", zPathname);
    }else if( isDeleted ){
      blob_appendf(report, "DELETED    %s\n", zPathname);
    }else if( isChnged==2 ){
      blob_appendf(report, "UPDATED_BY_MERGE %s\n", zPathname);
    }else if( isChnged==3 ){
      blob_appendf(report, "ADDED_BY_MERGE %s\n", zPathname);
    }else if( isChnged==1 ){
      blob_appendf(report, "EDITED     %s\n", zPathname);
    }else if( isRenamed ){
      blob_appendf(report, "RENAMED    %s\n", zPathname);
    }
    free(zFullName);
  }
  db_finalize(&q);
  db_prepare(&q, "SELECT uuid FROM vmerge JOIN blob ON merge=rid"
                 " WHERE id=0");
  while( db_step(&q)==SQLITE_ROW ){
    blob_append(report, zPrefix, nPrefix);
    blob_appendf(report, "MERGED_WITH %s\n", db_column_text(&q, 0));
  }
  db_finalize(&q);
  if( nErr ){
    fossil_fatal("aborting due to prior errors");
  }
}

/*
** COMMAND: changes
**
** Usage: %fossil changes
**
** Report on the edit status of all files in the current checkout.
** See also the "status" and "extra" commands.
*/
void changes_cmd(void){
  Blob report;
  int vid;
  db_must_be_within_tree();
  blob_zero(&report);
  vid = db_lget_int("checkout", 0);
  vfile_check_signature(vid, 0);
  status_report(&report, "", 0);
  blob_write_to_file(&report, "-");
}

/*
** COMMAND: status
**
** Usage: %fossil status
**
** Report on the status of the current checkout.
*/
void status_cmd(void){
  int vid;
  db_must_be_within_tree();
       /* 012345678901234 */
  printf("repository:   %s\n", db_lget("repository",""));
  printf("local-root:   %s\n", g.zLocalRoot);
  printf("server-code:  %s\n", db_get("server-code", ""));
  vid = db_lget_int("checkout", 0);
  if( vid ){
    show_common_info(vid, "checkout:", 0);
  }
  changes_cmd();
}

/*
** COMMAND: ls
**
** Usage: %fossil ls [-l]
**
** Show the names of all files in the current checkout.  The -l provides
** extra information about each file.
*/
void ls_cmd(void){
  int vid;
  Stmt q;
  int isBrief;

  isBrief = find_option("l","l", 0)==0;
  db_must_be_within_tree();
  vid = db_lget_int("checkout", 0);
  vfile_check_signature(vid, 0);
  db_prepare(&q,
     "SELECT pathname, deleted, rid, chnged, coalesce(origname!=pathname,0)"
     "  FROM vfile"
     " ORDER BY 1"
  );
  while( db_step(&q)==SQLITE_ROW ){
    const char *zPathname = db_column_text(&q,0);
    int isDeleted = db_column_int(&q, 1);
    int isNew = db_column_int(&q,2)==0;
    int chnged = db_column_int(&q,3);
    int renamed = db_column_int(&q,4);
    char *zFullName = mprintf("%s/%s", g.zLocalRoot, zPathname);
    if( isBrief ){
      printf("%s\n", zPathname);
    }else if( isNew ){
      printf("ADDED      %s\n", zPathname);
    }else if( !file_isfile(zFullName) ){
      if( access(zFullName, 0)==0 ){
        printf("NOT_A_FILE %s\n", zPathname);
      }else{
        printf("MISSING    %s\n", zPathname);
      }
    }else if( isDeleted ){
      printf("DELETED    %s\n", zPathname);
    }else if( chnged ){
      printf("EDITED     %s\n", zPathname);
    }else if( renamed ){
      printf("RENAMED    %s\n", zPathname);
    }else{
      printf("UNCHANGED  %s\n", zPathname);
    }
    free(zFullName);
  }
  db_finalize(&q);
}

/*
** Construct and return a string which is an SQL expression that will
** be TRUE if value zVal matches any of the GLOB expressions in the list
** zGlobList.  For example:
**
**    zVal:       "x"
**    zGlobList:  "*.o,*.obj"
**
**    Result:     "(x GLOB '*.o' OR x GLOB '*.obj')"
**
** Each element of the GLOB list may optionally be enclosed in either '...'
** or "...".  This allows commas in the expression.  Whitespace at the
** beginning and end of each GLOB pattern is ignored, except when enclosed
** within '...' or "...".
**
** This routine makes no effort to free the memory space it uses.
*/
char *glob_expr(const char *zVal, const char *zGlobList){
  Blob expr;
  char *zSep = "(";
  int nTerm = 0;
  int i;
  int cTerm;

  if( zGlobList==0 || zGlobList[0]==0 ) return "0";
  blob_zero(&expr);
  while( zGlobList[0] ){
    while( isspace(zGlobList[0]) || zGlobList[0]==',' ) zGlobList++;
    if( zGlobList[0]==0 ) break;
    if( zGlobList[0]=='\'' || zGlobList[0]=='"' ){
      cTerm = zGlobList[0];
      zGlobList++;
    }else{
      cTerm = ',';
    }
    for(i=0; zGlobList[i] && zGlobList[i]!=cTerm; i++){}
    if( cTerm==',' ){
      while( i>0 && isspace(zGlobList[i-1]) ){ i--; }
    }
    blob_appendf(&expr, "%s%s GLOB '%.*q'", zSep, zVal, i, zGlobList);
    zSep = " OR ";
    if( cTerm!=',' && zGlobList[i] ) i++;
    zGlobList += i;
    if( zGlobList[0] ) zGlobList++;
    nTerm++;
  }
  if( nTerm ){
    blob_appendf(&expr, ")");
    return blob_str(&expr);
  }else{
    return "0";
  }
}

/*
** COMMAND: extras
** Usage: %fossil extras ?--dotfiles? ?--ignore GLOBPATTERN?
**
** Print a list of all files in the source tree that are not part of
** the current checkout.  See also the "clean" command.
**
** Files and subdirectories whose names begin with "." are normally
** ignored but can be included by adding the --dotfiles option.
*/
void extra_cmd(void){
  Blob path;
  Blob repo;
  Stmt q;
  int n;
  const char *zIgnoreFlag = find_option("ignore",0,1);
  int allFlag = find_option("dotfiles",0,0)!=0;

  db_must_be_within_tree();
  db_multi_exec("CREATE TEMP TABLE sfile(x TEXT PRIMARY KEY)");
  n = strlen(g.zLocalRoot);
  blob_init(&path, g.zLocalRoot, n-1);
  if( zIgnoreFlag==0 ){
    zIgnoreFlag = db_get("ignore-glob", 0);
  }
  vfile_scan(0, &path, blob_size(&path), allFlag);
  db_prepare(&q, 
      "SELECT x FROM sfile"
      " WHERE x NOT IN ('manifest','manifest.uuid','_FOSSIL_',"
                       "'_FOSSIL_-journal','.fos','.fos-journal',"
                       "'_FOSSIL_-wal','_FOSSIL_-shm','.fos-wal',"
                       "'.fos-shm')"
      "   AND NOT %s"
      " ORDER BY 1",
      glob_expr("x", zIgnoreFlag)
  );
  if( file_tree_name(g.zRepositoryName, &repo, 0) ){
    db_multi_exec("DELETE FROM sfile WHERE x=%B", &repo);
  }
  while( db_step(&q)==SQLITE_ROW ){
    printf("%s\n", db_column_text(&q, 0));
  }
  db_finalize(&q);
}

/*
** COMMAND: clean
** Usage: %fossil clean ?--force? ?--dotfiles?
**
** Delete all "extra" files in the source tree.  "Extra" files are
** files that are not officially part of the checkout.  See also
** the "extra" command. This operation cannot be undone. 
**
** You will be prompted before removing each file. If you are
** sure you wish to remove all "extra" files you can specify the
** optional --force flag and no prompts will be issued.
**
** Files and subdirectories whose names begin with "." are
** normally ignored.  They are included if the "--dotfiles" option
** is used.
*/
void clean_cmd(void){
  int allFlag;
  int dotfilesFlag;
  Blob path, repo;
  Stmt q;
  int n;
  allFlag = find_option("force","f",0)!=0;
  dotfilesFlag = find_option("dotfiles",0,0)!=0;
  db_must_be_within_tree();
  db_multi_exec("CREATE TEMP TABLE sfile(x TEXT PRIMARY KEY)");
  n = strlen(g.zLocalRoot);
  blob_init(&path, g.zLocalRoot, n-1);
  vfile_scan(0, &path, blob_size(&path), dotfilesFlag);
  db_prepare(&q, 
      "SELECT %Q || x FROM sfile"
      " WHERE x NOT IN ('manifest','manifest.uuid','_FOSSIL_',"
                       "'_FOSSIL_-journal','.fos','.fos-journal',"
                       "'_FOSSIL_-wal','_FOSSIL_-shm','.fos-wal',"
                       "'.fos-shm')"
      " ORDER BY 1", g.zLocalRoot);
  if( file_tree_name(g.zRepositoryName, &repo, 0) ){
    db_multi_exec("DELETE FROM sfile WHERE x=%B", &repo);
  }
  while( db_step(&q)==SQLITE_ROW ){
    if( allFlag ){
      unlink(db_column_text(&q, 0));
    }else{
      Blob ans;
      char *prompt = mprintf("remove unmanaged file \"%s\" (y/N)? ",
                              db_column_text(&q, 0));
      blob_zero(&ans);
      prompt_user(prompt, &ans);
      if( blob_str(&ans)[0]=='y' ){
        unlink(db_column_text(&q, 0));
      }
    }
  }
  db_finalize(&q);
}

/*
** Prepare a commit comment.  Let the user modify it using the
** editor specified in the global_config table or either
** the VISUAL or EDITOR environment variable.
**
** Store the final commit comment in pComment.  pComment is assumed
** to be uninitialized - any prior content is overwritten.
**
** zInit is the text of the most recent failed attempt to check in
** this same change.  Use zInit to reinitialize the check-in comment
** so that the user does not have to retype.
**
** zBranch is the name of a new branch that this check-in is forced into.
** zBranch might be NULL or an empty string if no forcing occurs.
**
** parent_rid is the recordid of the parent check-in.
*/
static void prepare_commit_comment(
  Blob *pComment,
  char *zInit,
  const char *zBranch,
  int parent_rid
){
  const char *zEditor;
  char *zCmd;
  char *zFile;
  Blob text, line;
  char *zComment;
  int i;
  blob_init(&text, zInit, -1);
  blob_append(&text,
    "\n"
    "# Enter comments on this check-in.  Lines beginning with # are ignored.\n"
    "# The check-in comment follows wiki formatting rules.\n"
    "#\n", -1
  );
  if( zBranch && zBranch[0] ){
    blob_appendf(&text, "# tags: %s\n#\n", zBranch);
  }else{
    char *zTags = info_tags_of_checkin(parent_rid, 1);
    if( zTags )  blob_appendf(&text, "# tags: %z\n#\n", zTags);
  }
  if( g.markPrivate ){
    blob_append(&text,
      "# PRIVATE BRANCH: This check-in will be private and will not sync to\n"
      "# repositories.\n"
      "#\n", -1
    );
  }
  status_report(&text, "# ", 1);
  zEditor = db_get("editor", 0);
  if( zEditor==0 ){
    zEditor = getenv("VISUAL");
  }
  if( zEditor==0 ){
    zEditor = getenv("EDITOR");
  }
  if( zEditor==0 ){
#if defined(_WIN32)
    zEditor = "notepad";
#else
    zEditor = "ed";
#endif
  }
  zFile = db_text(0, "SELECT '%qci-comment-' || hex(randomblob(6)) || '.txt'",
                   g.zLocalRoot);
#if defined(_WIN32)
  blob_add_cr(&text);
#endif
  blob_write_to_file(&text, zFile);
  zCmd = mprintf("%s \"%s\"", zEditor, zFile);
  printf("%s\n", zCmd);
  if( portable_system(zCmd) ){
    fossil_panic("editor aborted");
  }
  blob_reset(&text);
  blob_read_from_file(&text, zFile);
  blob_remove_cr(&text);
  unlink(zFile);
  free(zFile);
  blob_zero(pComment);
  while( blob_line(&text, &line) ){
    int i, n;
    char *z;
    n = blob_size(&line);
    z = blob_buffer(&line);
    for(i=0; i<n && isspace(z[i]);  i++){}
    if( i<n && z[i]=='#' ) continue;
    if( i<n || blob_size(pComment)>0 ){
      blob_appendf(pComment, "%b", &line);
    }
  }
  blob_reset(&text);
  zComment = blob_str(pComment);
  i = strlen(zComment);
  while( i>0 && isspace(zComment[i-1]) ){ i--; }
  blob_resize(pComment, i);
}

/*
** Populate the Global.aCommitFile[] based on the command line arguments
** to a [commit] command. Global.aCommitFile is an array of integers
** sized at (N+1), where N is the number of arguments passed to [commit].
** The contents are the [id] values from the vfile table corresponding
** to the filenames passed as arguments.
**
** The last element of aCommitFile[] is always 0 - indicating the end
** of the array.
**
** If there were no arguments passed to [commit], aCommitFile is not
** allocated and remains NULL. Other parts of the code interpret this
** to mean "all files".
*/
void select_commit_files(void){
  if( g.argc>2 ){
    int ii;
    Blob b;
    blob_zero(&b);
    g.aCommitFile = malloc(sizeof(int)*(g.argc-1));

    for(ii=2; ii<g.argc; ii++){
      int iId;
      file_tree_name(g.argv[ii], &b, 1);
      iId = db_int(-1, "SELECT id FROM vfile WHERE pathname=%Q", blob_str(&b));
      if( iId<0 ){
        fossil_fatal("fossil knows nothing about: %s", g.argv[ii]);
      }
      g.aCommitFile[ii-2] = iId;
      blob_reset(&b);
    }
    g.aCommitFile[ii-2] = 0;
  }
}

/*
** Return true if the check-in with RID=rid is a leaf.
** A leaf has no children in the same branch. 
*/
int is_a_leaf(int rid){
  int rc;
  static const char zSql[] = 
    @ SELECT 1 FROM plink
    @  WHERE pid=%d
    @    AND coalesce((SELECT value FROM tagxref
    @                   WHERE tagid=%d AND rid=plink.pid), 'trunk')
    @       =coalesce((SELECT value FROM tagxref
    @                   WHERE tagid=%d AND rid=plink.cid), 'trunk')
  ;
  rc = db_int(0, zSql, rid, TAG_BRANCH, TAG_BRANCH);
  return rc==0;
}

/*
** Make sure the current check-in with timestamp zDate is younger than its
** ancestor identified rid and zUuid.  Throw a fatal error if not.
*/
static void checkin_verify_younger(
  int rid,              /* The record ID of the ancestor */
  const char *zUuid,    /* The artifact ID of the ancestor */
  const char *zDate     /* Date & time of the current check-in */
){
#ifndef FOSSIL_ALLOW_OUT_OF_ORDER_DATES
  int b;
  b = db_exists(
    "SELECT 1 FROM event"
    " WHERE datetime(mtime)>=%Q"
    "   AND type='ci' AND objid=%d",
    zDate, rid
  );
  if( b ){
    fossil_fatal("ancestor check-in [%.10s] (%s) is younger (clock skew?)"
                 " Use -f to override.", zUuid, zDate);
  }
#endif
}

/*
** zDate should be a valid date string.  Convert this string into the
** format YYYY-MM-DDTHH:MM:SS.  If the string is not a valid date, 
** print a fatal error and quit.
*/
char *date_in_standard_format(const char *zInputDate){
  char *zDate = db_text(0, "SELECT datetime(%Q)", zInputDate);
  if( zDate[0]==0 ){
    fossil_fatal("unrecognized date format (%s): use \"YYYY-MM-DD HH:MM:SS\"",
                 zInputDate);
  }
  assert( strlen(zDate)==19 );
  assert( zDate[10]==' ' );
  zDate[10] = 'T';
  return zDate;
}

/*
** COMMAND: ci
** COMMAND: commit
**
** Usage: %fossil commit ?OPTIONS? ?FILE...?
**
** Create a new version containing all of the changes in the current
** checkout.  You will be prompted to enter a check-in comment unless
** the comment has been specified on the command-line using "-m".
** The editor defined in the "editor" fossil option (see %fossil help set)
** will be used, or from the "VISUAL" or "EDITOR" environment variables
** (in that order) if no editor is set.
**
** You will be prompted for your GPG passphrase in order to sign the
** new manifest unless the "--nosign" option is used.  All files that
** have changed will be committed unless some subset of files is
** specified on the command line.
**
** The --branch option followed by a branch name cases the new check-in
** to be placed in the named branch.  The --bgcolor option can be followed
** by a color name (ex:  '#ffc0c0') to specify the background color of
** entries in the new branch when shown in the web timeline interface.
**
** A check-in is not permitted to fork unless the --force or -f
** option appears.  A check-in is not allowed against a closed check-in.
**
** The --private option creates a private check-in that is never synced.
** Children of private check-ins are automatically private.
**
** Options:
**
**    --comment|-m COMMENT-TEXT
**    --branch NEW-BRANCH-NAME
**    --bgcolor COLOR
**    --nosign
**    --force|-f
**    --private
**    
*/
void commit_cmd(void){
  int rc;
  int vid, nrid, nvid;
  Blob comment;
  const char *zComment;
  Stmt q;
  Stmt q2;
  char *zUuid, *zDate;
  int noSign = 0;        /* True to omit signing the manifest using GPG */
  int isAMerge = 0;      /* True if checking in a merge */
  int forceFlag = 0;     /* Force a fork */
  char *zManifestFile;   /* Name of the manifest file */
  int nBasename;         /* Length of "g.zLocalRoot/" */
  const char *zBranch;   /* Create a new branch with this name */
  const char *zBgColor;  /* Set background color when branching */
  const char *zDateOvrd; /* Override date string */
  const char *zUserOvrd; /* Override user name */
  const char *zComFile;  /* Read commit message from this file */
  Blob filename;         /* complete filename */
  Blob manifest;
  Blob muuid;            /* Manifest uuid */
  Blob mcksum;           /* Self-checksum on the manifest */
  Blob cksum1, cksum2;   /* Before and after commit checksums */
  Blob cksum1b;          /* Checksum recorded in the manifest */
 
  url_proxy_options();
  noSign = find_option("nosign",0,0)!=0;
  zComment = find_option("comment","m",1);
  forceFlag = find_option("force", "f", 0)!=0;
  zBranch = find_option("branch","b",1);
  zBgColor = find_option("bgcolor",0,1);
  zComFile = find_option("message-file", "M", 1);
  if( find_option("private",0,0) ){
    g.markPrivate = 1;
    if( zBranch==0 ) zBranch = "private";
    if( zBgColor==0 ) zBgColor = "#fec084";  /* Orange */
  }
  zDateOvrd = find_option("date-override",0,1);
  zUserOvrd = find_option("user-override",0,1);
  db_must_be_within_tree();
  noSign = db_get_boolean("omitsign", 0)|noSign;
  if( db_get_boolean("clearsign", 0)==0 ){ noSign = 1; }
  verify_all_options();

  /* Get the ID of the parent manifest artifact */
  vid = db_lget_int("checkout", 0);
  if( content_is_private(vid) ){
    g.markPrivate = 1;
  }

  /*
  ** Autosync if autosync is enabled and this is not a private check-in.
  */
  if( !g.markPrivate ){
    autosync(AUTOSYNC_PULL);
  }

  /* There are two ways this command may be executed. If there are
  ** no arguments following the word "commit", then all modified files
  ** in the checked out directory are committed. If one or more arguments
  ** follows "commit", then only those files are committed.
  **
  ** After the following function call has returned, the Global.aCommitFile[]
  ** array is allocated to contain the "id" field from the vfile table
  ** for each file to be committed. Or, if aCommitFile is NULL, all files
  ** should be committed.
  */
  select_commit_files();
  isAMerge = db_exists("SELECT 1 FROM vmerge");
  if( g.aCommitFile && isAMerge ){
    fossil_fatal("cannot do a partial commit of a merge");
  }

  user_select();
  /*
  ** Check that the user exists.
  */
  if( !db_exists("SELECT 1 FROM user WHERE login=%Q", g.zLogin) ){
    fossil_fatal("no such user: %s", g.zLogin);
  }
  
  db_begin_transaction();
  db_record_repository_filename(0);
  rc = unsaved_changes();
  if( rc==0 && !isAMerge && !forceFlag ){
    fossil_panic("nothing has changed");
  }

  /* If one or more files that were named on the command line have not
  ** been modified, bail out now.
  */
  if( g.aCommitFile ){
    Blob unmodified;
    memset(&unmodified, 0, sizeof(Blob));
    blob_init(&unmodified, 0, 0);
    db_blob(&unmodified, 
      "SELECT pathname FROM vfile WHERE chnged = 0 AND file_is_selected(id)"
    );
    if( strlen(blob_str(&unmodified)) ){
      fossil_panic("file %s has not changed", blob_str(&unmodified));
    }
  }

  /*
  ** Do not allow a commit that will cause a fork unless the --force flag
  ** is used or unless this is a private check-in.
  */
  if( zBranch==0 && forceFlag==0 && g.markPrivate==0 && !is_a_leaf(vid) ){
    fossil_fatal("would fork.  \"update\" first or use -f or --force.");
  }

  /*
  ** Do not allow a commit against a closed leaf 
  */
  if( db_exists("SELECT 1 FROM tagxref"
                " WHERE tagid=%d AND rid=%d AND tagtype>0",
                TAG_CLOSED, vid) ){
    fossil_fatal("cannot commit against a closed leaf");
  }

  vfile_aggregate_checksum_disk(vid, &cksum1);
  if( zComment ){
    blob_zero(&comment);
    blob_append(&comment, zComment, -1);
  }else if( zComFile ){
    blob_zero(&comment);
    blob_read_from_file(&comment, zComFile);
  }else{
    char *zInit = db_text(0, "SELECT value FROM vvar WHERE name='ci-comment'");
    prepare_commit_comment(&comment, zInit, zBranch, vid);
    free(zInit);
  }
  if( blob_size(&comment)==0 ){
    Blob ans;
    blob_zero(&ans);
    prompt_user("empty check-in comment.  continue (y/N)? ", &ans);
    if( blob_str(&ans)[0]!='y' ){
      fossil_exit(1);
    }
  }else{
    db_multi_exec("REPLACE INTO vvar VALUES('ci-comment',%B)", &comment);
    db_end_transaction(0);
    db_begin_transaction();
  }

  /* Step 1: Insert records for all modified files into the blob 
  ** table. If there were arguments passed to this command, only
  ** the identified fils are inserted (if they have been modified).
  */
  db_prepare(&q,
    "SELECT id, %Q || pathname, mrid FROM vfile "
    "WHERE chnged==1 AND NOT deleted AND file_is_selected(id)"
    , g.zLocalRoot
  );
  while( db_step(&q)==SQLITE_ROW ){
    int id, rid;
    const char *zFullname;
    Blob content;

    id = db_column_int(&q, 0);
    zFullname = db_column_text(&q, 1);
    rid = db_column_int(&q, 2);

    blob_zero(&content);
    blob_read_from_file(&content, zFullname);
    nrid = content_put(&content, 0, 0);
    blob_reset(&content);
    if( rid>0 ){
      content_deltify(rid, nrid, 0);
    }
    db_multi_exec("UPDATE vfile SET mrid=%d, rid=%d WHERE id=%d", nrid,nrid,id);
    db_multi_exec("INSERT OR IGNORE INTO unsent VALUES(%d)", nrid);
  }
  db_finalize(&q);

  /* Create the manifest */
  blob_zero(&manifest);
  if( blob_size(&comment)==0 ){
    blob_append(&comment, "(no comment)", -1);
  }
  blob_appendf(&manifest, "C %F\n", blob_str(&comment));
  zDate = date_in_standard_format(zDateOvrd ? zDateOvrd : "now");
  blob_appendf(&manifest, "D %s\n", zDate);
  zDate[10] = ' ';
  db_prepare(&q,
    "SELECT pathname, uuid, origname, blob.rid, isexe"
    "  FROM vfile JOIN blob ON vfile.mrid=blob.rid"
    " WHERE NOT deleted AND vfile.vid=%d"
    " ORDER BY 1", vid);
  blob_zero(&filename);
  blob_appendf(&filename, "%s", g.zLocalRoot);
  nBasename = blob_size(&filename);
  while( db_step(&q)==SQLITE_ROW ){
    const char *zName = db_column_text(&q, 0);
    const char *zUuid = db_column_text(&q, 1);
    const char *zOrig = db_column_text(&q, 2);
    int frid = db_column_int(&q, 3);
    int isexe = db_column_int(&q, 4);
    const char *zPerm;
    blob_append(&filename, zName, -1);
#if !defined(_WIN32)
    /* For unix, extract the "executable" permission bit directly from
    ** the filesystem.  On windows, the "executable" bit is retained
    ** unchanged from the original. */
    isexe = file_isexe(blob_str(&filename));
#endif
    if( isexe ){
      zPerm = " x";
    }else{
      zPerm = "";
    }
    blob_resize(&filename, nBasename);
    if( zOrig==0 || strcmp(zOrig,zName)==0 ){
      blob_appendf(&manifest, "F %F %s%s\n", zName, zUuid, zPerm);
    }else{
      if( zPerm[0]==0 ){ zPerm = " w"; }
      blob_appendf(&manifest, "F %F %s%s %F\n", zName, zUuid, zPerm, zOrig);
    }
    if( !g.markPrivate ) content_make_public(frid);
  }
  blob_reset(&filename);
  db_finalize(&q);
  zUuid = db_text(0, "SELECT uuid FROM blob WHERE rid=%d", vid);
  blob_appendf(&manifest, "P %s", zUuid);

  if( !forceFlag ){
    checkin_verify_younger(vid, zUuid, zDate);
    db_prepare(&q2, "SELECT merge FROM vmerge WHERE id=:id");
    db_bind_int(&q2, ":id", 0);
    while( db_step(&q2)==SQLITE_ROW ){
      int mid = db_column_int(&q2, 0);
      if( !g.markPrivate && content_is_private(mid) ) continue;
      zUuid = db_text(0, "SELECT uuid FROM blob WHERE rid=%d", mid);
      if( zUuid ){
        blob_appendf(&manifest, " %s", zUuid);
        checkin_verify_younger(mid, zUuid, zDate);
        free(zUuid);
      }
    }
    db_finalize(&q2);
  }

  blob_appendf(&manifest, "\n");
  blob_appendf(&manifest, "R %b\n", &cksum1);
  if( zBranch && zBranch[0] ){
    Stmt q;
    if( zBgColor && zBgColor[0] ){
      blob_appendf(&manifest, "T *bgcolor * %F\n", zBgColor);
    }
    blob_appendf(&manifest, "T *branch * %F\n", zBranch);
    blob_appendf(&manifest, "T *sym-%F *\n", zBranch);

    /* Cancel all other symbolic tags */
    db_prepare(&q,
        "SELECT tagname FROM tagxref, tag"
        " WHERE tagxref.rid=%d AND tagxref.tagid=tag.tagid"
        "   AND tagtype>0 AND tagname GLOB 'sym-*'"
        "   AND tagname!='sym-'||%Q"
        " ORDER BY tagname",
        vid, zBranch);
    while( db_step(&q)==SQLITE_ROW ){
      const char *zTag = db_column_text(&q, 0);
      blob_appendf(&manifest, "T -%F *\n", zTag);
    }
    db_finalize(&q);
  }  
  blob_appendf(&manifest, "U %F\n", zUserOvrd ? zUserOvrd : g.zLogin);
  md5sum_blob(&manifest, &mcksum);
  blob_appendf(&manifest, "Z %b\n", &mcksum);
  zManifestFile = mprintf("%smanifest", g.zLocalRoot);
  if( !noSign && !g.markPrivate && clearsign(&manifest, &manifest) ){
    Blob ans;
    blob_zero(&ans);
    prompt_user("unable to sign manifest.  continue (y/N)? ", &ans);
    if( blob_str(&ans)[0]!='y' ){
      fossil_exit(1);
    }
  }
  blob_write_to_file(&manifest, zManifestFile);
  blob_reset(&manifest);
  blob_read_from_file(&manifest, zManifestFile);
  free(zManifestFile);
  nvid = content_put(&manifest, 0, 0);
  if( nvid==0 ){
    fossil_panic("trouble committing manifest: %s", g.zErrMsg);
  }
  db_multi_exec("INSERT OR IGNORE INTO unsent VALUES(%d)", nvid);
  manifest_crosslink(nvid, &manifest);
  content_deltify(vid, nvid, 0);
  zUuid = db_text(0, "SELECT uuid FROM blob WHERE rid=%d", nvid);
  printf("New_Version: %s\n", zUuid);
  zManifestFile = mprintf("%smanifest.uuid", g.zLocalRoot);
  blob_zero(&muuid);
  blob_appendf(&muuid, "%s\n", zUuid);
  blob_write_to_file(&muuid, zManifestFile);
  free(zManifestFile);
  blob_reset(&muuid);

  
  /* Update the vfile and vmerge tables */
  db_multi_exec(
    "DELETE FROM vfile WHERE (vid!=%d OR deleted) AND file_is_selected(id);"
    "DELETE FROM vmerge WHERE file_is_selected(id) OR id=0;"
    "UPDATE vfile SET vid=%d;"
    "UPDATE vfile SET rid=mrid, chnged=0, deleted=0, origname=NULL"
    " WHERE file_is_selected(id);"
    , vid, nvid
  );
  db_lset_int("checkout", nvid);

  /* Verify that the repository checksum matches the expected checksum
  ** calculated before the checkin started (and stored as the R record
  ** of the manifest file).
  */
  vfile_aggregate_checksum_repository(nvid, &cksum2);
  if( blob_compare(&cksum1, &cksum2) ){
    fossil_panic("tree checksum does not match repository after commit");
  }

  /* Verify that the manifest checksum matches the expected checksum */
  vfile_aggregate_checksum_manifest(nvid, &cksum2, &cksum1b);
  if( blob_compare(&cksum1, &cksum1b) ){
    fossil_panic("manifest checksum does not agree with manifest: "
                 "%b versus %b", &cksum1, &cksum1b);
  }
  if( blob_compare(&cksum1, &cksum2) ){
    fossil_panic("tree checksum does not match manifest after commit: "
                 "%b versus %b", &cksum1, &cksum2);
  }

  /* Verify that the commit did not modify any disk images. */
  vfile_aggregate_checksum_disk(nvid, &cksum2);
  if( blob_compare(&cksum1, &cksum2) ){
    fossil_panic("tree checksums before and after commit do not match");
  }

  /* Clear the undo/redo stack */
  undo_reset();

  /* Commit */
  db_multi_exec("DELETE FROM vvar WHERE name='ci-comment'");
  db_end_transaction(0);

  if( !g.markPrivate ){
    autosync(AUTOSYNC_PUSH);  
  }
  if( count_nonbranch_children(vid)>1 ){
    printf("**** warning: a fork has occurred *****\n");
  }
}
