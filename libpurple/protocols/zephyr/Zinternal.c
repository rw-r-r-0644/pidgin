/* This file is part of the Project Athena Zephyr Notification System.
 * It contains source for the internal Zephyr routines.
 *
 *	Created by:	Robert French
 *
 *	Copyright (c) 1987,1988,1991 by the Massachusetts Institute of
 *	Technology.
 *	For copying and distribution information, see the file
 *	"mit-copyright.h".
 */

#include <purple.h>

#include "internal.h"
#ifdef WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#endif

GSocket *__Zephyr_socket = NULL;
gint __Zephyr_port = -1;
guint32 __My_addr;
int __Q_CompleteLength;
int __Q_Size;
GQueue Z_input_queue = G_QUEUE_INIT;
GSocketAddress *__HM_addr;
ZLocations_t *__locate_list;
int __locate_num;
int __locate_next;
ZSubscription_t *__subscriptions_list;
int __subscriptions_num;
int __subscriptions_next;
int Z_discarded_packets = 0;

#ifdef ZEPHYR_USES_KERBEROS
C_Block __Zephyr_session;
#endif
char __Zephyr_realm[REALM_SZ];

static int Z_AddField(char **ptr, const char *field, char *end);
static int find_or_insert_uid(ZUnique_Id_t *uid, ZNotice_Kind_t kind);

/* Find or insert uid in the old uids buffer.  The buffer is a sorted
 * circular queue.  We make the assumption that most packets arrive in
 * order, so we can usually search for a uid or insert it into the buffer
 * by looking back just a few entries from the end.  Since this code is
 * only executed by the client, the implementation isn't microoptimized. */
static int
find_or_insert_uid(ZUnique_Id_t *uid, ZNotice_Kind_t kind)
{
    static struct _filter {
	ZUnique_Id_t	uid;
	ZNotice_Kind_t	kind;
	time_t		t;
    } *buffer;
    static long size;
    static long start;
    static long num;

    time_t now;
    struct _filter *new;
    long i, j, new_size;
    int result;

    /* Initialize the uid buffer if it hasn't been done already. */
    if (!buffer) {
	size = Z_INITFILTERSIZE;
	buffer = (struct _filter *) malloc(size * sizeof(*buffer));
	if (!buffer)
	    return 0;
    }

    /* Age the uid buffer, discarding any uids older than the clock skew. */
    time(&now);
    while (num && (now - buffer[start % size].t) > CLOCK_SKEW)
	start++, num--;
    start %= size;

    /* Make room for a new uid, since we'll probably have to insert one. */
    if (num == size) {
	new_size = size * 2 + 2;
	new = (struct _filter *) malloc(new_size * sizeof(*new));
	if (!new)
	    return 0;
	for (i = 0; i < num; i++)
	    new[i] = buffer[(start + i) % size];
	free(buffer);
	buffer = new;
	size = new_size;
	start = 0;
    }

    /* Search for this uid in the buffer, starting from the end. */
    for (i = start + num - 1; i >= start; i--) {
	result = memcmp(uid, &buffer[i % size].uid, sizeof(*uid));
	if (result == 0 && buffer[i % size].kind == kind)
	    return 1;
	if (result > 0)
	    break;
    }

    /* We didn't find it; insert the uid into the buffer after i. */
    i++;
    for (j = start + num; j > i; j--)
	buffer[j % size] = buffer[(j - 1) % size];
    buffer[i % size].uid = *uid;
    buffer[i % size].kind = kind;
    buffer[i % size].t = now;
    num++;

    return 0;
}

/* Wait for a complete notice to become available */

Code_t Z_WaitForComplete(void)
{
    Code_t retval;

    if (__Q_CompleteLength)
	return (Z_ReadEnqueue());

    while (!__Q_CompleteLength)
	if ((retval = Z_ReadWait()) != ZERR_NONE)
	    return (retval);

    return (ZERR_NONE);
}


