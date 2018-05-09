#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "random.h"

#include "net/af.h"
#include "net/conn/udp.h"
#include "net/ipv6/addr.h"
#include "thread.h"

#include "ccn-lite-riot.h"
#include "ccnl-pkt-ndntlv.h"
#include "ccnl-ext.h"

#include "copss_pkt_tlv.h"
#include "copss_headers.h"
#include "copss_util.h"
#include "copss_riot_ccn_lite.h"

#include "net/gnrc/netif.h"
#include "net/gnrc/netreg.h"
#include "net/gnrc/netif/hdr.h"
#include "net/packet.h"
#include "net/gnrc/pkt.h"

//# define PREFIX_BUFSIZE 50 //rewrite buf size
#define COPSS_MSG_QUEUE_SIZE   (8)
//#define BASE64SIZE CCNL_MAX_PACKET_SIZE * 2
//#define COPSS_BUFFER_SIZE      CCNL_MAX_PACKET_SIZE
static unsigned char _out[CCNL_MAX_PACKET_SIZE];
static unsigned char _int_buf[BUF_SIZE];
//static unsigned char base64_out[BASE64SIZE];
static bool server_running = false;
//static conn_udp_t conn;
//static char * str;
static sock_udp_t sock;
static unsigned char copss_buffer[CCNL_MAX_PACKET_SIZE];
static char copss_stack[THREAD_STACKSIZE_DEFAULT];
static char copss_if_stack[THREAD_STACKSIZE_DEFAULT];
static msg_t copss_msg_queue[COPSS_MSG_QUEUE_SIZE];
//static const char *_default_content = "COPSS";
/**
 * PID of the eventloop thread
 */
static kernel_pid_t _copss_event_loop_pid = KERNEL_PID_UNDEF;
static kernel_pid_t _copss_ccnl_riot_if_pid = KERNEL_PID_UNDEF;
static kernel_pid_t ccnl_riot_pid;
/**
 * @brief Central copss information
 */
struct copss_s copss;

#define MULTICAST_ENCAPSULATE_NAME   "multicast";
#define CONTROL_ENCAPSULATE_NAME  "control"
/**
 * currently configured suite
 */
static int _ccnl_suite = CCNL_SUITE_NDNTLV;

static gnrc_netreg_entry_t copss_ccnl_ne;

/**
 * @brief Some function pointers
 * @{
 */

typedef int (*ccnl_mkInterestFunc)(struct ccnl_prefix_s*, int*, unsigned char*,
		int);
typedef int (*ccnl_isContentFunc)(unsigned char*, int);

extern ccnl_mkInterestFunc ccnl_suite2mkInterestFunc(int suite);
extern ccnl_isContentFunc ccnl_suite2isContentFunc(int suite);
//extern char *base64_encode(const char *data, size_t input_length, size_t *output_length);
//extern unsigned char *base64_decode(const char *data, size_t input_length, size_t *output_length);

/**
 * @brief function prototypes required by ccnl-core.c
 * @{
 */
void free_packet(struct ccnl_pkt_s *pkt);
//-------------------------------------------------

