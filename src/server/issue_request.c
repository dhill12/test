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
 * issue_request.c
 *
 * Function to allow the server to issue requests to to other batch
 * servers, scheduler, MOM, or even itself.
 *
 * The encoding of the data takes place in other routines, see
 * the API routines in libpbs.a
 */
#include <pbs_config.h>   /* the master config generated by configure */
#include "issue_request.h"

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include "dis.h"
#include "libpbs.h"
#include "server_limits.h"
#include "list_link.h"
#include "attribute.h"
#include "credential.h"
#include "batch_request.h"
#include "log.h"
#include "../lib/Liblog/pbs_log.h"
#include "../lib/Liblog/log_event.h"
#include "work_task.h"
#include "net_connect.h"
#include "svrfunc.h"
#include "pbs_nodes.h"
#include "pbs_job.h"
#include "server.h"
#include <stdint.h>
#include "../lib/Libutils/u_lock_ctl.h" /* lock_node, unlock_node */
#include "process_request.h" /* dispatch_request */
#include "svr_connect.h" /* svr_disconnect_sock */
#include "node_manager.h" /* tfind_addr */
#include "ji_mutex.h"


/* Global Data Items: */

extern struct server server;
extern char *server_name;
extern struct connect_handle connection[];
extern char     *msg_daemonname;
extern all_tasks task_list_event;
extern char *msg_daemonname;
extern char *msg_issuebad;
extern char     *msg_norelytomom;
extern char *msg_err_malloc;
extern unsigned int pbs_mom_port;
extern unsigned int pbs_server_port_dis;

extern struct  connection svr_conn[];

extern int       LOGLEVEL;

int issue_to_svr(char *, struct batch_request *, void (*f)(struct work_task *));

/*
 * relay_to_mom - relay a (typically existing) batch_request to MOM
 *
 * Make connection to MOM and issue the request.  Called with
 * network address rather than name to save look-ups.
 *
 * Unlike issue_to_svr(), a failed connection is not retried.
 * The calling routine typically handles this problem.
 *
 * @see XXX() - routine which processes request on the MOM
 */

int relay_to_mom(

  job                   **pjob_ptr,
  struct batch_request   *request, /* the request to send */
  void                  (*func)(struct work_task *))

  {
  int             handle; /* a client style connection handle */
  int             rc;
  int             local_errno = 0;
  pbs_net_t       addr;
  unsigned short  port;
  job            *pjob = *pjob_ptr;
  char            jobid[PBS_MAXSVRJOBID + 1];
  char *job_momname = NULL;

  struct pbsnode *node;
  char            log_buf[LOCAL_LOG_BUF_SIZE];

  /* if MOM is down don't try to connect */
  addr = pjob->ji_qs.ji_un.ji_exect.ji_momaddr;
  port = pjob->ji_qs.ji_un.ji_exect.ji_momport;
  job_momname = strdup(pjob->ji_wattr[JOB_ATR_exec_host].at_val.at_str);
  if (job_momname == NULL)
    return PBSE_MEM_MALLOC;

  if ((node = tfind_addr(addr, port, job_momname)) == NULL)
    {
    free(job_momname);
    free_br(request);
    return(PBSE_NORELYMOM);
    }
  free(job_momname);

  if ((node != NULL) &&
      (node->nd_state & INUSE_DOWN))
    {
    unlock_node(node, __func__, "no rely mom", LOGLEVEL);
       free_br(request);
    return(PBSE_NORELYMOM);
    }

  if (LOGLEVEL >= 7)
    {
    char *tmp = netaddr_pbs_net_t(pjob->ji_qs.ji_un.ji_exect.ji_momaddr);
    sprintf(log_buf, "momaddr=%s",tmp);

    log_record(PBSEVENT_SCHED, PBS_EVENTCLASS_REQUEST, __func__, log_buf);

    free(tmp);
    }

  unlock_node(node, __func__, "after svr_connect", LOGLEVEL);
  handle = svr_connect(
           pjob->ji_qs.ji_un.ji_exect.ji_momaddr,
           pjob->ji_qs.ji_un.ji_exect.ji_momport,
           &local_errno,
           NULL,
           NULL,
           ToServerDIS);
    

  if (handle < 0)
    {
    log_event(PBSEVENT_ERROR,PBS_EVENTCLASS_REQUEST,"",msg_norelytomom);

    free_br(request);
    return(PBSE_NORELYMOM);
    }

  strcpy(jobid, pjob->ji_qs.ji_jobid);
  unlock_ji_mutex(pjob, __func__, NULL, LOGLEVEL);

  request->rq_orgconn = request->rq_conn; /* save client socket */

  rc = issue_Drequest(handle, request, func, NULL);

  *pjob_ptr = svr_find_job(jobid, TRUE);

  return(rc);
  }  /* END relay_to_mom() */