/* Read any available packets and enqueue them */

Code_t
Z_ReadEnqueue(void)
{
	if (ZGetSocket() == NULL) {
		return ZERR_NOPORT;
	}

	while (g_socket_condition_check(ZGetSocket(), G_IO_IN)) {
		Code_t retval = Z_ReadWait();
		if (retval != ZERR_NONE) {
			return retval;
		}
	}

	return ZERR_NONE;
}


/*
 * Search the queue for a notice with the proper multiuid - remove any
 * notices that haven't been touched in a while
 */

static Z_InputQ *
Z_SearchQueue(ZUnique_Id_t *uid, ZNotice_Kind_t kind)
{
	register GList *list;
	GList *next;
	gint64 now;

	now = g_get_monotonic_time();

	list = Z_input_queue.head;

	while (list) {
		Z_InputQ *qptr = (Z_InputQ *)list->data;
		if (ZCompareUID(uid, &qptr->uid) && qptr->kind == kind) {
			return qptr;
		}
		next = list->next;
		if (qptr->time && qptr->time + Z_NOTICETIMELIMIT < now) {
			Z_RemQueue(qptr);
		}
		list = next;
	}
	return NULL;
}

/*
 * Now we delve into really convoluted queue handling and
 * fragmentation reassembly algorithms and other stuff you probably
 * don't want to look at...
 *
 * This routine does NOT guarantee a complete packet will be ready when it
 * returns.
 */

