/*
 * interfaces.c - network interface function implementation
 *
 * Copyright (C) 2000, 2001 Stefan Jahn <stefan@lkcc.org>
 * Copyright (C) 2000 Raimund Jacob <raimi@lkcc.org>
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
 *
 * $Id: interface.c,v 1.2 2001/02/06 17:24:20 ela Exp $
 *
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if ENABLE_IFLIST

#ifndef __MINGW32__
# include <sys/types.h>
# include <sys/socket.h>
# include <sys/ioctl.h>
# include <net/if.h>
# include <netinet/in.h>
# include <arpa/inet.h>
#endif

#if HAVE_UNISTD_H
# include <unistd.h>
#endif

/* Solaris, IRIX */
#if HAVE_SYS_SOCKIO_H
# include <sys/sockio.h>
#endif

#ifdef __MINGW32__
# include <winsock.h>
# include "libserveez/ipdata.h" 
# include "libserveez/iphlpapi.h"
#endif

#endif /* ENABLE_IFLIST */

#include "libserveez/alloc.h"
#include "libserveez/util.h"
#include "libserveez/vector.h"
#include "libserveez/interface.h"

/*
 * The available interface list.
 */
svz_vector_t *svz_interface = NULL;

#if ENABLE_IFLIST

#ifdef __MINGW32__

/* Function pointer definition for use with GetProcAddress. */
typedef int (__stdcall *WsControlProc) (DWORD, DWORD, LPVOID, LPDWORD,
					LPVOID, LPDWORD);
#define WSCTL_TCP_QUERY_INFORMATION 0
#define WSCTL_TCP_SET_INFORMATION   1   

/*
 * The local interface list is requested by some "unrevealed" Winsock API 
 * routine called "WsControl". Works with Win95 and Win98.
 * Otherwise try using the IP Helper API which works with WinNT4x and Win2k.
 */
static WsControlProc WsControl = NULL;
static GetIfTableProc GetIfTable = NULL;
static GetIpAddrTableProc GetIpAddrTable = NULL;

#define NO_METHOD    0
#define WSCTL_METHOD 1
#define IPAPI_METHOD 2