/*
 * reissue_to_svr - recall issue_to_svr() after a delay to retry sending
 * a request that failed for a temporary reason
 */

static void reissue_to_svr(

  struct work_task *pwt)

  {
  time_t         time_now = time(NULL);
  char          *br_id = pwt->wt_parm1;
  batch_request *preq = get_remove_batch_request(br_id);

  /* if not timed-out, retry send to remote server */
  if (preq != NULL)
    {
    if (((time_now - preq->rq_time) > PBS_NET_RETRY_LIMIT) ||
        (issue_to_svr(preq->rq_host, preq, pwt->wt_parmfunc) != PBSE_NONE))
      {
      /* either timed-out or got hard error, tell post-function  */
      
      pwt->wt_aux = -1; /* seen as error by post function  */
      pwt->wt_event = -1; /* seen as connection by post func */
      
      ((void (*)())pwt->wt_parmfunc)(pwt);
      }
    }

  free(pwt->wt_mutex);
  free(pwt);
  }  /* END reissue_to_svr() */



/*
 * issue_to_svr - issue a batch request to a server
 * This function parses the server name, looks up its host address,
 * makes a connection and called issue_request (above) to send
 * the request.
 *
 * Returns:  0 on success,
 *   -1 on permanent error (no such host)
 *
 * On temporary error, establish a work_task to retry after a delay.
 */

int issue_to_svr(

  char                 *servern,                  /* I */
  struct batch_request *preq,                     /* I */
  void (*replyfunc)    (struct work_task *))      /* I */

  {
  int               rc = PBSE_NONE;
  int               do_retry = 0;
  int               handle;
  int               my_err = 0;
  pbs_net_t         svraddr;
  char             *svrname;
  unsigned int      port = pbs_server_port_dis;

  struct work_task *pwt;
  time_t            time_now = time(NULL);

  snprintf(preq->rq_host, sizeof(preq->rq_host), "%s", servern);

  preq->rq_fromsvr = 1;
  preq->rq_perm = ATR_DFLAG_MGRD | ATR_DFLAG_MGWR | ATR_DFLAG_SvWR;

  svrname = parse_servername(servern, &port); 
  svraddr = get_hostaddr(&my_err,svrname);

  free(svrname);

  if (svraddr == (pbs_net_t)0)
    {
    if (my_err == PBS_NET_RC_RETRY)
      {
      /* Non fatal error - retry */

      do_retry = 1;
      }
    }
  else
    {
    handle = svr_connect(svraddr, port, &my_err, NULL, NULL, ToServerDIS);

    if (handle >= 0)
      {
      if (((rc = issue_Drequest(handle, preq, replyfunc, NULL)) == PBSE_NONE) &&
          (handle != PBS_LOCAL_CONNECTION))
        {
        /* preq is already freed if handle == PBS_LOCAL_CONNECTION - a reply 
         * has always been sent */
        rc = preq->rq_reply.brp_code;
        }

      return(rc);
      }
    else if (handle == PBS_NET_RC_RETRY)
      {
      do_retry = 1;
      }
    }

  /* if reached here, it didn`t go, do we retry? */

  if (do_retry)
    {
    if (preq->rq_id == NULL)
      get_batch_request_id(preq);

    pwt = set_task(WORK_Timed, (long)(time_now + PBS_NET_RETRY_TIME), reissue_to_svr, preq->rq_id, TRUE);

    pwt->wt_parmfunc = replyfunc;

    pthread_mutex_unlock(pwt->wt_mutex);

    return(PBSE_NONE);
    }

  /* FAILURE */

  return(PBSE_INTERNAL);
  }  /* END issue_to_svr() */




