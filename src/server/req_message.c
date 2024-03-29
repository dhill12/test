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
/*
 * req_message.c - functions dealing with sending a message
 *       to a running job.
 */

#include <pbs_config.h>   /* the master config generated by configure */

#include <stdio.h>
#include <signal.h>
#include <pthread.h>
#include "libpbs.h"
#include "server_limits.h"
#include "list_link.h"
#include "attribute.h"
#include "server.h"
#include "credential.h"
#include "batch_request.h"
#include "pbs_job.h"
#include "work_task.h"
#include "pbs_error.h"
#include "log.h"
#include "../lib/Liblog/log_event.h"
#include "svrfunc.h"
#include "ji_mutex.h"


/* Private Function local to this file */

static void post_message_req(struct work_task *);

/* Global Data Items: */

extern int   pbs_mom_port;
extern char *msg_messagejob;
extern int   LOGLEVEL;

extern job  *chk_job_request(char *, struct batch_request *);
int copy_batchrequest(struct batch_request **newreq, struct batch_request *preq, int type, int jobid);

/*
 * req_messagejob - service the Message Job Request
 *
 * This request sends (via MOM) a message to a running job.
 */

void *req_messagejob(
    
  void *vp)

  {
  struct batch_request *preq = (struct batch_request *)vp;
  job                  *pjob;
  int                   rc;
  struct batch_request *dup_req = NULL;

  if ((pjob = chk_job_request(preq->rq_ind.rq_message.rq_jid, preq)) == NULL)
    return(NULL);

  /* the job must be running */

  if (pjob->ji_qs.ji_state != JOB_STATE_RUNNING)
    {
    req_reject(PBSE_BADSTATE, 0, preq, NULL, NULL);

    unlock_ji_mutex(pjob, __func__, "1", LOGLEVEL);
    
    return(NULL);
    }

  if ((rc = copy_batchrequest(&dup_req, preq, 0, -1)) != 0)
    {
    req_reject(PBSE_MEM_MALLOC, 0, preq, NULL, NULL);
    }
  /* pass the request on to MOM */
  /* The dup_req is freed in relay_to_mom (failure)
   * or in issue_Drequest (success) */
  else if ((rc = relay_to_mom(&pjob, dup_req, post_message_req)) != 0)
    req_reject(rc, 0, preq, NULL, NULL); /* unable to get to MOM */
  else
    free_br(preq);

  /* After MOM acts and replies to us, we pick up in post_message_req() */
  if (pjob != NULL)
    unlock_ji_mutex(pjob, __func__, "2", LOGLEVEL);

  return(NULL);
  } /* END req_messagejob() */

/*
 * post_message_req - complete a Message Job Request
 */

static void post_message_req(
    
  struct work_task *pwt)

  {
  struct batch_request *preq;
  char                  log_buf[LOCAL_LOG_BUF_SIZE];

  svr_disconnect(pwt->wt_event); /* close connection to MOM */

  preq = get_remove_batch_request(pwt->wt_parm1);

  free(pwt->wt_mutex);
  free(pwt);

  /* preq has been hadnled previously */
  if (preq == NULL)
    return;

  preq->rq_conn = preq->rq_orgconn;  /* restore socket to client */

  sprintf(log_buf, msg_messagejob, preq->rq_reply.brp_code);
  log_event(PBSEVENT_JOB, PBS_EVENTCLASS_JOB, preq->rq_ind.rq_message.rq_jid, log_buf);

  if (preq->rq_reply.brp_code)
    req_reject(preq->rq_reply.brp_code, 0, preq, NULL, NULL);
  else
    reply_ack(preq);
  } /* END post_message_req() */

