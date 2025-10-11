#include "lwip/opt.h"
#include "lwip/debug.h"
#include "lwip/stats.h"
#include "lwip/tcp.h"
#include "string.h"
#include "DoIP.h"
#include "Ultrasonic.h"
#include "evadc.h"

#if LWIP_TCP

static struct tcp_pcb *doip_pcb;

enum doip_states
{
	ES_NONE = 0,
	ES_ACCEPTED,
	ES_RECEIVED,
	ES_CLOSING
};

struct doip_state
{
	u8_t state;
	u8_t retries;
	struct tcp_pcb *pcb;
	/* pbuf (chain) to recycle */
	struct pbuf *p;
};

err_t doip_accept(void *arg, struct tcp_pcb *newpcb, err_t err);
err_t doip_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);
void doip_error(void *arg, err_t err);
err_t doip_poll(void *arg, struct tcp_pcb *tpcb);
err_t doip_sent(void *arg, struct tcp_pcb *tpcb, u16_t len);
void doip_send(struct tcp_pcb *tpcb, struct doip_state *es);
void doip_close(struct tcp_pcb *tpcb, struct doip_state *es);

#define UDS_PORT 13400

void DoIP_Init(void)
{
	doip_pcb = tcp_new();
	if (doip_pcb != NULL) {
		err_t err;

		err = tcp_bind(doip_pcb, IP_ADDR_ANY, UDS_PORT);
		if (err == ERR_OK) {
			doip_pcb = tcp_listen(doip_pcb);
			tcp_accept(doip_pcb, doip_accept);
		} else {
			/* abort? output diagnostic? */
		}
	} else {
		/* abort? output diagnostic? */
	}
}

err_t doip_accept(void *arg, struct tcp_pcb *newpcb, err_t err)
{
	err_t ret_err;
	struct doip_state *es;

	LWIP_UNUSED_ARG(arg);
	LWIP_UNUSED_ARG(err);
//	LWIP_DEBUGF(ECHO_DEBUG, ("doip_accept\n"));

	/* Unless this pcb should have NORMAL priority, set its priority now.
	 When running out of pcbs, low priority pcbs can be aborted to create
	 new pcbs of higher priority. */
	tcp_setprio(newpcb, TCP_PRIO_MIN);

	es = (struct doip_state*) mem_malloc(sizeof(struct doip_state));
	if (es != NULL) {
		es->state = ES_ACCEPTED;
		es->pcb = newpcb;
		es->retries = 0;
		es->p = NULL;
		/* pass newly allocated es to our callbacks */
		tcp_arg(newpcb, es);
		tcp_recv(newpcb, doip_recv);
		tcp_err(newpcb, doip_error);
		tcp_poll(newpcb, doip_poll, 0);
		ret_err = ERR_OK;
		es->state = ES_RECEIVED;
	} else {
		ret_err = ERR_MEM;
	}
	return ret_err;
}

err_t doip_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
    struct doip_state *es;
    err_t ret_err = ERR_OK;

    LWIP_ASSERT("arg != NULL", arg != NULL);
    es = (struct doip_state*) arg;

    if (p == NULL) {
        // 연결 종료 처리
        es->state = ES_CLOSING;
        doip_close(tpcb, es);
        return ERR_OK;
    } else if (err != ERR_OK) {
        // 오류 발생 시 버퍼 해제
        if (p != NULL) { pbuf_free(p); }
        return err;
    }

    // 1. DoIP 페이로드 타입 확인
    uint16_t payload_type = (*(((uint8*)p->payload) + 2) << 8) | *(((uint8*)p->payload) + 3);

    // 2. 페이로드 타입에 따라 분기하여 처리
    if (payload_type == 0x0005) // Routing Activation Request
    {
        // 라우팅 활성화 긍정 응답 메시지 생성
        uint8 resp_payload[] = {
            0x02, 0xFD, 0x00, 0x06, 0x00, 0x00, 0x00, 0x05, // DoIP Header
            0x0E, 0x80, 0x02, 0x01, 0x10,                 // Payload (Code: 0x10 = Success)
        };

        // 응답 전송
        tcp_write(tpcb, resp_payload, sizeof(resp_payload), 1);
        tcp_output(tpcb);
    }
    else if (payload_type == 0x8001) // UDS Message
    {
        uint8 service_id = *(((uint8*)p->payload) + 8);
        uint16 data_id = (*(((uint8*)p->payload) + 9) << 8) | *(((uint8*)p->payload) + 10);

        if (service_id == 0x22 && data_id == 0x0002)
        {
            uint16_t adc_val = (uint16_t)Evadc_readPR();

            // UDS 긍정 응답 메시지 생성
            uint8 resp_payload[13] = {
                0x02, 0xFD, 0x80, 0x02, 0x00, 0x00, 0x00, 0x05, // DoIP Header
                0x62, (data_id >> 8) & 0xFF, data_id & 0xFF,
                (adc_val >> 8) & 0xFF, adc_val & 0xFF
            };

            // 응답 전송
            tcp_write(tpcb, resp_payload, sizeof(resp_payload), 1);
            tcp_output(tpcb);
        }
    }

    pbuf_free(p); // 수신된 패킷은 처리 후 항상 해제
    return ERR_OK;
}

