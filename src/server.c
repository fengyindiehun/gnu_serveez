/*
 * src/server.c - Register your servers here
 *
 * Copyright (C) 2000 Raimi & Ela
 *
 * This is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this package; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <stdio.h>
#include <string.h>
#include <libsizzle/libsizzle.h>

#include "server.h"
#include "hash.h"
#include "alloc.h"
#include "util.h"

/* Include headers of servers
 */
#include "foo-server/foo-proto.h"
#include "awcs-server/awcs-proto.h"

/*
 * The list of registered server. Feel free to add yours.
 */
struct serverdefinition *all_server_definitions[] = {
  &foo_serverdefinition,
  &awcs_serverdefinition,
  NULL
};


/*
 * A list of actually instantiated servers
 */
int server_instances = 0;
struct server **servers = NULL;

/*
 * Local forward declarations
 */
static int set_int (char*,  char*, char*, int *, zzz_scm_t, int);
static int set_intarray (char*, char*, char*, int **, zzz_scm_t, int*);
static int set_string (char*,  char*, char*, char**, zzz_scm_t, char*);
static int set_stringarray (char*, char*, char*, char***, zzz_scm_t, char**);
static int set_hash (char*, char*, char*, hash_t ***, zzz_scm_t,hash_t**);
static int set_port (char*, char*, char*, struct portcfg **, zzz_scm_t,
		     struct portcfg *);
void * server_instantiate (char*, zzz_scm_t, struct serverdefinition*, char*);
static void server_add(struct server *);

/*
 * Helpers to extract info from val, if val == NULL, use the default
 */
static int
set_int (char *cfgfile,
	 char *varname,
	 char *keyname,
	 int *where,
	 zzz_scm_t val,
	 int def)
{
  
  if (val == zzz_undefined) {
      (*where) = def;
      return 0;
    }
  
  if ( !integer_p (val) )
    {
      fprintf(stderr,
	      "%s: Property `%s' of %s should be an integer but is not\n",
	      cfgfile, keyname, varname);
      return -1;
    }

  /* FIXME: do strings and atoi() ? */
  (*where) = integer_val (val);
  return 0;
}

static int
set_intarray (char *cfgfile,
	      char *varname,
	      char *keyname,
	      int **where,
	      zzz_scm_t val,
	      int* def)
{
  int erroneous = 0;
  int i;
  int *array = NULL;

  if ( val == zzz_undefined ) {
    (*where) = def;
    return 0;
  }

  array = (int *) xpmalloc (sizeof (int) * (zzz_list_length (val) +1));

  for (i = 1; cons_p (val); i++, val = cdr (val))
    {
      if ( !integer_p (car (val))) {
	fprintf (stderr,
		 "%s: Element %d of list `%s' is not an integer in `%s'\n",
		 cfgfile, i - 1, keyname, varname);
	erroneous = -1;
      } else {
	array[i] = integer_val ( car (val));
      }

    }
  array[0] = i;

  (*where) = array;

  return erroneous;
}

static int
set_string (char *cfgfile,
	    char *varname,
	    char *keyname,
	    char **where,
	    zzz_scm_t val,
	    char *def)
{
  if ( val == zzz_undefined ) {
    (*where) = xpstrdup(def);
    return 0;
  }

  if ( !string_p (val) )
    {
      fprintf(stderr,
	      "%s: Property `%s' of %s should be a string but is not\n",
	      cfgfile, keyname, varname);
      return -1;
    }

  (*where) = xpstrdup (string_val (val)); 
  return 0;
}


static int
set_stringarray (char *cfgfile,
		 char *varname,
		 char *keyname,
		 char*** where,
		 zzz_scm_t val,
		 char** def)
{
  int erroneous = 0;
  int i;
  char **array = NULL;

  if ( val == zzz_undefined )
    {
      (*where) = def;
      return 0;
    }

  if ( !list_p (val) )
    {
      fprintf(stderr,
	      "%s: Property `%s' of %s should be a list but is not\n",
	      cfgfile, keyname, varname);
      return -1;
    }

  array = (char**) xpmalloc ( sizeof (char *) * (zzz_list_length (val) + 1) );

  for (i = 0; cons_p (val); i++, val = cdr (val))
    {
      if ( !string_p (car (val))) {
	fprintf(stderr,
		"%s: Element %d of list `%s' is not a string in `%s'\n",
		cfgfile, i, keyname, varname);
	erroneous = -1;
      } else {
	array[i] = xpstrdup (string_val (car (val)));
      }
    }
  array[i] = NULL;

  (*where) = array;

  return erroneous;
}

