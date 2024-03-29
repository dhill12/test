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
 * queue_func.c - various functions dealing with queues
 *
 * Included functions are:
 * que_alloc() - allocacte and initialize space for queue structure
 * que_free() - free queue structure
 * que_purge() - remove queue from server
 * find_queuebyname() - find a queue with a given name
 * get_dfltque() - get default queue
 */

#include <pbs_config.h>   /* the master config generated by configure */
#include "queue_func.h"

#include <sys/param.h>
#include <memory.h>
#include <stdlib.h>
#include "pbs_ifl.h"
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include "list_link.h"
#include "log.h"
#include "../lib/Liblog/pbs_log.h"
#include "attribute.h"
#include "server_limits.h"
#include "server.h"
#include "queue.h"
#include "pbs_job.h"
#include "pbs_error.h"
#if __STDC__ != 1
#include <memory.h>
#endif
#include <pthread.h>
#include "queue_recycler.h"
#include "svrfunc.h"
#include "svr_func.h" /* get_svr_attr_* */
#include "ji_mutex.h"


#define MSG_LEN_LONG 160

/* Global Data */

extern int LOGLEVEL;

extern char     *msg_err_unlink;
extern char *path_queues;

extern struct    server server;
extern all_queues svr_queues;
extern queue_recycler q_recycler;

int lock_queue(

  struct pbs_queue *the_queue,
  const char       *id,
  char             *msg,
  int               logging)

  {
  int rc = PBSE_NONE;
  char *err_msg = NULL;

  if (logging >= 10)
    { 
    err_msg = (char *)calloc(1, MSG_LEN_LONG);
    snprintf(err_msg, MSG_LEN_LONG, "locking %s in method %s", the_queue->qu_qs.qu_name, id);
    log_record(PBSEVENT_DEBUG, PBS_EVENTCLASS_NODE, __func__, err_msg);
    }

  if (pthread_mutex_lock(the_queue->qu_mutex) != 0)
    { 
    if (logging >= 10) 
      {
      snprintf(err_msg, MSG_LEN_LONG, "ALERT: cannot lock queue %s mutex in method %s",
          the_queue->qu_qs.qu_name, id);
      log_record(PBSEVENT_DEBUG, PBS_EVENTCLASS_NODE, __func__, err_msg);
      }
    rc = PBSE_MUTEX;
    } 

  if (err_msg != NULL)
    free(err_msg);

  return rc;
  }

int unlock_queue(

  struct pbs_queue *the_queue,
  const char       *id,
  char             *msg,
  int               logging)

  {
  int rc = PBSE_NONE;
  char *err_msg = NULL;
  char stub_msg[] = "no pos";

  if (logging >= 10)
    {
    err_msg = (char *)calloc(1, MSG_LEN_LONG);
    if (msg == NULL)
      msg = stub_msg;
    snprintf(err_msg, MSG_LEN_LONG, "unlocking %s in method %s-%s", the_queue->qu_qs.qu_name, id, msg);
    log_record(PBSEVENT_DEBUG, PBS_EVENTCLASS_NODE, __func__, err_msg);
    }

  if (pthread_mutex_unlock(the_queue->qu_mutex) != 0)
    {
    if (logging >= 10)
      {
      snprintf(err_msg, MSG_LEN_LONG, "ALERT: cannot unlock queue %s mutex in method %s",
          the_queue->qu_qs.qu_name, id);
      log_record(PBSEVENT_DEBUG, PBS_EVENTCLASS_NODE, __func__, err_msg);
      }
    rc = PBSE_MUTEX;
    }

  if (err_msg != NULL)
    free(err_msg);

  return rc;
  }


/*
 * que_alloc - allocate space for a queue structure and initialize
 * attributes to "unset"
 *
 * Returns: pointer to structure or null is space not available.
 */

pbs_queue *que_alloc(

  char *name,
  int   sv_qs_mutex_held)

  {
  static char *mem_err = "no memory";

  int        i;
  pbs_queue *pq;

  pq = (pbs_queue *)calloc(1, sizeof(pbs_queue));

  if (pq == NULL)
    {
    log_err(errno, __func__, mem_err);

    return(NULL);
    }

  pq->qu_qs.qu_type = QTYPE_Unset;

  pq->qu_mutex = calloc(1, sizeof(pthread_mutex_t));
  pq->qu_jobs = calloc(1, sizeof(struct all_jobs));
  pq->qu_jobs_array_sum = calloc(1, sizeof(struct all_jobs));
  
  if ((pq->qu_mutex == NULL) ||
      (pq->qu_jobs == NULL) ||
      (pq->qu_jobs_array_sum == NULL))
    {
    log_err(ENOMEM, __func__, mem_err);
    return(NULL);
    }

  initialize_all_jobs_array(pq->qu_jobs);
  initialize_all_jobs_array(pq->qu_jobs_array_sum);
  pthread_mutex_init(pq->qu_mutex,NULL);
  lock_queue(pq, "que_alloc", NULL, LOGLEVEL);

  strncpy(pq->qu_qs.qu_name, name, PBS_MAXQUEUENAME);

  insert_queue(&svr_queues,pq);
 
  if (sv_qs_mutex_held == FALSE)
    lock_sv_qs_mutex(server.sv_qs_mutex, __func__);
  server.sv_qs.sv_numque++;
  if (sv_qs_mutex_held == FALSE)
    unlock_sv_qs_mutex(server.sv_qs_mutex, __func__);

  /* set the working attributes to "unspecified" */

  for (i = 0; i < QA_ATR_LAST; i++)
    {
    clear_attr(&pq->qu_attr[i], &que_attr_def[i]);
    }

  return(pq);
  }  /* END que_alloc() */