/*
 * release_req - this is the basic function to call after we are
 * through with an internally generated request to another server.
 * It frees the request structure and closes the connection (handle).
 *
 * In the work task entry, wt_event is the connection handle and
 * wt_parm1 is a pointer to the request structure.
 *
 * THIS SHOULD NOT BE USED IF AN EXTERNAL (CLIENT) REQUEST WAS "relayed".
 * The request/reply structure is still needed to reply to the client.
 */

void release_req(

  struct work_task *pwt)

  {
  batch_request *preq;
  char          *br_id = pwt->wt_parm1;

  if ((preq = get_remove_batch_request(br_id)) != NULL)
    free_br(preq);

  if (pwt->wt_event != -1)
    svr_disconnect(pwt->wt_event);

  free(pwt->wt_mutex);
  free(pwt);
  } /* END release_req() */




/* The following functions exist for the new DIS protocol */


/*
 * issue_request - issue a batch request to another server or to a MOM
 * or even to ourself!
 *
 * If the request is meant for this very server, then
 *  Set up work-task of type WORK_Deferred_Local with a dummy
 *  connection handle (PBS_LOCAL_CONNECTION).
 *
 *  Dispatch the request to be processed.  [reply_send() will
 *  dispatch the reply via the work task entry.]
 *
 * If the request is to another server/MOM, then
 *  Set up work-task of type WORK_Deferred_Reply with the
 *  connection handle as the event.
 *
 *  Encode and send the request.
 *
 *  When the reply is ready,  process_reply() will decode it and
 *  dispatch the work task.
 *
 * IT IS UP TO THE FUNCTION DISPATCHED BY THE WORK TASK TO CLOSE THE
 * CONNECTION (connection handle not socket) and FREE THE REQUEST
 * STRUCTURE.  The connection (non-negative if open) is in wt_event
 * and the pointer to the request structure is in wt_parm1.
 * If issue_Drequest fails before creating the work task then the 
 * batch request needs to be freed. The bottom line is that the 
 * batch request will always be freed after a call to issue_Drequest
 * either within issue_Drequest or in the work_task.
 */

