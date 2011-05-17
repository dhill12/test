/*
*         OpenPBS (Portable Batch System) v2.3 Software License
*
* Copyright (c) 1999-2000 Veridian Information Solutions, Inc.
* All rights reserved.
*
* ---------------------------------------------------------------------------
* For a license to use or redistribute the OpenPBS software under conditions
* other than those described below, or to purchase support for this software,
* please contact Veridian Systems, PBS Products Department ("Licensor") at:
*
*    www.OpenPBS.org  +1 650 967-4675                  sales@OpenPBS.org
*                        877 902-4PBS (US toll-free)
* ---------------------------------------------------------------------------
*
* This license covers use of the OpenPBS v2.3 software (the "Software") at
* your site or location, and, for certain users, redistribution of the
* Software to other sites and locations.  Use and redistribution of
* OpenPBS v2.3 in source and binary forms, with or without modification,
* are permitted provided that all of the following conditions are met.
* After December 31, 2001, only conditions 3-6 must be met:
*
* 1. Commercial and/or non-commercial use of the Software is permitted
*    provided a current software registration is on file at www.OpenPBS.org.
*    If use of this software contributes to a publication, product, or
*    service, proper attribution must be given; see www.OpenPBS.org/credit.html
*
* 2. Redistribution in any form is only permitted for non-commercial,
*    non-profit purposes.  There can be no charge for the Software or any
*    software incorporating the Software.  Further, there can be no
*    expectation of revenue generated as a consequence of redistributing
*    the Software.
*
* 3. Any Redistribution of source code must retain the above copyright notice
*    and the acknowledgment contained in paragraph 6, this list of conditions
*    and the disclaimer contained in paragraph 7.
*
* 4. Any Redistribution in binary form must reproduce the above copyright
*    notice and the acknowledgment contained in paragraph 6, this list of
*    conditions and the disclaimer contained in paragraph 7 in the
*    documentation and/or other materials provided with the distribution.
*
* 5. Redistributions in any form must be accompanied by information on how to
*    obtain complete source code for the OpenPBS software and any
*    modifications and/or additions to the OpenPBS software.  The source code
*    must either be included in the distribution or be available for no more
*    than the cost of distribution plus a nominal fee, and all modifications
*    and additions to the Software must be freely redistributable by any party
*    (including Licensor) without restriction.
*
* 6. All advertising materials mentioning features or use of the Software must
*    display the following acknowledgment:
*
*     "This product includes software developed by NASA Ames Research Center,
*     Lawrence Livermore National Laboratory, and Veridian Information
*     Solutions, Inc.
*     Visit www.OpenPBS.org for OpenPBS software support,
*     products, and information."
*
* 7. DISCLAIMER OF WARRANTY
*
* THIS SOFTWARE IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND. ANY EXPRESS
* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
* OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT
* ARE EXPRESSLY DISCLAIMED.
*
* IN NO EVENT SHALL VERIDIAN CORPORATION, ITS AFFILIATED COMPANIES, OR THE
* U.S. GOVERNMENT OR ANY OF ITS AGENCIES BE LIABLE FOR ANY DIRECT OR INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
* OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
* NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
* EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
* This license will be governed by the laws of the Commonwealth of Virginia,
* without reference to its choice of law rules.
*/
#include <pbs_config.h>   /* the master config generated by configure */

#include <limits.h>
#include <assert.h>
#include <ctype.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pbs_ifl.h"
#include "list_link.h"
#include "attribute.h"
#include "pbs_error.h"

/*
 * This file contains functions for manipulating attributes of type
 * time: [[hh:]mm:]ss[.sss]
 *
 * Each set has functions for:
 * Decoding the value string to the machine representation.
 * Encoding the internal attribute to external form
 * Setting the value by =, + or - operators.
 * Comparing a (decoded) value with the attribute value.
 *
 * Some or all of the functions for an attribute type may be shared with
 * other attribute types.
 *
 * The prototypes are declared in "attribute.h"
 *
 * --------------------------------------------------
 * The Set of Attribute Functions for attributes with
 * value type "long"
 * --------------------------------------------------
 */


/*
 * decode_time - decode time into into attribute structure of type ATR_TYPE_LONG
 *
 * Returns: 0 if ok
 *  >0 error number if error
 *  *patr elements set
 */

#define PBS_MAX_TIME (LONG_MAX - 1)