static int
set_hash (char *cfgfile,
	  char *varname,
	  char *keyname,
	  hash_t *** where,
	  zzz_scm_t val,
	  hash_t ** def)
{
  int erroneous = 0;
  unsigned int i;
  zzz_scm_t foo;
  hash_t *h;
  hash_t ** href = (hash_t**) xpmalloc (sizeof (hash_t**));

  if ( val == zzz_undefined ) {
    (*where) = def;
    return 0;
  }

  h = hash_create(17);

  if ( !vector_p (val)) {
    fprintf (stderr, "%s: Element `%s' of `%s' should be a hash but is not\n",
	     cfgfile, keyname, varname);
    return -1;
  }

  for (i = 0; i < vector_len (val); i++)
    {
      for ( foo = zzz_vector_get (val, i);
	    cons_p (foo);
	    foo = cdr (foo))
	{
	  if ( !string_p (car (car (foo)))) {
	    fprintf (stderr, "%s: hash `%s' in `%s' is broken\n",
		     cfgfile, keyname, varname);
	    erroneous = -1;
	    continue;
	  }

	  if ( !string_p (cdr (car (foo)))) {
	    fprintf (stderr, "%s: %s: value of `%s' is not a string in `%s'\n",
		     cfgfile, varname, string_val (car (car (foo))), keyname);
	    erroneous = -1;
	    continue;
	  }

	  hash_put (h,
		    xpstrdup (string_val( car (car (foo)))),
		    xpstrdup (string_val (cdr (car (foo)))));
	}
    }

  (*href) = h;
  (*where) = href;

  return erroneous;
}

/*
 * set a portcfg from a scheme variable
 */

static int set_port (char *cfgfile,
		     char *varname,
		     char *keyname,
		     struct portcfg **where,
		     zzz_scm_t val,
		     struct portcfg *def)
{
  zzz_scm_t key = NULL;
  zzz_scm_t tmp = NULL;
  char *tstr = NULL;
  struct portcfg *newport;

  if ( val == zzz_undefined ) {
    (*where) = def;
    return 0;
  }

  newport = (struct portcfg *) xpmalloc (sizeof (struct portcfg));

  if ( !vector_p (val) ) {
    fprintf (stderr,
	     "%s: portconfig `%s' of `%s' does not specify a protocol\n",
	     cfgfile, keyname, varname);
    return -1;
  }

  /* First, find out what kind of port is about to be recognized */
  key = zzz_make_string ("proto", -1);
  tmp = zzz_hash_ref (val, key, zzz_undefined);

  if ( !string_p (tmp) ) {
    fprintf (stderr,
	     "%s: `proto' of portconfig `%s' of `%s' should be a string\n",
	     cfgfile, keyname, varname);
    return -1;
  }

  tstr = string_val (tmp);

  if ( strcmp (tstr, "tcp") == 0 ) {
    newport->proto = PROTO_TCP;
    key = zzz_make_string ("port", -1);
    tmp = zzz_hash_ref (val, key, zzz_undefined);

    if ( !integer_p (tmp)) {
      fprintf (stderr, "%s: `%s': port is not numerical in %s\n",
	       cfgfile, varname, keyname);
      return -1;
    }
    newport->port = integer_val (tmp);

  } else if (strcmp (tstr, "udp") == 0) {
    newport->proto = PROTO_UDP;

    key = zzz_make_string ("port", -1);
    tmp = zzz_hash_ref (val, key, zzz_undefined);

    if ( !integer_p (tmp)) {
      fprintf (stderr, "%s: `%s': port is not numerical in %s\n",
	       cfgfile, varname, keyname);
      return -1;
    }
    newport->port = integer_val (tmp);

  } else if (strcmp (tstr, "pipe") == 0) {
    newport->proto = PROTO_PIPE;
    fprintf (stderr, "server.c: pipes not implemented :-)\n");
    return -1;
  } else {
    fprintf (stderr,
	     "%s: `proto' of portconfig `%s' of `%s' doesn't specify a "
	     "valid protocol (tcp,udp,pipe)\n",
	     cfgfile, keyname, varname);
    return -1;
  }