int issue_Drequest(

  int                    conn,
  struct batch_request  *request,
  void                 (*func) (struct work_task *),
  struct work_task     **ppwt)

  {
  struct attropl   *patrl;

  struct work_task *ptask = NULL;

  struct svrattrl  *psvratl;
  int               rc = PBSE_NONE;
  int               tmp_rc = PBSE_NONE;
  int               sock = 0;
  enum work_type    wt;
  char              log_buf[LOCAL_LOG_BUF_SIZE];
  struct tcp_chan  *chan = NULL;

  if (conn == PBS_LOCAL_CONNECTION)
    {
    wt = WORK_Deferred_Local;

    request->rq_conn = PBS_LOCAL_CONNECTION;
    }
  else
    {
    pthread_mutex_lock(connection[conn].ch_mutex);

    sock = connection[conn].ch_socket;

    pthread_mutex_unlock(connection[conn].ch_mutex);

    request->rq_conn = sock;

    wt = WORK_Deferred_Reply;

    if ((chan = DIS_tcp_setup(sock)) == NULL)
      {
      log_err(PBSE_MEM_MALLOC, __func__,
          "Could not allocate memory for socket buffer");
      close_conn(sock, FALSE);
      free_br(request);
      return(PBSE_MEM_MALLOC);
      }
    }

  if (conn == PBS_LOCAL_CONNECTION)
    {
    /* the request should be issued to ourself */
    rc = dispatch_request(PBS_LOCAL_CONNECTION, request);

    return(rc);
    }

  if (func != NULL)
    {
    if (request->rq_id == NULL)
      get_batch_request_id(request);

    ptask = set_task(wt, (long)conn, func, request->rq_id, FALSE);

    if (ppwt != NULL)
      *ppwt = ptask;

    if (ptask == NULL)
      {
      log_err(errno, __func__, "could not set_task");
      close_conn(sock, FALSE);

      if (chan != NULL)
        DIS_tcp_cleanup(chan);
      free_br(request);
      return(PBSE_MEM_MALLOC);
      }
    }

  /* the request is bound to another server, encode/send the request */

  switch (request->rq_type)
    {
#ifndef PBS_MOM

    case PBS_BATCH_DeleteJob:

      rc = PBSD_mgr_put(
             conn,
             PBS_BATCH_DeleteJob,
             MGR_CMD_DELETE,
             MGR_OBJ_JOB,
             request->rq_ind.rq_delete.rq_objname,
             NULL,
             NULL);

      break;

    case PBS_BATCH_HoldJob:

      attrl_fixlink(&request->rq_ind.rq_hold.rq_orig.rq_attr);

      psvratl = (struct svrattrl *)GET_NEXT(request->rq_ind.rq_hold.rq_orig.rq_attr);

      patrl = &psvratl->al_atopl;

      rc = PBSD_mgr_put(
             conn,
             PBS_BATCH_HoldJob,
             MGR_CMD_SET,
             MGR_OBJ_JOB,
             request->rq_ind.rq_hold.rq_orig.rq_objname,
             patrl,
             NULL);

      break;

    case PBS_BATCH_CheckpointJob:

      rc = PBSD_mgr_put(
             conn,
             PBS_BATCH_CheckpointJob,
             MGR_CMD_SET,
             MGR_OBJ_JOB,
             request->rq_ind.rq_hold.rq_orig.rq_objname,
             NULL,
             NULL);

      break;

    case PBS_BATCH_GpuCtrl:

      rc = PBSD_gpu_put(
             conn,
             request->rq_ind.rq_gpuctrl.rq_momnode,
             request->rq_ind.rq_gpuctrl.rq_gpuid,
             request->rq_ind.rq_gpuctrl.rq_gpumode,
             request->rq_ind.rq_gpuctrl.rq_reset_perm,
             request->rq_ind.rq_gpuctrl.rq_reset_vol,
             NULL);

      break;

    case PBS_BATCH_MessJob:

      rc = PBSD_msg_put(
             conn,
             request->rq_ind.rq_message.rq_jid,
             request->rq_ind.rq_message.rq_file,
             request->rq_ind.rq_message.rq_text,
             NULL);

      break;

    case PBS_BATCH_ModifyJob:

    case PBS_BATCH_AsyModifyJob:

      attrl_fixlink(&request->rq_ind.rq_modify.rq_attr);

      patrl = (struct attropl *) & ((struct svrattrl *)GET_NEXT(
                                      request->rq_ind.rq_modify.rq_attr))->al_atopl;

      rc = PBSD_mgr_put(
             conn,
             request->rq_type,
             MGR_CMD_SET,
             MGR_OBJ_JOB,
             request->rq_ind.rq_modify.rq_objname,
             patrl,
             NULL);

      break;

    case PBS_BATCH_Rerun:

      if ((rc = encode_DIS_ReqHdr(chan, PBS_BATCH_Rerun, msg_daemonname)))
        break;

      if ((rc = encode_DIS_JobId(chan, request->rq_ind.rq_rerun)))
        break;

      if ((rc = encode_DIS_ReqExtend(chan, 0)))
        break;

      rc = DIS_tcp_wflush(chan);

      break;

    case PBS_BATCH_RegistDep:

      if ((rc = encode_DIS_ReqHdr(chan, PBS_BATCH_RegistDep, msg_daemonname)))
        break;

      if ((rc = encode_DIS_Register(chan, request)))
        break;

      if ((rc = encode_DIS_ReqExtend(chan, 0)))
        break;

      rc = DIS_tcp_wflush(chan);

      break;

    case PBS_BATCH_AsySignalJob:

    case PBS_BATCH_SignalJob:

      rc = PBSD_sig_put(
             conn,
             request->rq_ind.rq_signal.rq_jid,
             request->rq_ind.rq_signal.rq_signame,
             request->rq_extra);

      break;

    case PBS_BATCH_StatusJob:

      rc = PBSD_status_put(
             conn,
             PBS_BATCH_StatusJob,
             request->rq_ind.rq_status.rq_id,
             NULL,
             NULL);

      break;

    case PBS_BATCH_TrackJob:

      if ((rc = encode_DIS_ReqHdr(chan, PBS_BATCH_TrackJob, msg_daemonname)))
        break;

      if ((rc = encode_DIS_TrackJob(chan, request)))
        break;

      if ((rc = encode_DIS_ReqExtend(chan, 0)))
        break;

      rc = DIS_tcp_wflush(chan);

      break;

    case PBS_BATCH_ReturnFiles:

      if ((rc = encode_DIS_ReqHdr(chan, PBS_BATCH_ReturnFiles, msg_daemonname)))
        break;

      if ((rc = encode_DIS_ReturnFiles(chan, request)))
        break;

      if ((rc = encode_DIS_ReqExtend(chan, 0)))
        break;

      rc = DIS_tcp_wflush(chan);

      break;

    case PBS_BATCH_CopyFiles:

      if ((rc = encode_DIS_ReqHdr(chan, PBS_BATCH_CopyFiles, msg_daemonname)))
        break;

      if ((rc = encode_DIS_CopyFiles(chan, request)))
        break;

      if ((rc = encode_DIS_ReqExtend(chan, 0)))
        break;

      rc = DIS_tcp_wflush(chan);

      break;

    case PBS_BATCH_DelFiles:

      if ((rc = encode_DIS_ReqHdr(chan, PBS_BATCH_DelFiles, msg_daemonname)))
        break;

      if ((rc = encode_DIS_CopyFiles(chan, request)))
        break;

      if ((rc = encode_DIS_ReqExtend(chan, 0)))
        break;

      rc = DIS_tcp_wflush(chan);

      break;

    case PBS_BATCH_DeleteReservation:

      if ((rc = encode_DIS_ReqHdr(chan, PBS_BATCH_DeleteReservation, msg_daemonname)))
        break;

      if ((rc = encode_DIS_ReqExtend(chan, request->rq_extend)))
        break;

      rc = DIS_tcp_wflush(chan);
      
      break;

#else /* PBS_MOM */

    case PBS_BATCH_JobObit:

      /* who is sending obit request? */

      if ((rc = encode_DIS_ReqHdr(chan, PBS_BATCH_JobObit, msg_daemonname)))
        break;

      if ((rc = encode_DIS_JobObit(chan, request)))
        break;

      if ((rc = encode_DIS_ReqExtend(chan, 0)))
        break;

      rc = DIS_tcp_wflush(chan);

      break;

#endif /* PBS_MOM */

    default:

      sprintf(log_buf, msg_issuebad,
              request->rq_type);

      log_err(-1, __func__, log_buf);

      pthread_mutex_lock(ptask->wt_mutex);
      if (ptask->wt_being_recycled == FALSE)
        delete_task(ptask);
      else
        pthread_mutex_unlock(ptask->wt_mutex);

      rc = -1;

      break;
    }  /* END switch (request->rq_type) */

  if ((tmp_rc = DIS_reply_read(chan, &request->rq_reply)) != 0)
    {
    sprintf(log_buf, "DIS_reply_read failed: %d", tmp_rc);
    log_record(PBSEVENT_JOB, PBS_EVENTCLASS_JOB, __func__, log_buf);
    request->rq_reply.brp_code = tmp_rc;
    request->rq_reply.brp_choice = BATCH_REPLY_CHOICE_NULL;
    }  
  DIS_tcp_cleanup(chan);
  svr_disconnect(conn);

  if (func != NULL)
    dispatch_task(ptask);

  return(rc);
  }  /* END issue_Drequest() */




/* END issue_request.c */