Code_t
Z_ReadWait(void)
{
	register Z_InputQ *qptr;
	Z_Hole *hole = NULL;
    ZNotice_t notice;
    ZPacket_t packet;
	GSocketAddress *from = NULL;
    int packet_len, zvlen, part, partof;
    char *slash;
    Code_t retval;
	GError *error = NULL;

	if (ZGetSocket() == NULL) {
		return ZERR_NOPORT;
	}

	if (!g_socket_condition_timed_wait(ZGetSocket(), G_IO_IN,
	                                   60 * G_USEC_PER_SEC, NULL, &error)) {
		gint ret = ZERR_INTERNAL;
		if (error->code == G_IO_ERROR_TIMED_OUT) {
			ret = ETIMEDOUT;
		}
		g_error_free(error);
		return ret;
	}

	packet_len = g_socket_receive_from(ZGetSocket(), &from, packet,
	                                   sizeof(packet) - 1, NULL, &error);
	if (packet_len < 0) {
		purple_debug_error("zephyr", "Unable to receive from socket: %s",
		                   error->message);
		g_error_free(error);
		return ZERR_INTERNAL;
	}

	if (packet_len == 0) {
		return ZERR_EOF;
	}

	packet[packet_len] = '\0';

	/* Ignore obviously non-Zephyr packets. */
	zvlen = sizeof(ZVERSIONHDR) - 1;
	if (packet_len < zvlen || memcmp(packet, ZVERSIONHDR, zvlen) != 0) {
		Z_discarded_packets++;
		g_object_unref(from);
		return ZERR_NONE;
	}

	/* Parse the notice */
	if ((retval = ZParseNotice(packet, packet_len, &notice)) != ZERR_NONE) {
		g_object_unref(from);
		return retval;
	}

	/* If the notice is of an appropriate kind, send back a CLIENTACK to
	 * whoever sent it to say we got it. */
	if (notice.z_kind != HMACK && notice.z_kind != SERVACK &&
			notice.z_kind != SERVNAK && notice.z_kind != CLIENTACK) {
		GSocketAddress *olddest = NULL;
		ZNotice_t tmpnotice;
		ZPacket_t pkt;
		int len;

		tmpnotice = notice;
		tmpnotice.z_kind = CLIENTACK;
		tmpnotice.z_message_len = 0;
		olddest = __HM_addr;
		__HM_addr = from;
		if ((retval = ZFormatSmallRawNotice(&tmpnotice, pkt, &len)) != ZERR_NONE) {
			__HM_addr = olddest;
			g_object_unref(from);
			return retval;
		}
		if ((retval = ZSendPacket(pkt, len, 0)) != ZERR_NONE) {
			__HM_addr = olddest;
			g_object_unref(from);
			return retval;
		}
		__HM_addr = olddest;
	}
	if (find_or_insert_uid(&notice.z_uid, notice.z_kind)) {
		g_object_unref(from);
		return ZERR_NONE;
	}

	/* Check authentication on the notice. */
	notice.z_checked_auth = ZCheckAuthentication(&notice);

    /*
     * Parse apart the z_multinotice field - if the field is blank for
     * some reason, assume this packet stands by itself.
     */
    slash = strchr(notice.z_multinotice, '/');
    if (slash) {
	part = atoi(notice.z_multinotice);
	partof = atoi(slash+1);
	if (part > partof || partof == 0) {
	    part = 0;
	    partof = notice.z_message_len;
	}
    }
    else {
	part = 0;
	partof = notice.z_message_len;
    }

	/* Too big a packet...just ignore it! */
	if (partof > Z_MAXNOTICESIZE) {
		g_object_unref(from);
		return ZERR_NONE;
	}

    /* If we can find a notice in the queue with the same multiuid field,
     * insert the current fragment as appropriate. */
    switch (notice.z_kind) {
    case SERVACK:
    case SERVNAK:
	/* The SERVACK and SERVNAK replies shouldn't be reassembled
	   (they have no parts).  Instead, we should hold on to the reply
	   ONLY if it's the first part of a fragmented message, i.e.
	   multi_uid == uid.  This allows programs to wait for the uid
	   of the first packet, and get a response when that notice
	   arrives.  Acknowledgements of the other fragments are discarded
	   (XXX we assume here that they all carry the same information
	   regarding failure/success)
	 */
	if (!ZCompareUID(&notice.z_multiuid, &notice.z_uid)) {
		/* they're not the same... throw away this packet. */
		g_object_unref(from);
		return ZERR_NONE;
	}
	/* fall thru & process it */
    default:
	/* for HMACK types, we assume no packet loss (local loopback
	   connections).  The other types can be fragmented and MUST
	   run through this code. */
	if ((qptr = Z_SearchQueue(&notice.z_multiuid, notice.z_kind)) != NULL) {
		/* If this is the first fragment, and we haven't already gotten
		 * a first fragment, grab the header from it. */
		if (part == 0 && qptr->header == NULL) {
			qptr->header_len = packet_len - notice.z_message_len;
			qptr->header = g_memdup(packet, qptr->header_len);
		}
		g_object_unref(from);
		return Z_AddNoticeToEntry(qptr, &notice, part);
	}
    }

	/* We'll have to create a new entry...make sure the queue isn't going
	 * to get too big. */
	if (__Q_Size + partof > Z_MAXQUEUESIZE) {
		g_object_unref(from);
		return ZERR_NONE;
	}

	/* This is a notice we haven't heard of, so create a new queue entry
	 * for it and zero it out. */
	qptr = g_new0(Z_InputQ, 1);

	/* Insert the entry at the end of the queue */
	g_queue_push_tail(&Z_input_queue, qptr);

	/* Copy the from field, multiuid, kind, and checked authentication. */
	qptr->from = from;
	qptr->uid = notice.z_multiuid;
	qptr->kind = notice.z_kind;
	qptr->auth = notice.z_checked_auth;

	/* If this is the first part of the notice, we take the header from it.
	 * We only take it if this is the first fragment so that the Unique
	 * ID's will be predictable. */
	if (part == 0) {
		qptr->header_len = packet_len - notice.z_message_len;
		qptr->header = g_memdup(packet, qptr->header_len);
	}

	/* If this is not a fragmented notice, then don't bother with a hole
	 * list. */
	if (part == 0 && notice.z_message_len == partof) {
		__Q_CompleteLength++;
		qptr->holelist = NULL;
		qptr->complete = TRUE;
		/* allocate a msg buf for this piece */
		if (notice.z_message_len == 0) {
			qptr->msg = NULL;
		} else {
			qptr->msg = g_memdup(notice.z_message, notice.z_message_len);
		}
		qptr->msg_len = notice.z_message_len;
		__Q_Size += notice.z_message_len;
		qptr->packet_len = qptr->header_len + qptr->msg_len;
		qptr->packet = g_new(gchar, qptr->packet_len);
		memcpy(qptr->packet, qptr->header, qptr->header_len);
		if (qptr->msg) {
			memcpy(qptr->packet + qptr->header_len, qptr->msg, qptr->msg_len);
		}
		return ZERR_NONE;
	}

	/* We know how long the message is going to be (this is better than IP
	 * fragmentation...), so go ahead and allocate it all. */
	qptr->msg = g_new0(gchar, partof);
	qptr->msg_len = partof;
	__Q_Size += partof;

	/*
	 * Well, it's a fragmented notice...allocate a hole list and
	 * initialize it to the full packet size.  Then insert the
	 * current fragment.
	 */
	hole = g_new0(Z_Hole, 1);
	if (hole == NULL) {
		return ENOMEM;
	}
	hole->first = 0;
	hole->last = partof - 1;
	qptr->holelist = g_slist_prepend(qptr->holelist, hole);
	return Z_AddNoticeToEntry(qptr, &notice, part);
}