  (*where) = newport;
  return 0;
}

/*
 * Instantiate a server, given a serverdefinition and a sizzle hash
 */
void *
server_instantiate(char *cfgfile,
		   zzz_scm_t hash,
		   struct serverdefinition *sd,
		   char *varname)
{
  char * newserver = NULL;
  int i, e = 0;
  int erroneous = 0;
  zzz_scm_t hashkey;
  zzz_scm_t hashval;
  result_t r;
  long offset;

  /* FIXME: better checking if that is really a hash
   */
  if ( !vector_p (hash) || error_p(hash) ) {
    fprintf(stderr, "%s is not a hash\n", varname);
    return NULL;
  }

  newserver = (char *) xpmalloc (sd->prototype_size);
  /*
   * FIXME: use again
    memcpy (newserver, sd->prototype_start, sd->prototype_size);
  */

  for (i = 0; sd->items[i].type != ITEM_END; i++)
    {
      offset = (char *)sd->items[i].address -
	(char *)sd->prototype_start;

      hashkey = zzz_make_string(sd->items[i].name, -1);
      hashval = zzz_hash_ref(hash, hashkey, zzz_undefined);

      if (hashval == zzz_undefined && !sd->items[i].defaultable)
	{
	  fprintf(stderr,
		  "%s: `%s' does not define a default for `%s' in `%s'\n",
		  cfgfile, sd->name, sd->items[i].name, varname);
	  erroneous = -1;
	  continue;
	}

      switch (sd->items[i].type) {
      case ITEM_INT:
	e = set_int (cfgfile,
		     varname,
		     sd->items[i].name,
		     (int*)(newserver + offset),
		     hashval,
		     *(int*)sd->items[i].address);
	break;

      case ITEM_INTARRAY:
	e = set_intarray (cfgfile,
			  varname,
			  sd->items[i].name,
			  (int **)(newserver + offset),
			  hashval,
			  *(int**)sd->items[i].address);
	break;

      case ITEM_STR:
 	e = set_string (cfgfile,
			varname,
			sd->items[i].name,
			(char**)(newserver + offset),
			hashval,
			*(char**)sd->items[i].address ) ;
	break;

      case ITEM_STRARRAY:
	e = set_stringarray (cfgfile,
			     varname,
			     sd->items[i].name,
			     (char***)(newserver + offset),
			     hashval,
			     *(char***)sd->items[i].address);
	break;

      case ITEM_HASH:
	e = set_hash (cfgfile,
		      varname,
		      sd->items[i].name,
		      (hash_t ***)(newserver + offset),
		      hashval,
		      *(hash_t ***)sd->items[i].address);
	break;

      case ITEM_PORTCFG:
	e = set_port (cfgfile,
		      varname,
		      sd->items[i].name,
		      (struct portcfg **)(newserver + offset),
		      hashval,
		      *(struct portcfg **)sd->items[i].address);
	break;

      default:
	fprintf(stderr, "Inconsistent data in server.c!\n");
	erroneous = -1;
      }

      /* Propagate error, if any */
      if (e < 0)
	erroneous = -1;
      
    }

  return (erroneous ? NULL : newserver);
}


/*
 * Get server settings from a file and instantiate servers as needed.
 */