void doip_error(void *arg, err_t err)
{
	struct doip_state *es;
//	LWIP_DEBUGF(ECHO_DEBUG, ("doip_error\n"));

	LWIP_UNUSED_ARG(err);

	es = (struct doip_state*) arg;
	if (es != NULL) {
		mem_free(es);
	}
}

err_t doip_poll(void *arg, struct tcp_pcb *tpcb)
{
	err_t ret_err;
	struct doip_state *es;
//	LWIP_DEBUGF(ECHO_DEBUG, ("doip_poll\n"));

	es = (struct doip_state*) arg;
	if (es != NULL) {
		if (es->p != NULL) {
			/* there is a remaining pbuf (chain)  */
			tcp_sent(tpcb, doip_sent);
			doip_send(tpcb, es);
		} else {
			/* no remaining pbuf (chain)  */
			if (es->state == ES_CLOSING) {
				doip_close(tpcb, es);
			}
		}
		ret_err = ERR_OK;
	} else {
		/* nothing to be done */
		tcp_abort(tpcb);
		ret_err = ERR_ABRT;
	}
	return ret_err;
}

err_t doip_sent(void *arg, struct tcp_pcb *tpcb, u16_t len)
{
	struct doip_state *es;

	LWIP_UNUSED_ARG(len);
//	LWIP_DEBUGF(ECHO_DEBUG, ("doip_sent\n"));

	es = (struct doip_state*) arg;
	es->retries = 0;

	if (es->p != NULL) {
		/* still got pbufs to send */
		tcp_sent(tpcb, doip_sent);
		doip_send(tpcb, es);
	} else {
		/* no more pbufs to send */
		if (es->state == ES_CLOSING) {
			doip_close(tpcb, es);
		}
	}
	return ERR_OK;
}

void doip_send(struct tcp_pcb *tpcb, struct doip_state *es)
{
	struct pbuf *ptr;
	err_t wr_err = ERR_OK;
//	LWIP_DEBUGF(ECHO_DEBUG, ("doip_send\n"));

	while ((wr_err == ERR_OK) && (es->p != NULL)
			&& (es->p->len <= tcp_sndbuf(tpcb))) {
		ptr = es->p;

		/* enqueue data for transmission */
		wr_err = tcp_write(tpcb, ptr->payload, ptr->len, 1);
		if (wr_err == ERR_OK) {
			u16_t plen;
			u8_t freed;

			plen = ptr->len;
			/* continue with next pbuf in chain (if any) */
			es->p = ptr->next;
			if (es->p != NULL) {
				/* new reference! */
				pbuf_ref(es->p);
			}
			/* chop first pbuf from chain */
			do {
				/* try hard to free pbuf */
				freed = pbuf_free(ptr);
			} while (freed == 0);
			/* we can read more data now */
			tcp_recved(tpcb, plen);
		} else if (wr_err == ERR_MEM) {
			/* we are low on memory, try later / harder, defer to poll */
			es->p = ptr;
		} else {
			/* other problem ?? */
		}
	}
}

void doip_close(struct tcp_pcb *tpcb, struct doip_state *es)
{
	tcp_arg(tpcb, NULL);
	tcp_sent(tpcb, NULL);
	tcp_recv(tpcb, NULL);
	tcp_err(tpcb, NULL);
	tcp_poll(tpcb, NULL, 0);
//	LWIP_DEBUGF(ECHO_DEBUG, ("doip_close\n"));

	if (es != NULL) {
		mem_free(es);
	}
	tcp_close(tpcb);
}

#endif /* LWIP_TCP */