//'-' replace '/'
static char encoding_table[] = { 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I',
		'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',
		'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k',
		'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y',
		'z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '-' };

static char *decoding_table = NULL;
static int mod_table[] = { 0, 2, 1 };

static void base64_build_decoding_table(void) {

	decoding_table = (char *) malloc(256);
	int i;
	for (i = 0; i < 64; i++)
		decoding_table[(unsigned char) encoding_table[i]] = i;
}

static char *base64_encode(const char *data, size_t input_length,
		size_t *output_length) {

	*output_length = 4 * ((input_length + 2) / 3) + 1;

	char *encoded_data = (char *) malloc(*output_length);
	memset(encoded_data, '\0', *output_length);
	if (encoded_data == NULL)
		return NULL;
	int i, j;
	for (i = 0, j = 0; (unsigned int) i < input_length;) {

		uint32_t octet_a =
				(unsigned int) i < input_length ? (unsigned char) data[i++] : 0;
		uint32_t octet_b =
				(unsigned int) i < input_length ? (unsigned char) data[i++] : 0;
		uint32_t octet_c =
				(unsigned int) i < input_length ? (unsigned char) data[i++] : 0;

		uint32_t triple = (octet_a << 0x10) + (octet_b << 0x08) + octet_c;

		encoded_data[j++] = encoding_table[(triple >> 3 * 6) & 0x3F];
		encoded_data[j++] = encoding_table[(triple >> 2 * 6) & 0x3F];
		encoded_data[j++] = encoding_table[(triple >> 1 * 6) & 0x3F];
		encoded_data[j++] = encoding_table[(triple >> 0 * 6) & 0x3F];
	}
	for (i = 0; i < mod_table[input_length % 3]; i++)
		encoded_data[*output_length - 2 - i] = '=';

	return encoded_data;
}

static unsigned char *base64_decode(const char *data, size_t input_length,
		size_t *output_length) {
	uint32_t i, j;
	if (decoding_table == NULL)
		base64_build_decoding_table();

	if (input_length % 4 != 0)
		return NULL;

	*output_length = input_length / 4 * 3;
	if (data[input_length - 1] == '=')
		(*output_length)--;
	if (data[input_length - 2] == '=')
		(*output_length)--;

	unsigned char *decoded_data = (unsigned char *) malloc(*output_length);
	if (decoded_data == NULL)
		return NULL;

	for (i = 0, j = 0; i < input_length;) {

		uint32_t sextet_a =
				data[i] == '=' ?
						0 & (int32_t) i++ :
						decoding_table[(uint32_t) data[i++]];
		uint32_t sextet_b =
				data[i] == '=' ?
						0 & (int32_t) i++ :
						decoding_table[(uint32_t) data[i++]];
		uint32_t sextet_c =
				data[i] == '=' ?
						0 & (int32_t) i++ :
						decoding_table[(uint32_t) data[i++]];
		uint32_t sextet_d =
				data[i] == '=' ?
						0 & (int32_t) i++ :
						decoding_table[(uint32_t) data[i++]];

		uint32_t triple = (sextet_a << 3 * 6) + (sextet_b << 2 * 6)
				+ (sextet_c << 1 * 6) + (sextet_d << 0 * 6);

		if (j < *output_length)
			decoded_data[j++] = (triple >> 2 * 8) & 0xFF;
		if (j < *output_length)
			decoded_data[j++] = (triple >> 1 * 8) & 0xFF;
		if (j < *output_length)
			decoded_data[j++] = (triple >> 0 * 8) & 0xFF;
	}

	return decoded_data;
}

void copss_ccnl_interest_remove(struct ccnl_relay_s* relay) {

	struct ccnl_interest_s *i;
	struct ccnl_face_s *f;
	for (f = relay->faces; f; f = f->next) {
		f->flags &= ~CCNL_FACE_FLAGS_SERVED; // reply on a face only once
	}
	char* muticast_encap_name = MULTICAST_ENCAPSULATE_NAME
	;
	char* control_encap_name = CONTROL_ENCAPSULATE_NAME;
	for (i = relay->pit; i;) {
		//struct ccnl_pendint_s *pi;
		if (!i->pkt->pfx)
			continue;
		switch (i->pkt->pfx->suite) {
		case CCNL_SUITE_NDNTLV:

			if (i->pkt->pfx->compcnt >= 1
					&& (memcmp(i->pkt->pfx->comp[1], muticast_encap_name,
							strlen(muticast_encap_name)) == 0
							|| memcmp(i->pkt->pfx->comp[1], control_encap_name,
									strlen(control_encap_name)) == 0)) {
				//                    printf("copss_ccnl_interest_remove: confirmed it is a muticast_pkt \n");

			} else {

				i = i->next;
				continue;
			}
			break;
		default:
			i = i->next;
			continue;
		}
		i = ccnl_interest_remove(relay, i);
	}

}

void _handle_multicast(struct copss_pkt_s *copkt, struct copss_face_s *cf_from,
		unsigned char *orignal_data, int datalen) {
	//    puts("\n_handle_multicast");
	//    if (cf_from)
	//        printf("_handle_multicast: cf_from->is_router_flag:%c\n", cf_from->is_router_flag);
	if (cf_from == NULL || cf_from->is_router_flag == '1') {
		// If from a router or from RP, do multicast
		// puts("_handle_multicast: this pkt comes from a router or RP");
		for (struct copss_face_s* cfm = copss.faces; cfm; cfm = cfm->next) {
			cfm->flags &= ~CCNL_FACE_FLAGS_SERVED; // forward on a copss face only once
		}
		for (struct copss_contentName_s *cn = copkt->content_name_add; cn; cn =
				cn->next) {
			for (struct copss_face_s* cfm = copss.faces; cfm; cfm = cfm->next) {
				if (!cfm->sub_conentName)
					continue;
				if (cfm->flags & CCNL_FACE_FLAGS_SERVED)
					continue;
				//bool match = false;
				for (struct copss_contentName_s* sub_cn = cfm->sub_conentName;
						sub_cn; sub_cn = sub_cn->next) {
					int res;
					int rc = 0;
					rc = ccnl_prefix_cmp(sub_cn->prefix, NULL, cn->prefix,
					CMP_LONGEST);
					//                    printf("\nrc=%d/%d\n", rc, sub_cn->prefix->compcnt);

					if (rc < sub_cn->prefix->compcnt)
						continue;
					//                    str = ccnl_prefix_to_path(cn->prefix);
					//                    printf(" matched, prefix==%s\n", str);
					//                    free(str);
					//                    str = NULL;
					//match = true;
					//send to remote
					cfm->flags |= CCNL_FACE_FLAGS_SERVED;
					if ((res = sock_udp_send(&sock, orignal_data, datalen,
							&cfm->peer)) < 0) {
						puts("could not send");
					} else {
					}

					//                    else {
					//                        ipv6_addr_t remote_ipv6;
					//                        memcpy(remote_ipv6.u8, cfm->peer.addr.ipv6, sizeof (cfm->peer.addr.ipv6));
					//                        static char str_addr[MAX_ADDR_PRINT_LEN * 3];
					//
					//                        ipv6_addr_to_str(str_addr, &remote_ipv6, sizeof (str_addr));
					//                        printf("_handle_multicast: Success send to remote: send %u byte to %s:%d\n", (unsigned) res, str_addr, cfm->peer.port);
					//                    }

					break;
				}
				/*
				 if (!match)
				 continue;
				 break;
				 */
			}
		}

	} else {
		// puts("\n it come from end host, encapsulate and send to ccnlite");
		/*If it comes from an end host (I'm the 1st hop router): encapsulate the
		 multicast into (multiple) Interest(s), write to ccnlite*/
		struct ccnl_face_s *ccnl_f_from = copss_find_ccnlf_by_cf(&ccnl_relay,
				cf_from);
		if (!ccnl_f_from) {
			return;
		}
		size_t base64_out_size;
		//= BASE64SIZE;
		struct copss_contentNames_table_s* p, *RP2CD_table;
		RP2CD_table = splitContentNames(copss.CD2RP_table,
				copkt->content_name_add);
		if (!RP2CD_table) {
			printf("RP2CD_table is null. no matching\n");
		}
		for (p = RP2CD_table; p; p = p->next) {
			//            puts("1st encapsulate");
			//encapsulate and send to ccnl
			//memset(_out, '\0', CCNL_MAX_PACKET_SIZE);
			// memset(base64_out, '\0', base64_out_size);
			int offs = CCNL_MAX_PACKET_SIZE;
			int arg_len;
			arg_len = copss_tlv_prependMulticast(p->content_name,
					copkt->content, copkt->contlen, &offs, _out);
			unsigned char *olddata;
			unsigned char *data = olddata = _out + offs;
			int old_len = arg_len;
			int len;
			unsigned typ;
			if (ccnl_ndntlv_dehead(&data, &arg_len, (int*) &typ, &len) ||
			typ != COPSS_TLV_Multicast) {
				printf("handle mutilcast pkt error: unable to dehead");
				return;
			}

			/*if (!base64_encode(olddata, old_len, base64_out, &base64_out_size) == BASE64_SUCCESS) {
			 printf("handle mutilcast pkt error: unable to base64 encode");
			 return;

			 }*/
			char* base64_out = base64_encode((char*) olddata, (size_t) old_len,
					&base64_out_size);
			if (!base64_out) {
				printf("handle mutilcast pkt error: unable to base64 encode");
				return;
			}
			char* muticast_encap_name = MULTICAST_ENCAPSULATE_NAME
			;
			int suite = CCNL_SUITE_NDNTLV;

			memset(_int_buf, '\0', BUF_SIZE);

			char *RP_name_path = ccnl_prefix_to_path(p->prefix);

			int uri_len = strlen(RP_name_path) + strlen(muticast_encap_name)
					+ base64_out_size + 3;
			char uri[uri_len];
			sprintf(uri, "%s/%s/%s", RP_name_path, muticast_encap_name,
					base64_out);
			if (base64_out) {
				free(base64_out);
				base64_out = NULL;
			}
			free(RP_name_path);
			RP_name_path = NULL;
			struct ccnl_prefix_s *prefix = ccnl_URItoPrefix(&uri[0], suite,
					NULL, NULL);
			//            puts("2nd send to ccnlite");
			copss_send_interest(&ccnl_relay, prefix, ccnl_f_from, _int_buf,
			BUF_SIZE);
		}
		free_copss_contentName_table(RP2CD_table);
	}
	copss_ccnl_interest_remove(&ccnl_relay);
}

int _control_encapsulate_send(struct ccnl_prefix_s* RP_prefix,
		struct copss_contentName_s* contentName_add,
		struct copss_contentName_s* contentName_remove,
		struct ccnl_face_s *ccnl_f_from) {

	//    str = ccnl_prefix_to_path(RP_prefix);
	//    printf("_control_encapsulate_send: RP name: %s  ccnl face=%d \n", str, ccnl_f_from->faceid);
	//    free(str);
	//    str = NULL;

	char* control_encap_name = CONTROL_ENCAPSULATE_NAME;
	int suite = CCNL_SUITE_NDNTLV;
	enum copss_control_type_e type;
	int version = 0;
	int ttl = -1;
	type = ST_Change;
	size_t base64_out_size; //= BASE64SIZE;
	memset(_out, '\0', CCNL_MAX_PACKET_SIZE);
	//memset(base64_out, '\0', base64_out_size);

	int arg_len;
	int offs = CCNL_MAX_PACKET_SIZE;
	arg_len = copss_tlv_prependControl(type, contentName_add,
			contentName_remove, &version, &ttl, &offs, _out);
	unsigned char *olddata;
	unsigned char *data = olddata = _out + offs;

	int len, old_len = arg_len;
	unsigned typ;
	if (ccnl_ndntlv_dehead(&data, &arg_len, (int*) &typ, &len) ||
	typ != COPSS_TLV_Control) {
		return -1;
	}

	/*if (!base64_encode(olddata, old_len, base64_out, &base64_out_size) == BASE64_SUCCESS) {
	 printf("handle control pkt error: unable to base64 encode");
	 return -2;

	 }*/
	//    printf("try to base64 encode\n");
	char* base64_out = base64_encode((char*) olddata, (size_t) old_len,
			&base64_out_size);
	if (!base64_out) {
		printf("handle mutilcast pkt error: unable to base64 encode\n");
		return -2;
	}
	//    printf("base64 encode : base64_out:%s\n", base64_out);

	memset(_int_buf, '\0', BUF_SIZE);

	char *RP_name_path = ccnl_prefix_to_path(RP_prefix);

	int uri_len = strlen(RP_name_path) + strlen(control_encap_name)
			+ base64_out_size + 3;
	char uri[uri_len];

	sprintf(uri, "%s/%s/%s", RP_name_path, control_encap_name, base64_out);
	if (base64_out) {
		free(base64_out);
		base64_out = NULL;
	}
	free(RP_name_path);
	RP_name_path = NULL;

	//    puts("ccnl_URItoPrefix");
	struct ccnl_prefix_s *prefix = ccnl_URItoPrefix(&uri[0], suite, NULL, NULL);

	//    str = ccnl_prefix_to_path(prefix);
	//    printf("\ntry to copss_send_interest: prefix: %s  ccnl face/ccnl_f_from=%d \n\n", str, ccnl_f_from->faceid);
	//    free(str);
	//    str = NULL;

	copss_send_interest(&ccnl_relay, prefix, ccnl_f_from, _int_buf, BUF_SIZE);
	return 0;
}

void _handle_control(struct copss_pkt_s *copkt, struct copss_face_s *cf_from) {

	struct copss_contentName_s* res_cn_add, *res_cn_rem;
	switch (copkt->control_type) {
	case ST_Change:
		//            printf("\n _handle_control: control type: %s\n", "ST_change");
		//            // modify ST
		//            puts("try to modify ST, check content_name_add");
		res_cn_add = NULL, res_cn_rem = NULL;
		for (struct copss_contentName_s* cn_add = copkt->content_name_add;
				cn_add; cn_add = cn_add->next) {
			bool hasSameCD = false, alreadySubscribed = false;
			for (struct copss_face_s* cf = copss.faces; cf; cf = cf->next) {
				if (!cf->sub_conentName)
					continue;
				//                    puts("\nfor cf = copss.faces");
				for (struct copss_contentName_s* cn_sub = cf->sub_conentName;
						cn_sub; cn_sub = cn_sub->next) {
					//                        puts("for cn_sub = cf->sub_conentName");
					//                        str = ccnl_prefix_to_path(cn_sub->prefix);
					//                        printf("cn_sub->prefix:%s\n", str);
					//                        free(str);
					//                        str = NULL;

					int rc = 0;
					rc = ccnl_prefix_cmp(cn_sub->prefix, NULL, cn_add->prefix,
					CMP_LONGEST);
					//                        printf("_handle_control:modify ST:ccnl_prefix_cmp: rc=%d/%d\n", rc, cn_sub->prefix->compcnt);
					if (rc < cn_sub->prefix->compcnt)
						continue;
					//                        str = ccnl_prefix_to_path(cn_sub->prefix);
					//                        printf(" found matched, prefix==%s\n", str);
					//                        free(str);
					//                        str = NULL;

					if (ccnl_prefix_cmp(cn_sub->prefix, NULL, cn_add->prefix,
					CMP_EXACT) == 0) {
						hasSameCD = true;
						if (cf == cf_from) {
							//                                printf("hasSameCD = true; and already subed: alreadySubscribed = true\n");
							alreadySubscribed = true;
						}
					}
				}
			}
			if (!alreadySubscribed) {
				struct copss_contentName_s * cn_add_temp =
						(struct copss_contentName_s *) calloc(1,
								sizeof(struct copss_contentName_s));
				cn_add_temp->prefix = ccnl_prefix_dup(cn_add->prefix);
				DBL_LINKED_LIST_ADD(cf_from->sub_conentName, cn_add_temp);
				//                    printf("new sub add to cf_from\t");
				if (!hasSameCD) {
					struct copss_contentName_s * cn_add_temp1 =
							(struct copss_contentName_s *) calloc(1,
									sizeof(struct copss_contentName_s));
					cn_add_temp1->prefix = ccnl_prefix_dup(cn_add->prefix);
					DBL_LINKED_LIST_ADD(res_cn_add, cn_add_temp1);
					//                        printf("hasSameCD=false: add to res_cn_add\n");
				}
			}
		}
		//            puts("\ntry to modify ST, check content_name_remove");
		for (struct copss_contentName_s* cn_rem = copkt->content_name_remove;
				cn_rem; cn_rem = cn_rem->next) {
			bool ematch = false; // check if the subscriber really subscribed to the CD
			for (struct copss_face_s* cf = copss.faces; cf; cf = cf->next) {
				if (!cf->sub_conentName)
					continue;
				struct copss_contentName_s * cn_sub_temp;
				for (struct copss_contentName_s* cn_sub = cf->sub_conentName;
						cn_sub; cn_sub = cn_sub->next) {
					if (ccnl_prefix_cmp(cn_sub->prefix, NULL, cn_rem->prefix,
					CMP_EXACT) == 0) {

						//                            str = ccnl_prefix_to_path(cn_sub->prefix);
						//                            printf(" cn_rem matched, prefix==%s\n", str);
						//                            free(str);
						//                            str = NULL;

						if (cf == cf_from) {
							//the subscriber really subscribed to the CD
							cn_sub_temp = cn_sub;
							ematch = true;
							break;
						}
					}
				}
				if (ematch) {
					//the subscriber really subscribed to the CD
					//                        puts("the subscriber really subscribed to the CD, try to remove");
					DBL_LINKED_LIST_REMOVE(cf->sub_conentName, cn_sub_temp);
					cn_sub_temp->next = NULL;
					cn_sub_temp->prev = NULL;
					free_copss_contentName(cn_sub_temp);
					break;
				}
			}

			if (ematch) {
				//the subscriber did really subscribe to the CD
				// if nobody subscribed to the same CD, continue unsubscription
				ematch = false;
				for (struct copss_face_s* cf = copss.faces; cf; cf = cf->next) {
					if (!cf->sub_conentName)
						continue;
					for (struct copss_contentName_s* cn_sub = cf->sub_conentName;
							cn_sub; cn_sub = cn_sub->next) {
						if (ccnl_prefix_cmp(cn_sub->prefix, NULL,
								cn_rem->prefix, CMP_EXACT) == 0) {
							ematch = true;
							break;
						}

						/*
						 int rc = 0;
						 rc = ccnl_prefix_cmp(cn_sub->prefix, NULL, cn_rem->prefix, CMP_LONGEST);
						 printf(" _handle_control, rc=%d/%d\n", rc, cn_sub->prefix->compcnt);
						 if (rc < cn_sub->prefix->compcnt)
						 continue;

						 str = ccnl_prefix_to_path(cn_sub->prefix);
						 printf("_handle_control matched, subscriber really subscribed to the CD :prefix==%s\n", str);
						 free(str);
						 str = NULL;

						 lmatch = true;
						 if (!ccnl_prefix_cmp(cn_sub->prefix, NULL, cn_rem->prefix, CMP_EXACT) == 0) {
						 add = true;
						 break;
						 } else
						 continue;
						 */
					}
					if (ematch)
						break;
				}
				if (!ematch) {
					// if nobody subscribed to the same CD, continue unsubscription
					struct copss_contentName_s * cn_rem_temp =
							(struct copss_contentName_s *) calloc(1,
									sizeof(struct copss_contentName_s));
					cn_rem_temp->prefix = ccnl_prefix_dup(cn_rem->prefix);
					DBL_LINKED_LIST_ADD(res_cn_rem, cn_rem_temp);
				}
			}

		}
		if (!res_cn_add && !res_cn_rem) {
			//                puts("res_cn_add&&res_cn_rem are both null");
			break;
		}
		struct ccnl_face_s *ccnl_f_from = copss_find_ccnlf_by_cf(&ccnl_relay,
				cf_from);
		if (!ccnl_f_from) {
			puts("ccnl_f_from is null");
			break;
		}
		// Split, encapsulate and forward
		//            puts("\n_handle_control: try to Split, encapsulate and forward");
		struct copss_contentNames_table_s* p, *RP2CD__add_table;
		RP2CD__add_table = splitContentNames(copss.CD2RP_table, res_cn_add);
		if (!RP2CD__add_table) {
			//                printf("_handle_control: RP2CD__add_table is null. no matching\n");
		}
		if (res_cn_add) {
			free_copss_contentName(res_cn_add);
		}

		struct copss_contentNames_table_s* RP2CD__rm_table;
		RP2CD__rm_table = splitContentNames(copss.CD2RP_table, res_cn_rem);
		if (!RP2CD__rm_table) {
			//                printf("_handle_control: RP2CD__rm_table is null. no matching\n");
		}
		if (res_cn_rem) {
			free_copss_contentName(res_cn_rem);
		}

		struct copss_contentNames_table_s* p_add = RP2CD__add_table;
		while (p_add) {
			struct copss_contentNames_table_s* p_rm = RP2CD__rm_table;
			bool match = false;
			while (p_rm) {
				if (ccnl_prefix_cmp(p_add->prefix, NULL, p_rm->prefix,
				CMP_EXACT)) {
					//same RP, encapsulate and send to ccnl
					_control_encapsulate_send(p_add->prefix,
							p_add->content_name, p_rm->content_name,
							ccnl_f_from);
					struct copss_contentNames_table_s* p_rm_temp = p_rm;
					p_rm = p_rm->next;
					DBL_LINKED_LIST_REMOVE(RP2CD__rm_table, p_rm_temp);
					p_rm_temp->next = NULL;
					p_rm_temp->prev = NULL;
					free_copss_contentName(p_rm_temp->content_name);
					free_prefix(p_rm_temp->prefix);
					p_rm_temp->prefix = NULL;
					// free(p_rm_temp->content_name);
					// p_rm_temp->content_name = NULL;
					free(p_rm_temp);
					p_rm_temp = NULL;
					match = true;
					break;
				}
				p_rm = p_rm->next;
			}
			if (match) {
				struct copss_contentNames_table_s* p_add_temp = p_add;
				p_add = p_add->next;
				DBL_LINKED_LIST_REMOVE(RP2CD__add_table, p_add_temp);
				p_add_temp->next = NULL;
				p_add_temp->prev = NULL;
				free_copss_contentName(p_add_temp->content_name);
				free_prefix(p_add_temp->prefix);
				p_add_temp->prefix = NULL;
				//free(p_add_temp->content_name);
				//p_add_temp->content_name = NULL;
				free(p_add_temp);
				p_add_temp = NULL;
				continue;
			}
			p_add = p_add->next;
		}
		if (RP2CD__add_table) {
			p = RP2CD__add_table;
			while (p) {
				//                    str = ccnl_prefix_to_path(p->prefix);
				//                    printf("_handle_control: RP2CD__add_table: try to call _control_encapsulate_send: RP name: %s  ccnl face=%d \n", str, ccnl_f_from->faceid);
				//                    free(str);
				//                    str = NULL;

				_control_encapsulate_send(p->prefix, p->content_name, NULL,
						ccnl_f_from);
				//                    puts("_control_encapsulate_send done");
				struct copss_contentNames_table_s* p_temp = p;

				p = p->next;
				//free_prefix(p_temp->prefix);
				p_temp->prefix = NULL;
				//puts("free_prefix(p_temp->prefix); done ");
				free_copss_contentName(p_temp->content_name);
				// puts("free_copss_contentName(p_temp->content_name); done");
				// free(p_temp->content_name);
				// p_temp->content_name = NULL;
				free(p_temp);
				p_temp = NULL;
				// puts("free RP2CD__add_table p_temp done;");
			}
		}
		if (RP2CD__rm_table) {
			p = RP2CD__rm_table;
			while (p) {
				_control_encapsulate_send(p->prefix, NULL, p->content_name,
						ccnl_f_from);
				struct copss_contentNames_table_s* p_temp = p;
				p = p->next;
				free_copss_contentName(p_temp->content_name);
				free_prefix(p_temp->prefix);
				p_temp->prefix = NULL;
				//free(p_temp->content_name);
				//p_temp->content_name = NULL;
				free(p_temp);
				p_temp = NULL;
				// puts("free RP2CD__rm_table p_temp done;");
			}
		}
		break;
	case FIB_Change:
		//TODO
		printf("control type: %s\n", "FIB_change");
		break;
	default:
		printf("control type is invalid\n");
		break;
	}

}

void _receive_from_ccnl_riot(struct copss_s *copss_p, msg_t *m) {
	/* packet parsing */
	gnrc_pktsnip_t *pkt = (gnrc_pktsnip_t *) m->content.ptr;

	//    printf("\n copss-ccn if:[_receive_from_ccnl_riot]  RIOT pkt total count %d : \n", gnrc_pkt_count(pkt));
	//    gnrc_pktsnip_t *p;
	//    for (p = pkt; p; p = p->next) {
	//        printf("[size %d : %p]\t", p->size, (void*) p);
	//    }
	gnrc_pktsnip_t *ccn_pkt, *netif_pkt;
	LL_SEARCH_SCALAR(pkt, ccn_pkt, type, GNRC_NETTYPE_CCN);
	LL_SEARCH_SCALAR(pkt, netif_pkt, type, GNRC_NETTYPE_NETIF);

	//printf("_receive_from_ccnl_riot ccn_pkt size %d\n", ccn_pkt->size);
	gnrc_netif_hdr_t *nethdr = (gnrc_netif_hdr_t *) netif_pkt->data;
	struct sockaddr_ll dest;
	memset(&dest, 0, sizeof(dest));
	dest.sll_family = AF_PACKET;
	dest.sll_halen = nethdr->dst_l2addr_len;
	memcpy(dest.sll_addr, gnrc_netif_hdr_get_dst_addr(nethdr),
			nethdr->dst_l2addr_len);

	struct copss_face_s *cf;
	int res;
	if (nethdr->flags == GNRC_NETIF_HDR_FLAGS_BROADCAST) {
		for (cf = copss_p->faces; cf; cf = cf->next) {
			//            printf("\ncopss-ccn if:BROADCAST: try to send %d byte CCN_PKT to remote\n", ccn_pkt->size);
			if ((res = sock_udp_send(&sock, ccn_pkt->data, ccn_pkt->size,
					&cf->peer)) < 0) {
				puts("could not send");
			} else {
				//                ipv6_addr_t remote_ipv6;
				//                memcpy(remote_ipv6.u8, cf->peer.addr.ipv6, sizeof (cf->peer.addr.ipv6));
				//                char str_addr[MAX_ADDR_PRINT_LEN * 3];
				//
				//                ipv6_addr_to_str(str_addr, &remote_ipv6, sizeof (str_addr));
				//                printf("BROADCAST Success: send %u byte to %s:%d\n", (unsigned) res, str_addr, cf->peer.port);
			}
		}
	} else {
		//  ipv6_addr_t src = IPV6_ADDR_UNSPECIFIED;
		//find the matched faces in copss
		for (cf = copss_p->faces; cf; cf = cf->next) {
			if (cf->copss_ccnl_riot_f_conn.sll_halen
					== nethdr->dst_l2addr_len) {
				if (memcmp(cf->copss_ccnl_riot_f_conn.sll_addr, dest.sll_addr,
						dest.sll_halen) == 0) {
					//                    printf("\ncopss-ccn if: try to send %d byte CCN_PKT to remote\n", ccn_pkt->size);
					//send it to remote
					if ((res = sock_udp_send(&sock, ccn_pkt->data,
							ccn_pkt->size, &cf->peer)) < 0) {
						puts("could not send");
					} else {
					}
					//
					//                        ipv6_addr_t remote_ipv6;
					//                        memcpy(remote_ipv6.u8, cf->peer.addr.ipv6, sizeof (cf->peer.addr.ipv6));
					//                        char str_addr[MAX_ADDR_PRINT_LEN * 3];
					//
					//                        ipv6_addr_to_str(str_addr, &remote_ipv6, sizeof (str_addr));
					//                        printf("Success: send %u byte to %s:%d\n", (unsigned) res, str_addr, cf->peer.port);
					//
					break;
				}

			}
		}

		//check if define RP in copss
		//        puts("check if define RP in copss");
		if (copss.is_has_RP_flag == '1') {
			//check if it is a PKT for RP
			//            puts("check if it is a PKT for RP");
			if (copss.copss_ccnl_riot_RP2f_conn.sll_halen == dest.sll_halen) {
				if (memcmp(copss.copss_ccnl_riot_RP2f_conn.sll_addr,
						dest.sll_addr, dest.sll_halen) == 0) {
					//try decapsulate
					int len, datalen = ccn_pkt->size;
					unsigned int typ;
					unsigned char *olddata;
					unsigned char *data = olddata = ccn_pkt->data;

					struct ccnl_pkt_s *ccpkt;
					unsigned char * multicast_data, *base64_data;
					//size_t base64_out_size= BASE64SIZE;
					size_t base64_out_size, base64_len;

					if (ccnl_ndntlv_dehead(&data, &datalen, (int*) &typ, &len)
							== 0 && typ == NDN_TLV_Interest) {
						//                        printf("decapsulate :it is a NDN_TLV_Interest\n");
						ccpkt = ccnl_ndntlv_bytes2pkt(typ, olddata, &data,
								&datalen);
						if (!ccpkt) {
							//                            printf("  ndntlv packet coding problem\n");
							gnrc_pktbuf_release(pkt);
							return;
						}
						ccpkt->type = typ;
						char* muticast_encap_name = MULTICAST_ENCAPSULATE_NAME
						;
						if (ccpkt->pfx->compcnt >= 1
								&& memcmp(ccpkt->pfx->comp[1],
										muticast_encap_name,
										strlen(muticast_encap_name)) == 0) {
							//                            printf("confirmed it is a muticast_pkt for RP try to base64 decode\n");
							base64_data = ccpkt->pfx->comp[2];
							base64_len = ccpkt->pfx->complen[2];

							/* memset(base64_out, '\0', base64_out_size);
							 if (!base64_decode(base64_data, base64_len, base64_out, &base64_out_size) == BASE64_SUCCESS) {
							 printf("handle mutilcast pkt error: unable to base64 encode");
							 gnrc_pktbuf_release(pkt);
							 return;
							 } */

							unsigned char* base64_out = base64_decode(
									(char*) base64_data, base64_len,
									&base64_out_size);
							if (!base64_out) {
								printf(
										"handle mutilcast pkt error: unable to base64 encode\n");

								if (ccpkt) {
									free_packet(ccpkt);
									ccpkt = NULL;
								}
								gnrc_pktbuf_release(pkt);
								return;
							} else {

								unsigned char* temp_data = multicast_data =
										base64_out;
								int temp_len = base64_out_size;
								//                                printf("ccnl_ndntlv_dehead \n");
								if (ccnl_ndntlv_dehead(&temp_data, &temp_len,
										(int*) &typ, &len) ||
										typ != COPSS_TLV_Multicast) {
									printf(
											"handle mutilcast pkt error: unable to dehead\n");
									if (base64_out) {
										free(base64_out);
										base64_out = NULL;
									}
									if (ccpkt) {
										free_packet(ccpkt);
										ccpkt = NULL;
									}
									gnrc_pktbuf_release(pkt);
									return;
								} else {
									struct copss_pkt_s *copkt =
											copss_tlv_bytes2pkt(multicast_data,
													&temp_data, &temp_len);
									//print_copss_pkt(copkt);
									//                                    printf("RP: try to call _handle_multicast\n");
									_handle_multicast(copkt, NULL,
											multicast_data, base64_out_size);
									if (base64_out) {
										free(base64_out);
										base64_out = NULL;
									}

									if (copkt)
										free_copss_packet(copkt);
								}
							}
						}
						//                        else
						//                            printf("it is not a pkt for RP: no MULTICAST_ENCAPSULATE_NAME\n");
						if (ccpkt) {
							free_packet(ccpkt);
							ccpkt = NULL;
						}

					}
				}
				//not support pkt, discard it
			}
		}

	}
	copss_ccnl_interest_remove(&ccnl_relay);
	gnrc_pktbuf_release(pkt);
}

void _receive_from_remote(msg_t * m) {

	//forward to ccnlite riot main thread
	gnrc_pktsnip_t *riot_pkt = (gnrc_pktsnip_t *) m->content.ptr;
	//    printf("\n copss-ccn if:[_receive_from_remote]  forward to ccnlRIOT event: RIOT pkt total count %d : \n", gnrc_pkt_count(riot_pkt));
	//    gnrc_pktsnip_t *p;
	//    for (p = riot_pkt; p; p = p->next) {
	//        printf("[size %d : %p]\t", p->size, (void*) p);
	//    }
	if (gnrc_netapi_receive(ccnl_riot_pid, riot_pkt) < 1) {
		puts(
				"error: COPSS_CCNlite IF: unable to forward ccnl packet to CCCNliteRiot, discard it\n");
		gnrc_pktbuf_release(riot_pkt);
	}
}

static void* _copss_ccnl_riot_if_event_loop(void *arg) {

	msg_init_queue(copss_msg_queue, COPSS_MSG_QUEUE_SIZE);
	struct copss_s * copss_p = (struct copss_s*) arg;

	while (1) {
		msg_t m, reply;
		/* start periodic timer */
		reply.type = CCNL_MSG_AGEING;
		msg_receive(&m);

		switch (m.type) {
		case GNRC_NETAPI_MSG_TYPE_RCV:
			// puts("copss-ccn-lite if: GNRC_NETAPI_MSG_TYPE_RCV received");
			_receive_from_remote(&m);
			break;

		case GNRC_NETAPI_MSG_TYPE_SND:
			//puts("copss if ccn-lite: GNRC_NETAPI_MSG_TYPE_SND received");
			//send to remote by copss with UDP
			_receive_from_ccnl_riot(copss_p, &m);
			break;

		case GNRC_NETAPI_MSG_TYPE_GET:
		case GNRC_NETAPI_MSG_TYPE_SET:
			puts("copss if ccn-lite: reply to unsupported get/set\n");
			reply.content.value = -ENOTSUP;
			msg_reply(&m, &reply);
			break;
		default:
			puts("copss if ccn-lite: unknown message type\n");
			break;
		}
	}
	return NULL;
}

/* the copss smain event-loop */
void* _copss_event_loop(void *arg) {
	struct copss_s * copss_p = (struct copss_s*) arg;

	sock_udp_ep_t local = SOCK_IPV6_EP_ANY;

	local.port = copss_p->port;
	if (sock_udp_create(&sock, &local, NULL, 0) < 0) {
		puts("Error creating UDP sock");
		return NULL;
	}

	server_running = true;

	printf("Success: started COPSS UDP server on port %" PRIu8 "\n", copss.port);

	while (!copss_p->halt_flag) {
		sock_udp_ep_t remote;
		ssize_t res;
		if ((res = sock_udp_recv(&sock, copss_buffer, sizeof(copss_buffer),
		SOCK_NO_TIMEOUT, &remote)) < 0) {
			puts("Error while receiving");

		} else if (res == 0) {
			puts("No data received");
		} else {

			ipv6_addr_t peer_ipv6;
			char str_addr[MAX_ADDR_PRINT_LEN * 3];

			//find copss face
			struct copss_face_s *cf;
			//  ipv6_addr_t src = IPV6_ADDR_UNSPECIFIED;
			//find the matched faces in copss
			for (cf = copss.faces; cf; cf = cf->next) {
				if (cf->peer.port == remote.port) {
					if (memcmp(&(cf->peer.addr.ipv6), &(remote.addr.ipv6),
							sizeof(remote.addr.ipv6)) == 0) {
						memcpy(peer_ipv6.u8, cf->peer.addr.ipv6,
								sizeof(cf->peer.addr.ipv6));
						ipv6_addr_to_str(str_addr, &peer_ipv6,
								sizeof(str_addr));
						//                        printf("\n COPSS main event: Get a pkt, from the matched cf: Peer addr: %s port: %d \n", str_addr, cf->peer.port);
						break;
					}
				}
			}

			if (!cf) {
				puts("Error: no suitable copss face found");
				// no suitable copss face found
				return NULL;
			}

			int len, datalen = res;
			unsigned int typ;
			unsigned char *olddata;
			unsigned char *data = olddata = copss_buffer;
			struct copss_pkt_s *copkt;
			struct ccnl_pkt_s *ccpkt;
			char* control_encap_name;
			gnrc_pktsnip_t *hdr = NULL;
			gnrc_pktsnip_t *riot_pkt;
			if (ccnl_ndntlv_dehead(&data, &datalen, (int*) &typ, &len) == 0) {
				// printf("dehead, type: %x \n", typ);
				switch (typ) {
				case COPSS_TLV_Control:
					//printf("Control pkt: res: %d\n", datalen);
					copkt = copss_tlv_bytes2pkt(olddata, &data, &datalen);
					//print pkt
					//                        print_copss_pkt(copkt);
					_handle_control(copkt, cf);
					//puts("COPSS_TLV_Control: _handle_control done");
					free_copss_packet(copkt);
					break;

				case COPSS_TLV_Multicast:
					//puts("multicast");
					copkt = copss_tlv_bytes2pkt(olddata, &data, &datalen);
					//print pkt
					//                        print_copss_pkt(copkt);
					_handle_multicast(copkt, cf, olddata, res);

					free_copss_packet(copkt);
					break;

				case NDN_TLV_Interest:
					// Check if it is an encapsulated Control
					//                        printf(" copss main event Check if it is an encapsulated Control\n");
					ccpkt = ccnl_ndntlv_bytes2pkt(typ, olddata, &data,
							&datalen);
					if (!ccpkt) {
						printf(
								"  copss main event NDN_TLV_Interest coding problem\n");
						break;
					}
					control_encap_name = CONTROL_ENCAPSULATE_NAME;
					if (ccpkt->pfx->compcnt >= 1
							&& memcmp(ccpkt->pfx->comp[1], control_encap_name,
									strlen(control_encap_name)) == 0) {
						//confirmed
						//                            printf("confirm it is an encapsulated Control, try to base64 decode\n");
						//int base64_len;

						unsigned char *control_data, *base64_data;
						//size_t base64_out_size = BASE64SIZE;
						size_t base64_out_size, base64_len;
						base64_data = ccpkt->pfx->comp[2];
						base64_len = ccpkt->pfx->complen[2];

						/*memset(base64_out, '\0', base64_out_size);
						 if (!base64_decode(base64_data, base64_len, base64_out, &base64_out_size) == BASE64_SUCCESS) {
						 printf("handle control pkt error: unable to base64 encode");
						 } */

						unsigned char* base64_out = base64_decode(
								(char*) base64_data, base64_len,
								&base64_out_size);
						if (!base64_out) {
							if (ccpkt) {
								free_packet(ccpkt);
								ccpkt = NULL;
							}
							printf(
									"handle mutilcast pkt error: unable to base64 encode");
							break;
						} else {

							unsigned char* temp_data = control_data =
									base64_out;
							int temp_len = base64_out_size;
							if (ccnl_ndntlv_dehead(&temp_data, &temp_len,
									(int*) &typ, &len) ||
									typ != COPSS_TLV_Control) {
								printf(
										"handle control pkt error: unable to dehead");
							} else {
								struct copss_pkt_s *copkt1 =
										copss_tlv_bytes2pkt(control_data,
												&temp_data, &temp_len);
								_handle_control(copkt1, cf);
								//                                    puts(" _handle_control(copkt1, cf); done");
								if (copkt1) {
									free_copss_packet(copkt1);
									copkt1 = NULL;
								}
								//                                    puts(" free copkt1 done");
							}
							if (base64_out) {
								free(base64_out);
								base64_out = NULL;
							}
							if (ccpkt) {
								free_packet(ccpkt);
								ccpkt = NULL;
							}
							//                                puts(" free base64_out ccpkt done");
						}
						break;
					}
					if (ccpkt) {
						free_packet(ccpkt);
						ccpkt = NULL;
					}

				case NDN_TLV_Data:
					//write to COPSS-CCNlite interface
					/* allocate memory */
					riot_pkt = gnrc_pktbuf_add(NULL, copss_buffer, res,
							GNRC_NETTYPE_CCN);
					//                        printf("copss main event : ccn pkt, without header size: %d ccn type: %d\n", riot_pkt->size, riot_pkt->type);
					hdr = gnrc_netif_hdr_build(
							cf->copss_ccnl_riot_f_conn.sll_addr,
							cf->copss_ccnl_riot_f_conn.sll_halen, NULL, 0);

					/* check if header building succeeded */
					if (hdr == NULL) {
						puts("error: packet buffer full");
						gnrc_pktbuf_release(riot_pkt);
						return NULL;
					}
					LL_PREPEND(riot_pkt, hdr);
					//                        printf("RIOT pkt total count:%d with header: size: %d type: %d\n\n", gnrc_pkt_count(riot_pkt), riot_pkt->size, riot_pkt->type);

					if (gnrc_netapi_receive(_copss_ccnl_riot_if_pid, riot_pkt)
							< 1) {
						puts(
								"error: cops unable to forward ccnl packet, discard it\n");
						gnrc_pktbuf_release(riot_pkt);
					}

					break;

				default:
					puts("default");
					break;
				}
			}
		}
	}
	return NULL;
}

/* generates and send out an interest */
int copss_send_interest(struct ccnl_relay_s* relay,
		struct ccnl_prefix_s *prefix, struct ccnl_face_s *from,
		unsigned char *buf, size_t buf_len) {

	if (_ccnl_suite != CCNL_SUITE_NDNTLV) {
		//DEBUGMSG(WARNING, "Suite not supported by COPSS!");
		return -1;
	}

	ccnl_mkInterestFunc mkInterest;
	ccnl_isContentFunc isContent;

	mkInterest = ccnl_suite2mkInterestFunc(_ccnl_suite);
	isContent = ccnl_suite2isContentFunc(_ccnl_suite);

	if (!mkInterest || !isContent) {
		// DEBUGMSG(WARNING, "No functions for this suite were found!");
		return -1;
	}

	// DEBUGMSG(INFO, "interest for chunk number: %u\n", (prefix->chunknum == NULL) ? 0 : *prefix->chunknum);

	int nonce = random_uint32();
	// DEBUGMSG(DEBUG, "nonce: %i\n", nonce);

	int len = mkInterest(prefix, &nonce, buf, buf_len);
	ccnl_ndntlv_forwarder(relay, from, &buf, &len);
	copss_ccnl_interest_remove(relay);
	if (prefix)
		free_prefix(prefix);
	return 0;
}

int _load_CD2RP_mapping(void) {
	//TODO: replace hard-code with reading mapping form a file
	int suite = CCNL_SUITE_NDNTLV;
	struct copss_contentNames_table_s *CD2RP =
			(struct copss_contentNames_table_s *) calloc(1,
					sizeof(struct copss_contentNames_table_s));
	if (!CD2RP) {
		puts("Error: CD2RP_table could not be created!");
		return -1;
	}
	char RP_uri1[] = "/RP";
	char CN_uri2[] = "/copss";
	struct ccnl_prefix_s *RP_name = ccnl_URItoPrefix(&RP_uri1[0], suite, NULL,
			NULL);
	struct copss_contentName_s * cn = (struct copss_contentName_s *) calloc(1,
			sizeof(struct copss_contentName_s));
	if (!RP_name || !cn) {
		puts("Error: no memory prefix or cn could not be created!");
		free(CD2RP);
		return -1;
	}
	struct ccnl_prefix_s *prefix = ccnl_URItoPrefix(&CN_uri2[0], suite, NULL,
			NULL);
	if (!prefix) {
		puts("Error: _load_CD2RP_mapping: ccnl_URItoPrefix!");
		free(cn);
		return -1;
	}
	cn->prefix = prefix;
	CD2RP->prefix = RP_name;
	CD2RP->content_name = cn;
	DBL_LINKED_LIST_ADD(copss.CD2RP_table, CD2RP);

	return 0;
}

kernel_pid_t copss_start(uint16_t port) {
	copss.port = port;
	ccnl_core_init();
	//kernel_pid_t ccnl_riot_pit =
	ccnl_riot_pid = ccnl_start();
	//ccnl_relay.max_cache_entries = 0;
	_copss_ccnl_riot_if_pid = thread_create(copss_if_stack,
			sizeof(copss_if_stack),
			THREAD_PRIORITY_MAIN - 3,
			THREAD_CREATE_STACKTEST, _copss_ccnl_riot_if_event_loop, &copss,
			"copss_ccnl_riot_if");

	if (copss_ccnl_open_if(_copss_ccnl_riot_if_pid, &copss) < 0) {
		puts("Error registering at network interface!");
		return -1;
	}

	//    printf("add cust interface:%d to ccnl \n", _copss_ccnl_riot_if_pid);

	/* register for this nettype */
	gnrc_netreg_entry_init_pid(&copss_ccnl_ne, GNRC_NETREG_DEMUX_CTX_ALL,
			ccnl_riot_pid);
	gnrc_netreg_register(GNRC_NETTYPE_CCN, &copss_ccnl_ne);

	/* start the COPSS event-loop */
	_copss_event_loop_pid = thread_create(copss_stack, sizeof(copss_stack),
	THREAD_PRIORITY_MAIN - 2,
	THREAD_CREATE_STACKTEST, _copss_event_loop, &copss, "copssd");
	int i = _load_CD2RP_mapping();
	if (i != 0) {
		printf("error load CD2RP table%d\n", i);
	}

	return _copss_event_loop_pid;
}