/*
 * que_free - free queue structure and its various sub-structures
 */

void que_free(

  pbs_queue *pq,
  int        sv_qs_mutex_held)

  {
  int            i;
  pbs_attribute *pattr;
  attribute_def *pdef;

  /* remove any calloc working pbs_attribute space */
  for (i = 0;i < QA_ATR_LAST;i++)
    {
    pdef  = &que_attr_def[i];
    pattr = &pq->qu_attr[i];

    pdef->at_free(pattr);

    /* remove any acl lists associated with the queue */

    if (pdef->at_type == ATR_TYPE_ACL)
      {
      pattr->at_flags |= ATR_VFLAG_MODIFY;

      save_acl(pattr, pdef, pdef->at_name, pq->qu_qs.qu_name);
      }
    }

  /* now free the main structure */
  if (sv_qs_mutex_held == FALSE)
    lock_sv_qs_mutex(server.sv_qs_mutex, __func__);
  server.sv_qs.sv_numque--;
  if (sv_qs_mutex_held == FALSE)
    unlock_sv_qs_mutex(server.sv_qs_mutex, __func__);

  remove_queue(&svr_queues, pq);
  pq->q_being_recycled = TRUE;
  insert_into_queue_recycler(pq);
  unlock_queue(pq, "que_free", NULL, LOGLEVEL);

  return;
  }  /* END que_free() */




/*
 * que_purge - purge queue from system
 *
 * The queue is dequeued, the queue file is unlinked.
 * If the queue contains any jobs, the purge is not allowed.
 */

int que_purge(

  pbs_queue *pque)

  {
  char     namebuf[MAXPATHLEN];
  char     log_buf[LOCAL_LOG_BUF_SIZE];

  if (pque->qu_numjobs != 0)
    {
    return(PBSE_QUEBUSY);
    }

  snprintf(namebuf, sizeof(namebuf), "%s%s", path_queues, pque->qu_qs.qu_name);

  if (unlink(namebuf) < 0)
    {
    sprintf(log_buf, msg_err_unlink, "Queue", namebuf);

    log_err(errno, "queue_purge", log_buf);
    }

  que_free(pque, FALSE);

  return(0);
  }





/*
 * find_queuebyname() - find a queue by its name
 */

pbs_queue *find_queuebyname(

  char *quename) /* I */

  {
  char  *pc;
  pbs_queue *pque = NULL;
  char   qname[PBS_MAXDEST + 1];
  int    i;

  snprintf(qname, sizeof(qname), "%s", quename);

  pc = strchr(qname, (int)'@'); /* strip off server (fragment) */

  if (pc != NULL)
    *pc = '\0';

  pthread_mutex_lock(svr_queues.allques_mutex);

  i = get_value_hash(svr_queues.ht,qname);

  if (i >= 0)
    {
    pque = svr_queues.ra->slots[i].item;
    }
  
  if (pque != NULL)
    lock_queue(pque, __func__, NULL, LOGLEVEL);

  pthread_mutex_unlock(svr_queues.allques_mutex);
  
  if (pque != NULL)
    {
    if (pque->q_being_recycled != FALSE)
      {
      unlock_queue(pque, __func__, "recycled queue", LOGLEVEL);
      pque = NULL;
      }
    }

  if (pc != NULL)
    *pc = '@'; /* restore '@' server portion */

  return(pque);
  }  /* END find_queuebyname() */





/*
 * get_dfltque - get the default queue (if declared)
 */

pbs_queue *get_dfltque(void)

  {
  pbs_queue *pq = NULL;
  char      *dque = NULL;

  if (get_svr_attr_str(SRV_ATR_dflt_que, &dque) == PBSE_NONE)
    {
    pq = find_queuebyname(dque);
    }

  return(pq);
  }  /* END get_dfltque() */




void initialize_allques_array(

  all_queues *aq)

  {
  aq->ra = initialize_resizable_array(INITIAL_QUEUE_SIZE);
  aq->ht = create_hash(INITIAL_HASH_SIZE);

  aq->allques_mutex = calloc(1, sizeof(pthread_mutex_t));
  pthread_mutex_init(aq->allques_mutex,NULL);
  } /* END initialize_all_ques_array() */




void free_alljobs_array(

  struct all_jobs *aj)

  {
  free(aj->alljobs_mutex);
  free_resizable_array(aj->ra);
  free_hash(aj->ht);
  } /* END free_alljobs_array() */   