/* Fragment management routines - compliments, more or less, of RFC815 */

static gint
find_hole(gconstpointer element, gconstpointer data)
{
	Z_Hole *thishole = (Z_Hole *)element;
	Z_Hole *tofind = (Z_Hole *)data;

	if (tofind->first <= thishole->last && tofind->last >= thishole->first) {
		return 0;
	}

	return 1;
}

Code_t
Z_AddNoticeToEntry(Z_InputQ *qptr, ZNotice_t *notice, int part)
{
	GSList *thishole;
	Z_Hole *hole;
	gint last;

	/* Incorporate this notice's checked authentication. */
	if (notice->z_checked_auth == ZAUTH_FAILED) {
		qptr->auth = ZAUTH_FAILED;
	} else if (notice->z_checked_auth == ZAUTH_NO && qptr->auth != ZAUTH_FAILED) {
		qptr->auth = ZAUTH_NO;
	}

	qptr->time = g_get_monotonic_time();

	last = part + notice->z_message_len - 1;

	/* copy in the message body */
	memcpy(qptr->msg + part, notice->z_message, notice->z_message_len);

	/* Search for a hole that overlaps with the current fragment */
	hole = g_new(Z_Hole, 1);
	hole->first = part;
	hole->last = last;
	thishole = g_slist_find_custom(qptr->holelist, hole, find_hole);
	g_free(hole);

	/* If we found one, delete it and reconstruct a new hole */
	if (thishole) {
		gint oldfirst, oldlast;

		hole = (Z_Hole *)thishole->data;
		oldfirst = hole->first;
		oldlast = hole->last;
		qptr->holelist = g_slist_delete_link(qptr->holelist, thishole);
		g_free(hole);

		/*
		 * Now create a new hole that is the original hole without the
		 * current fragment.
		 */
		if (part > oldfirst) {
			hole = g_new0(Z_Hole, 1);
			qptr->holelist = g_slist_prepend(qptr->holelist, hole);
			hole->first = oldfirst;
			hole->last = part - 1;
		}
		if (last < oldlast) {
			hole = g_new0(Z_Hole, 1);
			qptr->holelist = g_slist_prepend(qptr->holelist, hole);
			hole->first = last + 1;
			hole->last = oldlast;
		}
	}

	/* If there are no more holes, the packet is complete. */
	if (qptr->holelist == NULL) {
		if (!qptr->complete) {
			__Q_CompleteLength++;
		}
		qptr->complete = TRUE;
		qptr->time = 0; /* don't time out anymore */
		qptr->packet_len = qptr->header_len + qptr->msg_len;
		qptr->packet = g_new(gchar, qptr->packet_len);
		memcpy(qptr->packet, qptr->header, qptr->header_len);
		memcpy(qptr->packet + qptr->header_len, qptr->msg, qptr->msg_len);
	}

	return ZERR_NONE;
}