void
svz_interface_collect (void)
{
  int result = 0;
  HMODULE WSockHandle;
  WSADATA WSAData;
  TCP_REQUEST_QUERY_INFORMATION_EX tcpRequestQueryInfoEx;
  DWORD tcpRequestBufSize;
  DWORD entityIdsBufSize;
  TDIEntityID *entityIds;
  DWORD entityCount;
  DWORD i, n, k;
  DWORD ifCount;
  ULONG entityType;
  DWORD entityTypeSize;
  DWORD ifEntrySize;
  IFEntry *ifEntry;
  DWORD ipAddrEntryBufSize;
  IPAddrEntry *ipAddrEntry;
  ULONG ifTableSize, ipTableSize;
  PMIB_IFTABLE ifTable;
  PMIB_IPADDRTABLE ipTable;
  unsigned long addr;
  ifc_entry_t *ifc;

  DWORD Method = NO_METHOD;

  /*
   * Try getting WsControl () from "wsock32.dll" via LoadLibrary
   * and GetProcAddress. Or try the IP Helper API.
   */
  if ((WSockHandle = LoadLibrary ("iphlpapi.dll")) != NULL)
    {
      Method = IPAPI_METHOD;
    }
  else
    {
      if ((WSockHandle = LoadLibrary ("wsock32.dll")) != NULL)
	{
	  WsControl = (WsControlProc) 
	    GetProcAddress (WSockHandle, "WsControl");
	  if (!WsControl)
	    {
	      printf ("GetProcAddress (WsControl): %s\n", SYS_ERROR);
	      FreeLibrary (WSockHandle);
	      return;
	    }
	  Method = WSCTL_METHOD;
	}
      else
	{
	  printf ("LoadLibrary (WSock32.dll): %s\n", SYS_ERROR);
	  return;
	}
    }

  if (Method == WSCTL_METHOD)
    {
      result = WSAStartup (MAKEWORD (1, 1), &WSAData);
      if (result) 
	{
	  printf ("WSAStartup: %s\n", NET_ERROR);
	  FreeLibrary (WSockHandle);
	  return;
	}

      memset (&tcpRequestQueryInfoEx, 0, sizeof (tcpRequestQueryInfoEx));
      tcpRequestQueryInfoEx.ID.toi_entity.tei_entity = GENERIC_ENTITY;
      tcpRequestQueryInfoEx.ID.toi_entity.tei_instance = 0;
      tcpRequestQueryInfoEx.ID.toi_class = INFO_CLASS_GENERIC;
      tcpRequestQueryInfoEx.ID.toi_type = INFO_TYPE_PROVIDER;
      tcpRequestQueryInfoEx.ID.toi_id = ENTITY_LIST_ID;
      tcpRequestBufSize = sizeof (tcpRequestQueryInfoEx);

      /*
       * this probably allocates too much space; not sure if MAX_TDI_ENTITIES
       * represents the max number of entities that can be returned or, if it
       * is the highest entity value that can be defined.
       */
      entityIdsBufSize = MAX_TDI_ENTITIES * sizeof (TDIEntityID);
      entityIds = (TDIEntityID *) calloc (1, entityIdsBufSize);
      
      result = WsControl (IPPROTO_TCP,
			  WSCTL_TCP_QUERY_INFORMATION,
			  &tcpRequestQueryInfoEx,
			  &tcpRequestBufSize, entityIds, &entityIdsBufSize);
      
      if (result) 
	{
	  printf ("WsControl: %s\n", NET_ERROR);
	  WSACleanup ();
	  FreeLibrary (WSockHandle);
	  free (entityIds);
	  return;
	}

      /* ... after the call we compute */
      entityCount = entityIdsBufSize / sizeof (TDIEntityID);
      ifCount = 0;

      /* find out the interface info for the generic interfaces */
      for (i = 0; i < entityCount; i++) 
	{
	  if (entityIds[i].tei_entity == IF_ENTITY) 
	    {
	      ++ifCount;

	      /* see if the interface supports snmp mib-2 info */
	      memset (&tcpRequestQueryInfoEx, 0,
		      sizeof (tcpRequestQueryInfoEx));
	      tcpRequestQueryInfoEx.ID.toi_entity = entityIds[i];
	      tcpRequestQueryInfoEx.ID.toi_class = INFO_CLASS_GENERIC;
	      tcpRequestQueryInfoEx.ID.toi_type = INFO_TYPE_PROVIDER;
	      tcpRequestQueryInfoEx.ID.toi_id = ENTITY_TYPE_ID;

	      entityTypeSize = sizeof (entityType);
	      
	      result = WsControl (IPPROTO_TCP,
				  WSCTL_TCP_QUERY_INFORMATION,
				  &tcpRequestQueryInfoEx,
				  &tcpRequestBufSize,
				  &entityType, &entityTypeSize);
	      
	      if (result) 
		{
		  printf ("WsControl: %s\n", NET_ERROR);
		  WSACleanup ();
		  FreeLibrary (WSockHandle);
		  free (entityIds);
		  return;
		}

	      if (entityType == IF_MIB) 
		{ 
		  /* Supports MIB-2 interface. Get snmp mib-2 info. */
		  tcpRequestQueryInfoEx.ID.toi_class = INFO_CLASS_PROTOCOL;
		  tcpRequestQueryInfoEx.ID.toi_id = IF_MIB_STATS_ID;

		  /*
		   * note: win95 winipcfg use 130 for MAX_IFDESCR_LEN while
		   * ddk\src\network\wshsmple\SMPLETCP.H defines it as 256
		   * we are trying to dup the winipcfg parameters for now
		   */
		  ifEntrySize = sizeof (IFEntry) + 128 + 1;
		  ifEntry = (IFEntry *) calloc (ifEntrySize, 1);
		  
		  result = WsControl (IPPROTO_TCP,
				      WSCTL_TCP_QUERY_INFORMATION,
				      &tcpRequestQueryInfoEx,
				      &tcpRequestBufSize,
				      ifEntry, &ifEntrySize);

		  if (result) 
		    {
		      printf ("WsControl: %s\n", NET_ERROR);
		      WSACleanup ();
		      FreeLibrary (WSockHandle);
		      return;
		    }

		  /* store interface index and description */
		  *(ifEntry->if_descr + ifEntry->if_descrlen) = '\0';
		  svz_interface_add (ifEntry->if_index, ifEntry->if_descr,
				     ifEntry->if_index);
		}
	    }
	}
  
      /* find the ip interfaces */
      for (i = 0; i < entityCount; i++) 
	{
	  if (entityIds[i].tei_entity == CL_NL_ENTITY) 
	    {
	      /* get ip interface info */
	      memset (&tcpRequestQueryInfoEx, 0,
		      sizeof (tcpRequestQueryInfoEx));
	      tcpRequestQueryInfoEx.ID.toi_entity = entityIds[i];
	      tcpRequestQueryInfoEx.ID.toi_class = INFO_CLASS_GENERIC;
	      tcpRequestQueryInfoEx.ID.toi_type = INFO_TYPE_PROVIDER;
	      tcpRequestQueryInfoEx.ID.toi_id = ENTITY_TYPE_ID;

	      entityTypeSize = sizeof (entityType);

	      result = WsControl (IPPROTO_TCP,
				  WSCTL_TCP_QUERY_INFORMATION,
				  &tcpRequestQueryInfoEx,
				  &tcpRequestBufSize,
				  &entityType, &entityTypeSize);

	      if (result) 
		{
		  printf ("WsControl: %s\n", NET_ERROR);
		  WSACleanup ();
		  FreeLibrary (WSockHandle);
		  return;
		}

	      if (entityType == CL_NL_IP) 
		{
		  /* Entity implements IP. Get ip address list. */
		  tcpRequestQueryInfoEx.ID.toi_class = INFO_CLASS_PROTOCOL;
		  tcpRequestQueryInfoEx.ID.toi_id = IP_MIB_ADDRTABLE_ENTRY_ID;

		  ipAddrEntryBufSize = sizeof (IPAddrEntry) * ifCount;
		  ipAddrEntry = 
		    (IPAddrEntry *) calloc (ipAddrEntryBufSize, 1);

		  result = WsControl (IPPROTO_TCP,
				      WSCTL_TCP_QUERY_INFORMATION,
				      &tcpRequestQueryInfoEx,
				      &tcpRequestBufSize,
				      ipAddrEntry, &ipAddrEntryBufSize);

		  if (result) 
		    {
		      printf ("WsControl: %s\n", NET_ERROR);
		      WSACleanup ();
		      FreeLibrary (WSockHandle);
		      return;
		    }
		
		  /* find ip address list and interface description */
		  for (n = 0; n < ifCount; n++) 
		    {
		      memcpy (&addr, &ipAddrEntry[n].iae_addr, sizeof (addr));

		      for (k = 0; k < svz_vector_length (svz_interface); k++)
			{
			  ifc = svz_vector_get (svz_interface, k);
			  if (ifc->index == ipAddrEntry[n].iae_index)
			    ifc->ipaddr = addr;
		    }
		}
	    }
	}

      WSACleanup ();
      FreeLibrary (WSockHandle);
    }

  /* this is for WinNT... */
  else if (Method == IPAPI_METHOD)
    {
      /* Use of the IPHelper-API here. */
      GetIfTable = (GetIfTableProc) 
	GetProcAddress (WSockHandle, "GetIfTable");
      if (!GetIfTable)
	{
	  printf ("GetProcAddress (GetIfTable): %s\n", SYS_ERROR);
	  FreeLibrary (WSockHandle);
	  return;
	}

      GetIpAddrTable = (GetIpAddrTableProc) 
	GetProcAddress (WSockHandle, "GetIpAddrTable");
      if (!GetIpAddrTable)
	{
	  printf ("GetProcAddress (GetIpAddrTable): %s\n", SYS_ERROR);
	  FreeLibrary (WSockHandle);
	  return;
	}

      ifTableSize = sizeof (MIB_IFTABLE);
      ifTable = (PMIB_IFTABLE) svz_malloc (ifTableSize);
      GetIfTable (ifTable, &ifTableSize, FALSE);
      ifTable = (PMIB_IFTABLE) svz_realloc (ifTable, ifTableSize);
      if (GetIfTable (ifTable, &ifTableSize, FALSE) != NO_ERROR)
	{
	  printf ("GetIfTable: %s\n", SYS_ERROR);
	  FreeLibrary (WSockHandle);
	  svz_free (ifTable);
	  return;
	}
  
      ipTableSize = sizeof (MIB_IPADDRTABLE);
      ipTable = (PMIB_IPADDRTABLE) svz_malloc (ipTableSize);
      GetIpAddrTable (ipTable, &ipTableSize, FALSE);
      ipTable = (PMIB_IPADDRTABLE) svz_realloc (ipTable, ipTableSize);
      if (GetIpAddrTable (ipTable, &ipTableSize, FALSE) != NO_ERROR)
	{
	  printf ("GetIpAddrTable: %s\n", SYS_ERROR);
	  FreeLibrary (WSockHandle);
	  svz_free (ipTable);
	  svz_free (ifTable);
	  return;
	}
      
      for (n = 0; n < ipTable->dwNumEntries; n++)
	{
	  for (i = 0; i < ifTable->dwNumEntries; i++)
	    {
	      if (ifTable->table[i].dwIndex == ipTable->table[n].dwIndex)
		{
		  ifTable->table[i].bDescr[ifTable->table[i].dwDescrLen] = 0;
		  svz_interface_add (ipTable->table[n].dwIndex, 
				     ifTable->table[i].bDescr,
				     ipTable->table[n].dwAddr);
		  break;
		}
	    }
	  if (i == ifTable->dwNumEntries)
	    {
	      svz_interface_add (ipTable->table[n].dwIndex, NULL,
				 ipTable->table[n].dwAddr);
	    }
	}

      svz_free (ipTable);
      svz_free (ifTable);
      FreeLibrary (WSockHandle);
    }
  else
    {
      printf ("Neither IPHlpApi.dll nor WSock32.WsControl found...\n");
    }
}

#else /* not __MINGW32__ */

void
svz_interface_collect (void)
{
  int numreqs = 16;
  struct ifconf ifc;
  struct ifreq *ifr;
  struct ifreq ifr2;
  int n;
  int fd;

  /* Get a socket out of the Internet Address Family. */
  if ((fd = socket (AF_INET, SOCK_STREAM, 0)) < 0) 
    {
      perror ("socket");
      return;
    }

  /* Collect information. */
  ifc.ifc_buf = NULL;
  for (;;) 
    {
      ifc.ifc_len = sizeof (struct ifreq) * numreqs;
      ifc.ifc_buf = svz_realloc (ifc.ifc_buf, ifc.ifc_len);

      /*
       * On newer AIXes we cannot use SIOCGICONF anymore, although it is
       * present. The data structure returned is bogus. Using OSIOCGIFCONF.
       */
#if defined (OSIOCGIFCONF)
      if (ioctl (fd, OSIOCGIFCONF, &ifc) < 0)
	{
	  perror ("OSIOCGIFCONF");
	  close (fd);
	  svz_free (ifc.ifc_buf);
	  return;	  
	}
#else /* OSIOCGIFCONF */
      if (ioctl (fd, SIOCGIFCONF, &ifc) < 0) 
	{
	  perror ("SIOCGIFCONF");
	  close (fd);
	  svz_free (ifc.ifc_buf);
	  return;
	}
#endif /* OSIOCGIFCONF */

      if ((unsigned) ifc.ifc_len == sizeof (struct ifreq) * numreqs) 
	{
	  /* Assume it overflowed and try again. */
	  numreqs += 10;
	  continue;
	}
      break;
    }

  ifr = ifc.ifc_req;
  for (n = 0; n < ifc.ifc_len; n += sizeof (struct ifreq), ifr++)
    {
      /*
       * On AIX (and perhaps others) you get interfaces that are not AF_INET
       * from the first `ioctl ()', so filter here again.
       */
#ifdef __FreeBSD__
      if ((ifr->ifr_phys & 0xFFFF0000) == 0)
#else
      if (ifr->ifr_addr.sa_family != AF_INET)
#endif
	continue;

      strcpy (ifr2.ifr_name, ifr->ifr_name);
      ifr2.ifr_addr.sa_family = AF_INET;
      if (ioctl (fd, SIOCGIFADDR, &ifr2) == 0)
	{
	  static int index = 0;

	  /* 
	   * The following cast looks bogus. ifr2.ifr_addr is a
	   * (struct sockaddr), but we know that we deal with a 
	   * (struct sockaddr_in) here. Since you cannot cast structures
	   * in C, I cast addresses just to get a (struct sockaddr_in) in 
	   * the end ...
	   */
#ifdef ifr_ifindex
	  index = ifr->ifr_ifindex;
#else
	  index++;
#endif
	  svz_interface_add (index, ifr->ifr_name, 
			     (*(struct sockaddr_in *) 
			      &ifr2.ifr_addr).sin_addr.s_addr);
	}
    }
  
  close (fd);
  svz_free (ifc.ifc_buf);
}

#endif /* not __MINGW32__ */

#else /* not ENABLE_IFLIST */

void
svz_interface_collect (void)
{
  printf ("\n"
	  "Sorry, the list of local interfaces is not available. If you\n"
	  "know how to get such a list on your OS, please contact\n"
	  "Raimund Jacob <raimi@lkcc.org>. Thanks.\n\n");
}

#endif /* not ENABLE_IFLIST */

/*
 * Print the text representation of all the network interfaces.
 */
void
svz_interface_list (void)
{
  unsigned long n;
  ifc_entry_t *ifc;

  printf ("--- list of local interfaces you can start ip services on ---\n");

  for (n = 0; n < svz_vector_length (svz_interface); n++)
    {
      ifc = svz_vector_get (svz_interface, n);

      /* interface with description */
      if (ifc->description)
	{
	  printf ("%40s: %s\n", ifc->description, 
		  util_inet_ntoa (ifc->ipaddr));
	}
      else
	{
	  /* interface with interface # only */
	  printf ("%31s%09lu: %s\n", "interface # ",
		  ifc->index, util_inet_ntoa (ifc->ipaddr));
	}
    }
}

/*
 * Add a network interface to the current list of known interfaces. Drop
 * duplicate entries.
 */
int
svz_interface_add (int index, char *desc, unsigned long addr)
{
  unsigned long n;
  ifc_entry_t *ifc;

  /* Check if there is such an interface already. */
  if (svz_interface == NULL)
    {
      svz_interface = svz_vector_create (sizeof (ifc_entry_t));
    }
  else
    {
      for (n = 0; n < svz_vector_length (svz_interface); n++)
	{
	  ifc = svz_vector_get (svz_interface, n);
	  if (ifc->ipaddr == addr)
	    return -1;
	}
    }

  /* Actually add this interface. */
  ifc = svz_malloc (sizeof (ifc_entry_t));
  ifc->index = index;
  ifc->ipaddr = addr;
  ifc->description = svz_strdup (desc);
  svz_vector_add (svz_interface, ifc);
  svz_free (ifc);
  return 0;
}

/*
 * Free the network interface list.
 */
int
svz_interface_free (void)
{
  unsigned long n;
  ifc_entry_t *ifc;

  if (svz_interface)
    {
      for (n = 0; n < svz_vector_length (svz_interface); n++)
	{
	  ifc = svz_vector_get (svz_interface, n);
	  if (ifc->description)
	    svz_free (ifc->description);
	}
      svz_vector_destroy(svz_interface);
      svz_interface = NULL;
      return 0;
    }
  return -1;
}