int insert_queue(

  all_queues *aq,
  pbs_queue  *pque)

  {
  int          rc;

  pthread_mutex_lock(aq->allques_mutex);

  if ((rc = insert_thing(aq->ra,pque)) == -1)
    {
    rc = ENOMEM;
    log_err(rc, __func__, "No memory to resize the array");
    }
  else
    {
    add_hash(aq->ht,rc,pque->qu_qs.qu_name);
    rc = PBSE_NONE;
    }

  pthread_mutex_unlock(aq->allques_mutex);

  return(rc);
  } /* END insert_queue() */





int remove_queue(

  all_queues *aq,
  pbs_queue  *pque)

  {
  int  rc = PBSE_NONE;
  int  index;
  char log_buf[1000];

  if (pthread_mutex_trylock(aq->allques_mutex))
    {
    unlock_queue(pque, __func__, NULL, LOGLEVEL);
    pthread_mutex_lock(aq->allques_mutex);
    lock_queue(pque, __func__, NULL, LOGLEVEL);
    }

  if ((index = get_value_hash(aq->ht,pque->qu_qs.qu_name)) < 0)
    rc = THING_NOT_FOUND;
  else
    {
    remove_thing_from_index(aq->ra,index);
    remove_hash(aq->ht,pque->qu_qs.qu_name);
    }

  snprintf(log_buf, sizeof(log_buf), "index = %d, name = %s", index, pque->qu_qs.qu_name);
  log_err(-1, __func__, log_buf);

  pthread_mutex_unlock(aq->allques_mutex);

  return(rc);
  } /* END remove_queue() */





pbs_queue *next_queue(

  all_queues *aq,
  int        *iter)

  {
  pbs_queue *pque;

  pthread_mutex_lock(aq->allques_mutex);

  pque = (pbs_queue *)next_thing(aq->ra,iter);
  if (pque != NULL)
    lock_queue(pque, "next_queue", NULL, LOGLEVEL);
  pthread_mutex_unlock(aq->allques_mutex);

  if (pque != NULL)
    {
    if (pque->q_being_recycled != FALSE)
      {
      unlock_queue(pque, __func__, "recycled queue", LOGLEVEL);
      pque = next_queue(aq, iter);
      }
    }

  return(pque);
  } /* END next_queue() */




/* 
 * gets the locks on both queues without releasing the all_queues mutex lock.
 * Doing this another way can cause deadlock.
 *
 * @return PBSE_NONE on success
 */

int get_parent_dest_queues(

  char       *queue_parent_name,
  char       *queue_dest_name,
  pbs_queue **parent,
  pbs_queue **dest,
  job       **pjob_ptr)

  {
  pbs_queue *pque_parent;
  pbs_queue *pque_dest;
  char       jobid[PBS_MAXSVRJOBID + 1];
  job       *pjob = *pjob_ptr;
  int        index_parent;
  int        index_dest;
  int        rc = PBSE_NONE;

  strcpy(jobid, pjob->ji_qs.ji_jobid);
  unlock_ji_mutex(pjob, __func__, "1", LOGLEVEL);

  unlock_queue(*parent, __func__, NULL, 0);

  *parent = NULL;
  *dest   = NULL;

  pthread_mutex_lock(svr_queues.allques_mutex);

  index_parent = get_value_hash(svr_queues.ht, queue_parent_name);
  index_dest   = get_value_hash(svr_queues.ht, queue_dest_name);

  if ((index_parent < 0) ||
      (index_dest < 0))
    {
    rc = -1;
    }
  else
    {
    /* good path */
    pque_parent = svr_queues.ra->slots[index_parent].item;
    pque_dest   = svr_queues.ra->slots[index_dest].item;

    if ((pque_parent == NULL) ||
        (pque_dest == NULL))
      {
      rc = -1;
      }
    else
      {
      /* SUCCESS! */
      lock_queue(pque_parent, __func__, NULL, 0);
      lock_queue(pque_dest,   __func__, NULL, 0);
      *parent = pque_parent;
      *dest = pque_dest;

      rc = PBSE_NONE;
      }
    }

  pthread_mutex_unlock(svr_queues.allques_mutex);

  if ((*pjob_ptr = svr_find_job(jobid, TRUE)) == NULL)
    rc = -1;

  return(rc);
  } /* END get_parent_dest_queues() */





pbs_queue *lock_queue_with_job_held(

  pbs_queue  *pque,
  job       **pjob_ptr)

  {
  char       jobid[PBS_MAXSVRJOBID];
  job       *pjob = *pjob_ptr;

  if (pque != NULL)
    {
    if (pthread_mutex_trylock(pque->qu_mutex))
      {
      /* if fail */
      strcpy(jobid, pjob->ji_qs.ji_jobid);
      unlock_ji_mutex(pjob, __func__, "1", LOGLEVEL);
      lock_queue(pque, __func__, NULL, LOGLEVEL);

      if ((pjob = svr_find_job(jobid, TRUE)) == NULL)
        {
        unlock_queue(pque, __func__, NULL, 0);
        pque = NULL;
        *pjob_ptr = NULL;
        }
      }
    }

  return(pque);
  } /* END get_jobs_queue() */



/* END queue_func.c */