Code_t
Z_FormatHeader(ZNotice_t *notice, char *buffer, int buffer_len, int *len,
               Z_AuthProc cert_routine)
{
	static char version[BUFSIZ]; /* default init should be all \0 */
	gint64 realtime;

	if (!notice->z_sender) {
		notice->z_sender = ZGetSender();
	}

	if (notice->z_port == 0) {
		GSocketAddress *addr = NULL;
		GError *error = NULL;

		if (ZGetSocket() == NULL) {
			Code_t retval = ZOpenPort(NULL);
			if (retval != ZERR_NONE) {
				return retval;
			}
		}

		addr = g_socket_get_local_address(ZGetSocket(), &error);
		if (addr == NULL) {
			purple_debug_error("zephyr",
			                   "Unable to determine socket local address: %s",
			                   error->message);
			g_error_free(error);
			return ZERR_INTERNAL;
		}
		notice->z_port =
		        g_inet_socket_address_get_port(G_INET_SOCKET_ADDRESS(addr));
		g_object_unref(addr);
	}

	notice->z_multinotice = "";

	realtime = g_get_real_time();
	notice->z_uid.tv.tv_sec = realtime / G_USEC_PER_SEC;
	notice->z_uid.tv.tv_usec =
	        realtime - notice->z_uid.tv.tv_sec * G_USEC_PER_SEC;
	notice->z_uid.tv.tv_sec = htonl((unsigned long)notice->z_uid.tv.tv_sec);
	notice->z_uid.tv.tv_usec = htonl((unsigned long)notice->z_uid.tv.tv_usec);

	memcpy(&notice->z_uid.zuid_addr, &__My_addr, sizeof(__My_addr));

	notice->z_multiuid = notice->z_uid;

	if (!version[0]) {
		sprintf(version, "%s%d.%d", ZVERSIONHDR, ZVERSIONMAJOR, ZVERSIONMINOR);
	}
	notice->z_version = version;

	return Z_FormatAuthHeader(notice, buffer, buffer_len, len, cert_routine);
}

Code_t
Z_FormatAuthHeader(ZNotice_t *notice, char *buffer, int buffer_len, int *len,
                   Z_AuthProc cert_routine)
{
    if (!cert_routine) {
	notice->z_auth = 0;
	notice->z_authent_len = 0;
	notice->z_ascii_authent = "";
	notice->z_checksum = 0;
	return (Z_FormatRawHeader(notice, buffer, buffer_len,
				  len, NULL, NULL));
    }

    return ((*cert_routine)(notice, buffer, buffer_len, len));
}