int
server_load_cfg(char *cfgfile)
{
  char *symname = NULL;
  zzz_scm_t symlist = NULL;
  zzz_scm_t sym = NULL;
  zzz_scm_t symval = NULL;
  unsigned int i, j;
  struct serverdefinition *sd;
  void *newserver_cfg;
  struct server *newserver;
  int erroneous = 0;

  /*
   * If the file was not found, the environment should be ok, anyway
   * Trying to use dedaults...
   */

  /* Scan for variables that could be meant for us */
  for (i = 0; i < vector_len (zzz_symbol_table); i++)
    {
      for (symlist = zzz_vector_get (zzz_symbol_table, i);
	   cons_p (symlist);
	   symlist = cdr (symlist))
	{
	  sym = car (symlist);

	  for (j = 0; NULL != (sd = all_server_definitions[j]); j++)
	    {
	      symname = xpstrdup (string_val (symbol_name (sym)));
	      if ( strncmp(symname, sd->varname, strlen (sd->varname)) == 0 )
		{
		  zzz_get_symbol_value(zzz_toplevel_env, sym, &symval);
		  newserver_cfg = server_instantiate (cfgfile,
						      symval,
						      sd,
						      symname);

		  if ( newserver_cfg != NULL )
		    {
		      newserver =
			(struct server*) xmalloc (sizeof(struct server));
		      newserver->cfg = newserver_cfg;
		      newserver->name = symname;
		      newserver->detect_proto = sd->detect_proto;
		      newserver->connect_socket = sd->connect_socket;
		      newserver->init = sd->init;

		      server_add(newserver);
		    } else {
		      /* FIXME: remove message */
		      fprintf(stderr, " No cfg for %s\n", symname);
		      erroneous = -1;
		    }

		}
	    }
	}
    }

  return erroneous;
}

/*
 * Debug helper funtion to traverse serverdefinitions
 */
#if ENABLE_DEBUG
void
server_show_definitions (void)
{
  int s, i, j;
  char *str = NULL;
  char **strarray = NULL;
  struct serverdefinition *sd;


  for (s = 0; all_server_definitions[s] != NULL; s++)
    {
      sd = all_server_definitions[s];
      printf("[%d] - %s\n", s, sd->name);
      printf("  detect_proto() at %p"
	     "  connect_socket() at %p\n",
	     sd->detect_proto, sd->connect_socket);
      
      if (sd->prototype_start != NULL)
	{
	  printf("  Configblock %d Byte at %p: \n",
		 sd->prototype_size, sd->prototype_start);

	  for (i = 0; sd->items[i].type != ITEM_END; i++)
	    {
	      long offset = (char *)sd->items[i].address -
		(char *)sd->prototype_start;
	      
	      printf("   Variable `%s' at offset %d, %sdefaultable: ",
		     sd->items[i].name, (int)offset,
		     (sd->items[i].defaultable?"":"not "));

	      switch (sd->items[i].type) {
	      case ITEM_INT:
		printf("int\n");
		break;
	      case ITEM_INTARRAY:
		printf("int array\n");
		break;
	      case ITEM_STR:
		printf("string\n");
		break;
	      case ITEM_STRARRAY:
		printf("string array\n");
		break;
	      case ITEM_HASH:
		printf("hash\n");
		break;
	      case ITEM_PORTCFG:
		printf("port configuration\n");
		break;
	      default:
		printf("dunno\n");
	      }
	    }
	} else {
	  printf("  No Configuration option\n");
	}
      
    }
}
#endif


/*
 * Add a server to the list of all servers
 */
static void
server_add(struct server *server)
{
  servers = (struct server**) xprealloc (servers, 
					 (server_instances + 1) *
					 sizeof (struct server*) );

  servers[server_instances++] = server;
}

/*
 * Run the global initializers of all servers. return -1 if some
 * server(class) doesn't feel like running
 */

int
server_global_init(void)
{
  int erroneous = 0;
  int i;
  struct serverdefinition *sd;

  log_printf(LOG_NOTICE, "Running global initialisers\n");

  for ( i = 0; NULL != (sd = all_server_definitions[i]); i++ )
    {
      if (sd->global_init != NULL) {
	if ( sd->global_init() < 0) {
	  erroneous = -1;
	  fprintf (stderr, "Error running global init for `%s'\n", sd->name);
	}
      }
    }

  return erroneous;
}

/*
 * Run the initialisers of all servers, return -1 if some server did't
 * think it's a good idea to run...
 */
int
server_init_all(void)
{
  int errneous = 0;
  int i;

  log_printf(LOG_NOTICE, "Initialising all server instances\n");
  for( i = 0; i < server_instances; i++)
    {
      if ( servers[i]->init != NULL ) {

	if ( servers[i]->init(servers[i]) < 0 ) {
	  errneous = -1;
	  fprintf (stderr, "Error while initialising %s\n", servers[i]->name);
	}
      }

    }

  return errneous;
}