int decode_time(

  attribute *patr,  /* I/O (modified) */
  char      *name,  /* I - attribute name (not used) */
  char      *rescn, /* I - resource name (not used) */
  char      *val,   /* I - attribute value */
  int        perm)  /* only used for resources */

  {
  int   i;
  char  msec[4];
  int   ncolon = 0;
  int   use_days = 0;
  int   days = 0;
  char *pc;
  long  rv = 0;
  char *workval;
  char *workvalsv;

  if ((val == NULL) || (strlen(val) == 0))
    {
    patr->at_flags = (patr->at_flags & ~ATR_VFLAG_SET) | ATR_VFLAG_MODIFY;

    patr->at_val.at_long = 0;

    /* SUCCESS */

    return(0);
    }

  /* FORMAT:  [DD]:HH:MM:SS[.MS] */

  workval = strdup(val);

  workvalsv = workval;

  if (workvalsv == NULL)
    {
    /* FAILURE - cannot alloc memory */

    goto badval;
    }

  for (i = 0;i < 3;++i)
    msec[i] = '0';

  msec[i] = '\0';

  for (pc = workval;*pc;++pc)
    {
    if (*pc == ':')
      {
      if (++ncolon > 3)
        goto badval;

      /* are days specified? */
      if (ncolon > 2)
        use_days = 1;
      }
    }

  for (pc = workval;*pc;++pc)
    {
    if (*pc == ':')
      {

      *pc = '\0';

      if (use_days)
        {
        days = atoi(workval);
        use_days = 0;
        }
      else
        {
        rv = (rv * 60) + atoi(workval);
        }

      workval = pc + 1;

      }
    else if (*pc == '.')
      {
      *pc++ = '\0';

      for (i = 0; (i < 3) && *pc; ++i)
        msec[i] = *pc++;

      break;
      }
    else if (!isdigit((int)*pc))
      {
      goto badval; /* bad value */
      }
    }

  rv = (rv * 60) + atoi(workval);
  
  if (days > 0)
   rv = rv + (days * 24 * 3600);

  if (rv > PBS_MAX_TIME)
    goto badval;

  if (atoi(msec) >= 500)
    rv++;

  patr->at_val.at_long = rv;

  patr->at_flags |= ATR_VFLAG_SET | ATR_VFLAG_MODIFY;

  free(workvalsv);

  /* SUCCESS */

  return(0);

badval:

  free(workvalsv);

  return(PBSE_BADATVAL);
  }  /* END decode_time() */





/*
 * encode_time - encode attribute of type long into attr_extern
 * with value in form of [[hh:]mm:]ss
 *
 * Returns: >0 if ok
 *   =0 if no value, no attrlist link added
 *   <0 if error
 */
/*ARGSUSED*/

/* NOTE:  if phead not specified, report output via atname (minsize=1024) */

#define CVNBUFSZ 21

int encode_time(

  attribute  *attr,   /* ptr to attribute (value in attr->at_val.at_long) */
  tlist_head *phead,  /* head of attrlist list (optional) */
  char       *atname, /* attribute name */
  char       *rsname, /* resource name (optional) */
  int         mode,   /* encode mode (not used) */
  int         perm)  /* only used for resources */

  {
  size_t  ct;
  char   cvnbuf[CVNBUFSZ];
  int    hr;
  int   min;
  long   n;
  svrattrl *pal;
  int   sec;
  char  *pv;

  if (attr == NULL)
    {
    /* FAILURE */

    return(-1);
    }

  if (!(attr->at_flags & ATR_VFLAG_SET))
    {
    return(0);
    }

  n   = attr->at_val.at_long;

  hr  = n / 3600;
  n   = n % 3600;
  min = n / 60;
  n   = n % 60;
  sec = n;


  sprintf(cvnbuf, "%02d:%02d:%02d",
          hr, min, sec);

  pv = cvnbuf;

  pv += strlen(pv);

  ct = strlen(cvnbuf) + 1;

  if (phead != NULL)
    {
    pal = attrlist_create(atname, rsname, ct);

    if (pal == NULL)
      {
      /* FAILURE */

      return(-1);
      }

    memcpy(pal->al_value, cvnbuf, ct);

    pal->al_flags = attr->at_flags;

    append_link(phead, &pal->al_link, pal);
    }
  else
    {
    strcpy(atname, cvnbuf);
    }

  /* SUCCESS */

  return(1);
  }

/*
 * set_time  - use the function set_l()
 *
 * comp_time - use the funttion comp_l()
 *
 * free_l - use free_null to (not) free space
 */