Code_t
Z_FormatRawHeader(ZNotice_t *notice, char *buffer, gsize buffer_len, int *len,
                  char **cstart, char **cend)
{
    char newrecip[BUFSIZ];
    char *ptr, *end;
    int i;

    if (!notice->z_class)
	    notice->z_class = "";

    if (!notice->z_class_inst)
	    notice->z_class_inst = "";

    if (!notice->z_opcode)
	    notice->z_opcode = "";

    if (!notice->z_recipient)
	    notice->z_recipient = "";

    if (!notice->z_default_format)
	    notice->z_default_format = "";

    ptr = buffer;
    end = buffer+buffer_len;

    if (buffer_len < strlen(notice->z_version)+1)
	return (ZERR_HEADERLEN);

    g_strlcpy(ptr, notice->z_version, buffer_len);
    ptr += strlen(ptr)+1;

    if (ZMakeAscii32(ptr, end-ptr, Z_NUMFIELDS + notice->z_num_other_fields)
	== ZERR_FIELDLEN)
	return (ZERR_HEADERLEN);
    ptr += strlen(ptr)+1;

    if (ZMakeAscii32(ptr, end-ptr, notice->z_kind) == ZERR_FIELDLEN)
	return (ZERR_HEADERLEN);
    ptr += strlen(ptr)+1;

    if (ZMakeAscii(ptr, end-ptr, (unsigned char *)&notice->z_uid,
		   sizeof(ZUnique_Id_t)) == ZERR_FIELDLEN)
	return (ZERR_HEADERLEN);
    ptr += strlen(ptr)+1;

    if (ZMakeAscii16(ptr, end-ptr, ntohs(notice->z_port)) == ZERR_FIELDLEN)
	return (ZERR_HEADERLEN);
    ptr += strlen(ptr)+1;

    if (ZMakeAscii32(ptr, end-ptr, notice->z_auth) == ZERR_FIELDLEN)
	return (ZERR_HEADERLEN);
    ptr += strlen(ptr)+1;

    if (ZMakeAscii32(ptr, end-ptr, notice->z_authent_len) == ZERR_FIELDLEN)
	return (ZERR_HEADERLEN);
    ptr += strlen(ptr)+1;

    if (Z_AddField(&ptr, notice->z_ascii_authent, end))
	return (ZERR_HEADERLEN);
    if (Z_AddField(&ptr, notice->z_class, end))
	return (ZERR_HEADERLEN);
    if (Z_AddField(&ptr, notice->z_class_inst, end))
	return (ZERR_HEADERLEN);
    if (Z_AddField(&ptr, notice->z_opcode, end))
	return (ZERR_HEADERLEN);
    if (Z_AddField(&ptr, notice->z_sender, end))
	return (ZERR_HEADERLEN);
    if (strchr(notice->z_recipient, '@') || !*notice->z_recipient) {
	if (Z_AddField(&ptr, notice->z_recipient, end))
	    return (ZERR_HEADERLEN);
    }
    else {
	if (strlen(notice->z_recipient) + strlen(__Zephyr_realm) + 2 >
	    sizeof(newrecip))
	    return (ZERR_HEADERLEN);
	(void) sprintf(newrecip, "%s@%s", notice->z_recipient, __Zephyr_realm);
	if (Z_AddField(&ptr, newrecip, end))
	    return (ZERR_HEADERLEN);
    }
    if (Z_AddField(&ptr, notice->z_default_format, end))
	return (ZERR_HEADERLEN);

    /* copy back the end pointer location for crypto checksum */
    if (cstart)
	*cstart = ptr;
    if (ZMakeAscii32(ptr, end-ptr, notice->z_checksum) == ZERR_FIELDLEN)
	return (ZERR_HEADERLEN);
    ptr += strlen(ptr)+1;
    if (cend)
	*cend = ptr;

    if (Z_AddField(&ptr, notice->z_multinotice, end))
	return (ZERR_HEADERLEN);

    if (ZMakeAscii(ptr, end-ptr, (unsigned char *)&notice->z_multiuid,
		   sizeof(ZUnique_Id_t)) == ZERR_FIELDLEN)
	return (ZERR_HEADERLEN);
    ptr += strlen(ptr)+1;

    for (i=0;i<notice->z_num_other_fields;i++)
	if (Z_AddField(&ptr, notice->z_other_fields[i], end))
	    return (ZERR_HEADERLEN);

    *len = ptr-buffer;

    return (ZERR_NONE);
}

static int
Z_AddField(char **ptr, const char *field, char *end)
{
    register int len;

    len = field ? strlen (field) + 1 : 1;

    if (*ptr+len > end)
	return 1;
    if (field)
        strcpy(*ptr, field);
    else
      **ptr = '\0';
    *ptr += len;

    return 0;
}

static gint
find_complete_input(gconstpointer a, G_GNUC_UNUSED gconstpointer b)
{
	Z_InputQ *qptr = (Z_InputQ *)a;
	return qptr->complete ? 0 : 1;
}

Z_InputQ *
Z_GetFirstComplete(void)
{
	GList *list;

	list = g_queue_find_custom(&Z_input_queue, NULL, find_complete_input);
	return list ? (Z_InputQ *)list->data : NULL;
}

Z_InputQ *
Z_GetNextComplete(Z_InputQ *qptr)
{
	GList *list = g_queue_find(&Z_input_queue, qptr);

	if (list) {
		list = list->next;
		list = g_list_find_custom(list, NULL, find_complete_input);
	}

	return list ? (Z_InputQ *)list->data : NULL;
}

void
Z_RemQueue(Z_InputQ *qptr)
{
	if (qptr->complete) {
		__Q_CompleteLength--;
	}

	__Q_Size -= qptr->msg_len;

	g_free(qptr->header);
	g_free(qptr->msg);
	g_free(qptr->packet);

	g_clear_object(&qptr->from);

	g_slist_free_full(qptr->holelist, g_free);

	g_queue_remove(&Z_input_queue, qptr);
	g_free(qptr);
}

Code_t
Z_SendFragmentedNotice(ZNotice_t *notice, int len, Z_AuthProc cert_func,
                       Z_SendProc send_func)
{
    ZNotice_t partnotice;
    ZPacket_t buffer;
    char multi[64];
    int offset, hdrsize, fragsize, ret_len, message_len, waitforack;
    Code_t retval;

    hdrsize = len-notice->z_message_len;
    fragsize = Z_MAXPKTLEN-hdrsize-Z_FRAGFUDGE;

    offset = 0;

    waitforack = (notice->z_kind == UNACKED || notice->z_kind == ACKED);

    partnotice = *notice;

    while (offset < notice->z_message_len || !notice->z_message_len) {
	(void) sprintf(multi, "%d/%d", offset, notice->z_message_len);
	partnotice.z_multinotice = multi;
	if (offset > 0) {
		gint64 realtime = g_get_real_time();
		partnotice.z_uid.tv.tv_sec = realtime / G_USEC_PER_SEC;
		partnotice.z_uid.tv.tv_usec =
			    realtime - partnotice.z_uid.tv.tv_sec * G_USEC_PER_SEC;
		partnotice.z_uid.tv.tv_sec =
			    htonl((unsigned long)partnotice.z_uid.tv.tv_sec);
		partnotice.z_uid.tv.tv_usec =
			    htonl((unsigned long)partnotice.z_uid.tv.tv_usec);
		memcpy(&partnotice.z_uid.zuid_addr, &__My_addr, sizeof(__My_addr));
	}
	message_len = MIN(notice->z_message_len - offset, fragsize);
	partnotice.z_message = (char*)notice->z_message+offset;
	partnotice.z_message_len = message_len;
	if ((retval = Z_FormatAuthHeader(&partnotice, buffer, Z_MAXHEADERLEN,
					 &ret_len, cert_func)) != ZERR_NONE) {
	    return (retval);
	}
	memcpy(buffer + ret_len, partnotice.z_message, message_len);
	if ((retval = (*send_func)(&partnotice, buffer, ret_len+message_len,
				   waitforack)) != ZERR_NONE) {
	    return (retval);
	}
	offset += fragsize;
	if (!notice->z_message_len)
	    break;
    }

    return (ZERR_NONE);
}

/*ARGSUSED*/
Code_t
Z_XmitFragment(ZNotice_t *notice, char *buf, int len, int wait)
{
	return(ZSendPacket(buf, len, wait));
}
